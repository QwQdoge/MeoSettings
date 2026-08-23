#include "bluetoothbackend.h"

#include <BluezQt/Adapter>
#include <BluezQt/Battery>
#include <BluezQt/Device>
#include <BluezQt/InitManagerJob>
#include <BluezQt/PendingCall>

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <algorithm>
#include <memory>

namespace {

constexpr auto bluezService = "org.bluez";
constexpr auto agentManagerPath = "/org/bluez";
constexpr auto agentManagerInterface = "org.bluez.AgentManager1";
constexpr auto keyboardDisplayCapability = "KeyboardDisplay";
constexpr auto pairingTimeoutMs = 120000;

bool isBenignDisconnectError(const BluezQt::PendingCall *call)
{
    return call && (call->error() == BluezQt::PendingCall::NoError
                    || call->error() == BluezQt::PendingCall::NotConnected);
}

bool isBenignConnectError(const BluezQt::PendingCall *call)
{
    return call && (call->error() == BluezQt::PendingCall::NoError
                    || call->error() == BluezQt::PendingCall::AlreadyConnected);
}

bool isAlreadyRegisteredError(const QDBusError &error)
{
    return error.name() == QStringLiteral("org.bluez.Error.AlreadyExists");
}

} // namespace

BluetoothBackend::BluetoothBackend(QObject *parent)
    : BackendBase(parent)
    , m_manager(this)
    , m_pairingAgent(this)
{
    m_pairingTimeout.setSingleShot(true);
    m_pairingTimeout.setInterval(pairingTimeoutMs);

    connect(&m_manager, &BluezQt::Manager::operationalChanged, this, [this] {
        connectAdapterSignals();
        if (!m_manager.isOperational() && m_pairing) {
            finishPairing(m_pairingGeneration, tr("The BlueZ service became unavailable during pairing."));
        }
        publishChanged();
    });
    connect(&m_manager, &BluezQt::Manager::usableAdapterChanged, this, [this] {
        connectAdapterSignals();
        publishChanged();
    });
    connect(&m_manager, &BluezQt::Manager::bluetoothBlockedChanged, this,
            [this](bool) { publishChanged(); });
    connect(&m_manager, &BluezQt::Manager::adapterAdded, this, [this](const BluezQt::AdapterPtr &) {
        connectAdapterSignals();
        publishChanged();
    });
    connect(&m_manager, &BluezQt::Manager::adapterRemoved, this, [this](const BluezQt::AdapterPtr &adapter) {
        if (adapter && adapter->ubi() == m_activeAdapterUbi) {
            m_activeAdapterUbi.clear();
        }
        connectAdapterSignals();
        publishChanged();
    });
    connect(&m_manager, &BluezQt::Manager::deviceAdded, this,
            [this](const BluezQt::DevicePtr &) { publishChanged(); });
    connect(&m_manager, &BluezQt::Manager::deviceChanged, this,
            [this](const BluezQt::DevicePtr &) { publishChanged(); });
    connect(&m_manager, &BluezQt::Manager::deviceRemoved, this,
            [this](const BluezQt::DevicePtr &device) {
                if (m_pairing && device && device->ubi() == m_pairingDeviceUbi) {
                    finishPairing(m_pairingGeneration, tr("The Bluetooth device disappeared during pairing."));
                }
                publishChanged();
            });

    connect(&m_pairingAgent, &BluetoothPairingAgent::authenticationChanged, this,
            [this] { Q_EMIT authenticationChanged(); });
    connect(&m_pairingAgent, &BluetoothPairingAgent::released, this, [this] {
        m_pairingAgentRegistered = false;
        m_pairingAgentRegistrationInProgress = false;
        m_pairingAgentUnregistrationInProgress = false;
        Q_EMIT pairingAgentChanged();
        if (m_pairing) {
            finishPairing(m_pairingGeneration, tr("BlueZ released the pairing agent before pairing completed."));
        }
    });
    connect(&m_pairingAgent, &BluetoothPairingAgent::unexpectedAuthentication, this,
            [this](const QString &) {
                if (!m_pairing) {
                    return;
                }
                const auto generation = m_pairingGeneration;
                setError(tr("BlueZ requested authentication for a different device; pairing was canceled."));
                m_pairingCancellationRequested = true;
                const auto device = m_manager.deviceForUbi(m_pairingDeviceUbi);
                m_pairingAgent.cancelAuthentication();
                if (!device) {
                    finishPairing(generation, error(), true);
                    return;
                }
                auto *call = device->cancelPairing();
                connect(call, &BluezQt::PendingCall::finished, this,
                        [this, generation](BluezQt::PendingCall *) {
                            finishPairing(generation, error(), true);
                        });
            });
    connect(&m_pairingTimeout, &QTimer::timeout, this, [this] {
        if (!m_pairing) {
            return;
        }
        const auto generation = m_pairingGeneration;
        const auto timeoutError = tr("Pairing timed out. Put the device in pairing mode and try again.");
        setError(timeoutError);
        m_pairingCancellationRequested = true;
        m_pairingAgent.cancelAuthentication();
        const auto device = m_manager.deviceForUbi(m_pairingDeviceUbi);
        if (!device) {
            finishPairing(generation, timeoutError, true);
            return;
        }
        auto *call = device->cancelPairing();
        connect(call, &BluezQt::PendingCall::finished, this,
                [this, generation, timeoutError](BluezQt::PendingCall *) {
                    finishPairing(generation, timeoutError, true);
                });
    });

    auto bus = QDBusConnection::systemBus();
    if (bus.isConnected()) {
        m_pairingAgentObjectExported = bus.registerObject(BluetoothPairingAgent::objectPath().path(),
                                                           &m_pairingAgent,
                                                           QDBusConnection::ExportAllSlots);
    }
    Q_EMIT pairingAgentChanged();

    auto *init = m_manager.init();
    connect(init, &BluezQt::InitManagerJob::result, this, [this] {
        connectAdapterSignals();
        publishChanged();
    });
    // BluezQt intentionally requires this explicit start. Without it a live
    // BlueZ adapter is permanently reported as unavailable.
    init->start();
}

BluetoothBackend::~BluetoothBackend()
{
    m_pairingTimeout.stop();
    m_pairingAgent.cancelAuthentication();
    if (m_pairingAgentObjectExported) {
        QDBusConnection::systemBus().unregisterObject(BluetoothPairingAgent::objectPath().path());
    }
}

bool BluetoothBackend::enabled() const
{
    return !m_manager.usableAdapter().isNull();
}

bool BluetoothBackend::discovering() const
{
    const auto adapter = activeAdapter();
    return adapter && adapter->isDiscovering();
}

QVariantList BluetoothBackend::adapters() const
{
    QVariantList result;
    const auto knownAdapters = m_manager.adapters();
    result.reserve(knownAdapters.size());
    for (const auto &adapter : knownAdapters) {
        if (!adapter) {
            continue;
        }
        result.push_back(QVariantMap{
            {QStringLiteral("ubi"), adapter->ubi()},
            {QStringLiteral("address"), adapter->address()},
            {QStringLiteral("name"), adapter->name().isEmpty() ? adapter->address() : adapter->name()},
            {QStringLiteral("powered"), adapter->isPowered()},
            {QStringLiteral("discoverable"), adapter->isDiscoverable()},
            {QStringLiteral("discoverableTimeout"), static_cast<int>(adapter->discoverableTimeout())},
            {QStringLiteral("pairable"), adapter->isPairable()},
            {QStringLiteral("pairableTimeout"), static_cast<int>(adapter->pairableTimeout())},
            {QStringLiteral("discovering"), adapter->isDiscovering()},
            {QStringLiteral("selected"), adapter->ubi() == activeAdapterUbi()},
        });
    }
    std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        const auto a = left.toMap();
        const auto b = right.toMap();
        if (a.value(QStringLiteral("selected")).toBool() != b.value(QStringLiteral("selected")).toBool()) {
            return a.value(QStringLiteral("selected")).toBool();
        }
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });
    return result;
}

QString BluetoothBackend::activeAdapterUbi() const
{
    if (!m_activeAdapterUbi.isEmpty()) {
        return m_activeAdapterUbi;
    }
    const auto adapter = activeAdapter();
    return adapter ? adapter->ubi() : QString{};
}

QVariantList BluetoothBackend::devices() const
{
    QVariantList result;
    const auto knownDevices = m_manager.devices();
    result.reserve(knownDevices.size());
    for (const auto &device : knownDevices) {
        if (!device) {
            continue;
        }
        const auto battery = device->battery();
        const auto adapter = device->adapter();
        result.push_back(QVariantMap{
            {QStringLiteral("ubi"), device->ubi()},
            {QStringLiteral("adapterUbi"), adapter ? adapter->ubi() : QString{}},
            {QStringLiteral("name"), device->friendlyName().isEmpty() ? device->address() : device->friendlyName()},
            {QStringLiteral("address"), device->address()},
            {QStringLiteral("icon"), materialIcon(device)},
            {QStringLiteral("type"), BluezQt::Device::typeToString(device->type())},
            {QStringLiteral("paired"), device->isPaired()},
            {QStringLiteral("connected"), device->isConnected()},
            {QStringLiteral("trusted"), device->isTrusted()},
            {QStringLiteral("blocked"), device->isBlocked()},
            {QStringLiteral("legacyPairing"), device->hasLegacyPairing()},
            {QStringLiteral("servicesResolved"), device->isServicesResolved()},
            {QStringLiteral("rssi"), static_cast<int>(device->rssi())},
            {QStringLiteral("batteryAvailable"), !battery.isNull()},
            {QStringLiteral("batteryPercent"), battery ? battery->percentage() : -1},
            {QStringLiteral("pairing"), m_pairing && device->ubi() == m_pairingDeviceUbi},
        });
    }
    std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        const auto a = left.toMap();
        const auto b = right.toMap();
        if (a.value(QStringLiteral("blocked")).toBool() != b.value(QStringLiteral("blocked")).toBool()) {
            return !a.value(QStringLiteral("blocked")).toBool();
        }
        if (a.value(QStringLiteral("connected")).toBool() != b.value(QStringLiteral("connected")).toBool()) {
            return a.value(QStringLiteral("connected")).toBool();
        }
        if (a.value(QStringLiteral("paired")).toBool() != b.value(QStringLiteral("paired")).toBool()) {
            return a.value(QStringLiteral("paired")).toBool();
        }
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });
    return result;
}

bool BluetoothBackend::rfkillBlocked() const
{
    return m_manager.isBluetoothBlocked();
}

bool BluetoothBackend::pairing() const
{
    return m_pairing;
}

QString BluetoothBackend::pairingDeviceUbi() const
{
    return m_pairingDeviceUbi;
}

QString BluetoothBackend::pairingDeviceName() const
{
    return m_pairingDeviceName;
}

bool BluetoothBackend::pairingAgentReady() const
{
    return m_pairingAgentObjectExported && QDBusConnection::systemBus().isConnected();
}

QVariantMap BluetoothBackend::authentication() const
{
    return m_pairingAgent.authentication();
}

bool BluetoothBackend::authenticationPending() const
{
    return m_pairingAgent.hasAuthentication();
}

void BluetoothBackend::setEnabled(const bool requestedEnabled)
{
    clearError();
    const auto knownAdapters = m_manager.adapters();
    if (knownAdapters.isEmpty()) {
        if (requestedEnabled) {
            setError(tr("No Bluetooth adapter is available."));
        }
        return;
    }
    if (!beginOperation(tr("changing Bluetooth power"))) {
        return;
    }

    auto pending = std::make_shared<int>(0);
    for (const auto &adapter : knownAdapters) {
        if (adapter && adapter->isPowered() != requestedEnabled) {
            ++*pending;
        }
    }
    if (*pending == 0) {
        finishOperation();
        return;
    }
    for (const auto &adapter : knownAdapters) {
        if (!adapter || adapter->isPowered() == requestedEnabled) {
            continue;
        }
        auto *call = adapter->setPowered(requestedEnabled);
        connect(call, &BluezQt::PendingCall::finished, this,
                [this, pending](BluezQt::PendingCall *finished) {
                    if (finished->error() != BluezQt::PendingCall::NoError) {
                        setError(finished->errorText());
                    }
                    if (--*pending == 0) {
                        finishOperation();
                    }
                });
    }
}

void BluetoothBackend::setActiveAdapterUbi(const QString &ubi)
{
    clearError();
    if (ubi.isEmpty()) {
        if (!m_activeAdapterUbi.isEmpty()) {
            m_activeAdapterUbi.clear();
            publishChanged();
        }
        return;
    }
    const auto adapter = m_manager.adapterForUbi(ubi);
    if (!adapter) {
        setError(tr("The selected Bluetooth adapter is no longer available."));
        return;
    }
    if (m_activeAdapterUbi != adapter->ubi()) {
        m_activeAdapterUbi = adapter->ubi();
        publishChanged();
    }
}

void BluetoothBackend::startDiscovery()
{
    clearError();
    const auto adapter = adapterForAction({}, tr("starting discovery"), true);
    if (!adapter || adapter->isDiscovering() || !beginOperation(tr("starting discovery"))) {
        return;
    }
    auto *call = adapter->startDiscovery();
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::stopDiscovery()
{
    clearError();
    const auto adapter = adapterForAction({}, tr("stopping discovery"));
    if (!adapter || !adapter->isDiscovering() || !beginOperation(tr("stopping discovery"))) {
        return;
    }
    auto *call = adapter->stopDiscovery();
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setAdapterPowered(const QString &adapterUbi, const bool powered)
{
    clearError();
    const auto adapter = adapterForAction(adapterUbi, tr("changing adapter power"));
    if (!adapter || adapter->isPowered() == powered || !beginOperation(tr("changing adapter power"))) {
        return;
    }
    auto *call = adapter->setPowered(powered);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setAdapterDiscoverable(const QString &adapterUbi, const bool discoverable)
{
    clearError();
    const auto adapter = adapterForAction(adapterUbi, tr("changing discoverability"), true);
    if (!adapter || adapter->isDiscoverable() == discoverable || !beginOperation(tr("changing discoverability"))) {
        return;
    }
    auto *call = adapter->setDiscoverable(discoverable);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setAdapterPairable(const QString &adapterUbi, const bool pairable)
{
    clearError();
    const auto adapter = adapterForAction(adapterUbi, tr("changing pairability"), true);
    if (!adapter || adapter->isPairable() == pairable || !beginOperation(tr("changing pairability"))) {
        return;
    }
    auto *call = adapter->setPairable(pairable);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setAdapterDiscoverableTimeout(const QString &adapterUbi, const int seconds)
{
    clearError();
    if (seconds < 0 || seconds > 86400) {
        setError(tr("Discoverable timeout must be between 0 and 86400 seconds."));
        return;
    }
    const auto adapter = adapterForAction(adapterUbi, tr("changing discoverable timeout"), true);
    if (!adapter || adapter->discoverableTimeout() == static_cast<quint32>(seconds)
        || !beginOperation(tr("changing discoverable timeout"))) {
        return;
    }
    auto *call = adapter->setDiscoverableTimeout(static_cast<quint32>(seconds));
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setAdapterPairableTimeout(const QString &adapterUbi, const int seconds)
{
    clearError();
    if (seconds < 0 || seconds > 86400) {
        setError(tr("Pairable timeout must be between 0 and 86400 seconds."));
        return;
    }
    const auto adapter = adapterForAction(adapterUbi, tr("changing pairable timeout"), true);
    if (!adapter || adapter->pairableTimeout() == static_cast<quint32>(seconds)
        || !beginOperation(tr("changing pairable timeout"))) {
        return;
    }
    auto *call = adapter->setPairableTimeout(static_cast<quint32>(seconds));
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::pairDevice(const QString &ubi)
{
    clearError();
    if (m_pairing || busy()) {
        setError(tr("Finish the current Bluetooth operation before starting another pairing."));
        return;
    }
    if (m_pairingAgentUnregistrationInProgress) {
        setError(tr("The previous pairing agent is closing. Try pairing again in a moment."));
        return;
    }
    if (!pairingAgentReady()) {
        setError(tr("The private Bluetooth authentication agent is unavailable."));
        return;
    }
    const auto device = deviceForAction(ubi, tr("pairing"));
    if (!device) {
        return;
    }
    if (device->isPaired()) {
        setError(tr("This device is already paired."));
        return;
    }
    if (device->isBlocked()) {
        setError(tr("Unblock this device before pairing it."));
        return;
    }

    ++m_pairingGeneration;
    const auto generation = m_pairingGeneration;
    m_pairingCancellationRequested = false;
    setBusy(true);
    setPairingState(true, device);
    m_pairingAgent.beginPairing(device->ubi(), device->address(),
                                device->friendlyName().isEmpty() ? device->address() : device->friendlyName());
    m_pairingTimeout.start();
    ensurePairingAgent(device, generation);
}

void BluetoothBackend::cancelPairing()
{
    if (!m_pairing) {
        m_pairingAgent.cancelAuthentication();
        Q_EMIT authenticationChanged();
        return;
    }
    const auto generation = m_pairingGeneration;
    m_pairingCancellationRequested = true;
    m_pairingAgent.cancelAuthentication();
    Q_EMIT authenticationChanged();
    const auto device = m_manager.deviceForUbi(m_pairingDeviceUbi);
    if (!device) {
        finishPairing(generation, tr("Pairing was canceled."), true);
        return;
    }
    auto *call = device->cancelPairing();
    connect(call, &BluezQt::PendingCall::finished, this, [this, generation](BluezQt::PendingCall *) {
        finishPairing(generation, tr("Pairing was canceled."), true);
    });
}

void BluetoothBackend::connectDevice(const QString &ubi)
{
    clearError();
    const auto device = deviceForAction(ubi, tr("connecting"));
    if (!device) {
        return;
    }
    if (!device->isPaired()) {
        setError(tr("Pair this device before connecting it."));
        return;
    }
    if (device->isBlocked()) {
        setError(tr("Unblock this device before connecting it."));
        return;
    }
    if (device->isConnected()) {
        publishChanged();
        return;
    }
    if (!beginOperation(tr("connecting"))) {
        return;
    }
    auto *call = device->connectToDevice();
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (!isBenignConnectError(finished)) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::disconnectDevice(const QString &ubi)
{
    clearError();
    const auto device = deviceForAction(ubi, tr("disconnecting"), false);
    if (!device) {
        return;
    }
    if (!device->isConnected()) {
        publishChanged();
        return;
    }
    if (!beginOperation(tr("disconnecting"))) {
        return;
    }
    auto *call = device->disconnectFromDevice();
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (!isBenignDisconnectError(finished)) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setDeviceTrusted(const QString &ubi, const bool trusted)
{
    clearError();
    const auto device = deviceForAction(ubi, tr("changing trust"), false);
    if (!device) {
        return;
    }
    if (!device->isPaired()) {
        setError(tr("Pair this device before changing its trust setting."));
        return;
    }
    if (device->isTrusted() == trusted) {
        publishChanged();
        return;
    }
    if (!beginOperation(tr("changing trust"))) {
        return;
    }
    auto *call = device->setTrusted(trusted);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::setDeviceBlocked(const QString &ubi, const bool blocked)
{
    clearError();
    const auto device = deviceForAction(ubi, tr("changing the block setting"), false);
    if (!device) {
        return;
    }
    if (device->isBlocked() == blocked) {
        publishChanged();
        return;
    }
    if (!beginOperation(tr("changing the block setting"))) {
        return;
    }
    auto *call = device->setBlocked(blocked);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::renameDevice(const QString &ubi, const QString &name)
{
    clearError();
    const auto trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || trimmedName.size() > 248) {
        setError(tr("Device name must contain 1 to 248 characters."));
        return;
    }
    if (std::any_of(trimmedName.cbegin(), trimmedName.cend(), [](const QChar character) {
            const auto codePoint = character.unicode();
            return codePoint < 0x20 || codePoint == 0x7f;
        })) {
        setError(tr("Device name cannot contain control characters."));
        return;
    }
    const auto device = deviceForAction(ubi, tr("renaming"), false);
    if (!device) {
        return;
    }
    if (device->name() == trimmedName) {
        publishChanged();
        return;
    }
    if (!beginOperation(tr("renaming"))) {
        return;
    }
    auto *call = device->setName(trimmedName);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::unpairDevice(const QString &ubi)
{
    clearError();
    const auto device = deviceForAction(ubi, tr("removing a device"), false);
    if (!device) {
        return;
    }
    if (!device->isPaired()) {
        setError(tr("Only paired devices can be removed."));
        return;
    }
    const auto adapter = device->adapter();
    if (!adapter) {
        setError(tr("The Bluetooth adapter for this device is unavailable."));
        return;
    }
    if (!beginOperation(tr("removing a device"))) {
        return;
    }
    auto *call = adapter->removeDevice(device);
    connect(call, &BluezQt::PendingCall::finished, this, [this](BluezQt::PendingCall *finished) {
        if (finished->error() != BluezQt::PendingCall::NoError) {
            setError(finished->errorText());
        }
        finishOperation();
    });
}

void BluetoothBackend::submitPin(const QString &pin)
{
    QString validationError;
    if (!m_pairingAgent.submitPin(pin, &validationError)) {
        setError(validationError);
        return;
    }
    clearError();
    Q_EMIT authenticationChanged();
}

void BluetoothBackend::submitPasskey(const QString &passkey)
{
    QString validationError;
    if (!m_pairingAgent.submitPasskey(passkey, &validationError)) {
        setError(validationError);
        return;
    }
    clearError();
    Q_EMIT authenticationChanged();
}

void BluetoothBackend::respondToAuthentication(const bool accepted)
{
    QString validationError;
    if (!m_pairingAgent.respondToAuthentication(accepted, &validationError)) {
        setError(validationError);
        return;
    }
    clearError();
    Q_EMIT authenticationChanged();
}

void BluetoothBackend::toggleDevice(const QString &address)
{
    clearError();
    const auto device = deviceForLegacyAddress(address, tr("changing this device"));
    if (!device) {
        return;
    }
    if (device->isConnected()) {
        disconnectDevice(device->ubi());
    } else {
        connectDevice(device->ubi());
    }
}

void BluetoothBackend::forgetDevice(const QString &address)
{
    clearError();
    const auto device = deviceForLegacyAddress(address, tr("removing this device"));
    if (!device) {
        return;
    }
    unpairDevice(device->ubi());
}

bool BluetoothBackend::beginOperation(const QString &description)
{
    if (m_pairing || busy()) {
        setError(tr("Finish the current Bluetooth operation before %1.").arg(description));
        return false;
    }
    setBusy(true);
    return true;
}

void BluetoothBackend::finishOperation()
{
    if (!m_pairing) {
        setBusy(false);
    }
    publishChanged();
}

BluezQt::AdapterPtr BluetoothBackend::activeAdapter() const
{
    if (!m_activeAdapterUbi.isEmpty()) {
        const auto selected = m_manager.adapterForUbi(m_activeAdapterUbi);
        if (selected) {
            return selected;
        }
    }
    const auto powered = m_manager.usableAdapter();
    if (powered) {
        return powered;
    }
    const auto knownAdapters = m_manager.adapters();
    for (const auto &adapter : knownAdapters) {
        if (adapter) {
            return adapter;
        }
    }
    return {};
}

BluezQt::AdapterPtr BluetoothBackend::adapterForAction(const QString &ubi, const QString &operation,
                                                        const bool requirePowered)
{
    const auto adapter = ubi.isEmpty() ? activeAdapter() : m_manager.adapterForUbi(ubi);
    if (!adapter) {
        setError(tr("The Bluetooth adapter is no longer available for %1.").arg(operation));
        return {};
    }
    if (requirePowered && !adapter->isPowered()) {
        setError(tr("Turn on the selected Bluetooth adapter before %1.").arg(operation));
        return {};
    }
    return adapter;
}

BluezQt::DevicePtr BluetoothBackend::deviceForAction(const QString &ubi, const QString &operation,
                                                      const bool requirePoweredAdapter)
{
    const auto device = m_manager.deviceForUbi(ubi);
    if (!device) {
        setError(tr("The Bluetooth device is no longer available for %1.").arg(operation));
        return {};
    }
    const auto adapter = device->adapter();
    if (!adapter) {
        setError(tr("The Bluetooth adapter for this device is unavailable."));
        return {};
    }
    if (requirePoweredAdapter && !adapter->isPowered()) {
        setError(tr("Turn on this device's Bluetooth adapter before %1.").arg(operation));
        return {};
    }
    return device;
}

BluezQt::DevicePtr BluetoothBackend::deviceForLegacyAddress(const QString &address, const QString &operation)
{
    BluezQt::DevicePtr result;
    for (const auto &device : m_manager.devices()) {
        if (!device || device->address().compare(address, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (result) {
            setError(tr("More than one Bluetooth adapter exposes this address; open the device from Meo Settings."));
            return {};
        }
        result = device;
    }
    if (!result) {
        setError(tr("The Bluetooth device is no longer available for %1.").arg(operation));
    }
    return result;
}

void BluetoothBackend::ensurePairingAgent(const BluezQt::DevicePtr &device, const quint64 generation)
{
    if (!m_pairing || generation != m_pairingGeneration) {
        return;
    }
    if (m_pairingAgentUnregistrationInProgress) {
        finishPairing(generation, tr("The previous pairing agent is still closing. Try again in a moment."));
        return;
    }
    if (m_pairingAgentRegistered) {
        startPairing(device, generation);
        return;
    }
    if (m_pairingAgentRegistrationInProgress) {
        return;
    }
    const auto bus = QDBusConnection::systemBus();
    if (!m_pairingAgentObjectExported || !bus.isConnected()) {
        finishPairing(generation, tr("The private Bluetooth authentication agent is unavailable."));
        return;
    }
    QDBusInterface manager(QString::fromLatin1(bluezService), QString::fromLatin1(agentManagerPath),
                            QString::fromLatin1(agentManagerInterface), bus);
    if (!manager.isValid()) {
        finishPairing(generation, tr("BlueZ does not expose its pairing-agent manager."));
        return;
    }

    m_pairingAgentRegistrationInProgress = true;
    Q_EMIT pairingAgentChanged();
    const auto call = manager.asyncCall(QStringLiteral("RegisterAgent"),
                                        QVariant::fromValue(BluetoothPairingAgent::objectPath()),
                                        QString::fromLatin1(keyboardDisplayCapability));
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, device, generation](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<> reply = *watcher;
                watcher->deleteLater();
                m_pairingAgentRegistrationInProgress = false;
                if (!m_pairing || generation != m_pairingGeneration) {
                    Q_EMIT pairingAgentChanged();
                    return;
                }
                if (reply.isError() && !isAlreadyRegisteredError(reply.error())) {
                    Q_EMIT pairingAgentChanged();
                    finishPairing(generation, tr("Could not register the Meo pairing agent: %1")
                                                 .arg(reply.error().message()));
                    return;
                }
                m_pairingAgentRegistered = true;
                Q_EMIT pairingAgentChanged();
                startPairing(device, generation);
            });
}

void BluetoothBackend::startPairing(const BluezQt::DevicePtr &device, const quint64 generation)
{
    if (!m_pairing || generation != m_pairingGeneration || !device) {
        return;
    }
    auto *call = device->pair();
    connect(call, &BluezQt::PendingCall::finished, this,
            [this, generation](BluezQt::PendingCall *finished) {
                if (generation != m_pairingGeneration || !m_pairing) {
                    return;
                }
                if (finished->error() != BluezQt::PendingCall::NoError
                    && finished->error() != BluezQt::PendingCall::AlreadyExists) {
                    const auto message = m_pairingCancellationRequested
                        ? tr("Pairing was canceled.")
                        : finished->errorText();
                    finishPairing(generation, message, m_pairingCancellationRequested);
                    return;
                }
                // Pairing does not grant trust or trigger a connection. Both
                // remain explicit user choices after BlueZ reports success.
                finishPairing(generation);
            });
}

void BluetoothBackend::finishPairing(const quint64 generation, const QString &pairingError,
                                     const bool canceled)
{
    if (!m_pairing || generation != m_pairingGeneration) {
        return;
    }
    m_pairingTimeout.stop();
    m_pairingAgent.cancelAuthentication();
    m_pairingAgent.endPairing();
    setPairingState(false);
    m_pairingCancellationRequested = false;
    if (!pairingError.isEmpty()) {
        setError(pairingError);
    } else if (canceled) {
        setError(tr("Pairing was canceled."));
    } else {
        clearError();
    }
    setBusy(false);
    unregisterPairingAgent();
    Q_EMIT authenticationChanged();
    publishChanged();
}

void BluetoothBackend::unregisterPairingAgent()
{
    if (!m_pairingAgentRegistered || m_pairingAgentUnregistrationInProgress) {
        return;
    }
    const auto bus = QDBusConnection::systemBus();
    QDBusInterface manager(QString::fromLatin1(bluezService), QString::fromLatin1(agentManagerPath),
                            QString::fromLatin1(agentManagerInterface), bus);
    if (!manager.isValid()) {
        m_pairingAgentRegistered = false;
        Q_EMIT pairingAgentChanged();
        return;
    }
    m_pairingAgentUnregistrationInProgress = true;
    m_pairingAgentRegistered = false;
    Q_EMIT pairingAgentChanged();
    const auto call = manager.asyncCall(QStringLiteral("UnregisterAgent"),
                                        QVariant::fromValue(BluetoothPairingAgent::objectPath()));
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                m_pairingAgentUnregistrationInProgress = false;
                m_pairingAgentRegistered = false;
                Q_EMIT pairingAgentChanged();
            });
}

void BluetoothBackend::setPairingState(const bool nextPairing, const BluezQt::DevicePtr &device)
{
    const auto nextUbi = nextPairing && device ? device->ubi() : QString{};
    const auto nextName = nextPairing && device
        ? (device->friendlyName().isEmpty() ? device->address() : device->friendlyName())
        : QString{};
    if (m_pairing == nextPairing && m_pairingDeviceUbi == nextUbi && m_pairingDeviceName == nextName) {
        return;
    }
    m_pairing = nextPairing;
    m_pairingDeviceUbi = nextUbi;
    m_pairingDeviceName = nextName;
    Q_EMIT pairingChanged();
}

void BluetoothBackend::connectAdapterSignals()
{
    for (const auto &adapter : m_manager.adapters()) {
        if (!adapter) {
            continue;
        }
        QObject *const adapterObject = adapter.data();
        if (m_connectedAdapterObjects.contains(adapterObject)) {
            continue;
        }
        m_connectedAdapterObjects.insert(adapterObject);
        connect(adapterObject, &QObject::destroyed, this, [this, adapterObject] {
            m_connectedAdapterObjects.remove(adapterObject);
        });
        connect(adapter.data(), &BluezQt::Adapter::poweredChanged, this,
                [this](bool) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::discoveringChanged, this,
                [this](bool) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::discoverableChanged, this,
                [this](bool) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::pairableChanged, this,
                [this](bool) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::discoverableTimeoutChanged, this,
                [this](quint32) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::pairableTimeoutChanged, this,
                [this](quint32) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::deviceAdded, this,
                [this](const BluezQt::DevicePtr &) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::deviceChanged, this,
                [this](const BluezQt::DevicePtr &) { publishChanged(); });
        connect(adapter.data(), &BluezQt::Adapter::deviceRemoved, this,
                [this](const BluezQt::DevicePtr &) { publishChanged(); });
    }
}

void BluetoothBackend::publishChanged()
{
    setAvailable(m_manager.isOperational() && !m_manager.adapters().isEmpty());
    Q_EMIT changed();
}

QString BluetoothBackend::materialIcon(const BluezQt::DevicePtr &device) const
{
    if (!device) {
        return QStringLiteral("bluetooth");
    }
    switch (device->type()) {
    case BluezQt::Device::Headset:
    case BluezQt::Device::Headphones: return QStringLiteral("headphones");
    case BluezQt::Device::AudioVideo: return QStringLiteral("speaker");
    case BluezQt::Device::Keyboard: return QStringLiteral("keyboard");
    case BluezQt::Device::Mouse: return QStringLiteral("mouse");
    case BluezQt::Device::Joypad: return QStringLiteral("sports_esports");
    case BluezQt::Device::Phone: return QStringLiteral("smartphone");
    case BluezQt::Device::Computer: return QStringLiteral("computer");
    default: return QStringLiteral("bluetooth");
    }
}
