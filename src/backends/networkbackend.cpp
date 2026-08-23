#include "networkbackend.h"

#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/Utils>
#include <NetworkManagerQt/WirelessNetwork>
#include <NetworkManagerQt/WirelessSetting>

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDBusObjectPath>
#include <QUuid>

#include <algorithm>

namespace
{
NetworkManager::WirelessDevice::Ptr firstWirelessDevice()
{
    const auto devices = NetworkManager::networkInterfaces();
    for (const auto &device : devices) {
        if (device && device->type() == NetworkManager::Device::Wifi) {
            return qSharedPointerDynamicCast<NetworkManager::WirelessDevice>(device);
        }
    }
    return {};
}

bool isSecured(const NetworkManager::AccessPoint::Ptr &accessPoint)
{
    return accessPoint
        && (accessPoint->capabilities().testFlag(NetworkManager::AccessPoint::Privacy)
            || accessPoint->wpaFlags() != NetworkManager::AccessPoint::WpaFlags()
            || accessPoint->rsnFlags() != NetworkManager::AccessPoint::WpaFlags());
}
}

NetworkBackend::NetworkBackend(QObject *parent)
    : BackendBase(parent)
{
    auto *notifier = NetworkManager::notifier();
    const auto refresh = [this] {
        refreshDevice();
        publishChanged();
    };
    connect(notifier, &NetworkManager::Notifier::statusChanged, this,
            [refresh](NetworkManager::Status) { refresh(); });
    connect(notifier, &NetworkManager::Notifier::wirelessEnabledChanged, this,
            [refresh](bool) { refresh(); });
    connect(notifier, &NetworkManager::Notifier::wirelessHardwareEnabledChanged, this,
            [refresh](bool) { refresh(); });
    connect(notifier, &NetworkManager::Notifier::primaryConnectionChanged, this,
            [refresh](const QString &) { refresh(); });
    connect(notifier, &NetworkManager::Notifier::connectivityChanged, this,
            [refresh](NetworkManager::Connectivity) { refresh(); });
    connect(notifier, &NetworkManager::Notifier::deviceAdded, this,
            [refresh](const QString &) { refresh(); });
    connect(notifier, &NetworkManager::Notifier::deviceRemoved, this,
            [refresh](const QString &) { refresh(); });

    refreshDevice();
    publishChanged();
}

bool NetworkBackend::wifiAvailable() const
{
    return !m_wifiDevice.isNull();
}

bool NetworkBackend::wifiEnabled() const
{
    return NetworkManager::isWirelessEnabled();
}

bool NetworkBackend::connected() const
{
    // NetworkManager's global status may be connected through Ethernet, VPN,
    // or a second adapter.  This page must only describe the selected Wi-Fi
    // device as connected when that device itself is activated.
    return m_wifiDevice && m_wifiDevice->state() == NetworkManager::Device::Activated;
}

QString NetworkBackend::connectionName() const
{
    if (!m_wifiDevice) {
        return {};
    }
    const auto accessPoint = m_wifiDevice->activeAccessPoint();
    if (accessPoint && !accessPoint->ssid().isEmpty()) {
        return accessPoint->ssid();
    }
    const auto activeConnection = m_wifiDevice->activeConnection();
    return activeConnection ? activeConnection->id() : QString();
}

bool NetworkBackend::scanning() const
{
    return m_scanning;
}

QVariantList NetworkBackend::networks() const
{
    QVariantList result;
    if (!m_wifiDevice || !wifiEnabled()) {
        return result;
    }

    const QString activeSsid = connectionName();
    const auto activating = m_wifiDevice->activeConnection();
    const QString activatingId = activating ? activating->id() : QString();
    const auto visible = m_wifiDevice->networks();
    result.reserve(visible.size());

    for (const auto &network : visible) {
        if (!network || network->ssid().isEmpty()) {
            continue;
        }
        const auto accessPoint = network->referenceAccessPoint();
        if (!accessPoint) {
            continue;
        }

        const auto security = NetworkManager::findBestWirelessSecurity(
            m_wifiDevice->wirelessCapabilities(), true, false,
            accessPoint->capabilities(), accessPoint->wpaFlags(), accessPoint->rsnFlags());
        const bool requiresPassword = security == NetworkManager::WpaPsk
            || security == NetworkManager::Wpa2Psk
            || security == NetworkManager::SAE;
        const bool directConnectSupported = security == NetworkManager::NoneSecurity
            || requiresPassword
            || security == NetworkManager::OWE;
        const auto saved = savedConnectionForSsid(network->ssid());
        result.push_back(QVariantMap{
            {QStringLiteral("ssid"), network->ssid()},
            {QStringLiteral("strength"), network->signalStrength()},
            {QStringLiteral("secured"), isSecured(accessPoint)},
            {QStringLiteral("securityLabel"), securityLabel(security)},
            // Enhanced Open (OWE) is protected without a user-entered
            // passphrase. Keep that distinction in the model so the QML
            // never presents a password dialog that cannot succeed.
            {QStringLiteral("requiresPassword"), requiresPassword},
            {QStringLiteral("directConnectSupported"), directConnectSupported},
            {QStringLiteral("saved"), !saved.isNull()},
            {QStringLiteral("connected"), network->ssid() == activeSsid && connected()},
            {QStringLiteral("connecting"), network->ssid() == activatingId},
        });
    }

    std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        const auto a = left.toMap();
        const auto b = right.toMap();
        if (a.value(QStringLiteral("connected")).toBool() != b.value(QStringLiteral("connected")).toBool()) {
            return a.value(QStringLiteral("connected")).toBool();
        }
        if (a.value(QStringLiteral("saved")).toBool() != b.value(QStringLiteral("saved")).toBool()) {
            return a.value(QStringLiteral("saved")).toBool();
        }
        return a.value(QStringLiteral("strength")).toInt() > b.value(QStringLiteral("strength")).toInt();
    });
    return result;
}

void NetworkBackend::setWifiEnabled(const bool enabled)
{
    clearError();
    if (!NetworkManager::isWirelessHardwareEnabled()) {
        if (enabled) {
            setError(tr("Wi-Fi is disabled by a hardware or rfkill switch."));
        }
        return;
    }
    if (wifiEnabled() != enabled) {
        NetworkManager::setWirelessEnabled(enabled);
    }
}

void NetworkBackend::requestScan()
{
    clearError();
    if (!m_wifiDevice || !wifiEnabled()) {
        setError(tr("Wi-Fi is unavailable."));
        return;
    }
    if (m_scanning) {
        return;
    }

    setScanning(true);
    const auto reply = m_wifiDevice->requestScan();
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<> result = *watcher;
                if (result.isError()) {
                    setError(result.error().message());
                }
                setScanning(false);
                publishChanged();
                watcher->deleteLater();
            });
}

void NetworkBackend::connectNetwork(const QString &ssid, const QString &password, const bool persist)
{
    clearError();
    if (!m_wifiDevice || ssid.isEmpty()) {
        setError(tr("Wi-Fi network is unavailable."));
        return;
    }

    const auto network = m_wifiDevice->findNetwork(ssid);
    if (!network || !network->referenceAccessPoint()) {
        setError(tr("This Wi-Fi network is no longer visible."));
        return;
    }
    const auto accessPoint = network->referenceAccessPoint();
    if (const auto saved = savedConnectionForSsid(ssid)) {
        setBusy(true);
        const auto reply = NetworkManager::activateConnection(saved->path(), m_wifiDevice->uni(), accessPoint->uni());
        auto *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher](QDBusPendingCallWatcher *) {
                    const QDBusPendingReply<QDBusObjectPath> result = *watcher;
                    if (result.isError()) {
                        setError(result.error().message());
                    }
                    setBusy(false);
                    publishChanged();
                    watcher->deleteLater();
                });
        return;
    }

    const auto security = NetworkManager::findBestWirelessSecurity(
        m_wifiDevice->wirelessCapabilities(), true, false,
        accessPoint->capabilities(), accessPoint->wpaFlags(), accessPoint->rsnFlags());
    NMVariantMapMap settings;
    settings.insert(QStringLiteral("connection"), QVariantMap{
        {QStringLiteral("id"), ssid},
        {QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("type"), QStringLiteral("802-11-wireless")},
        {QStringLiteral("autoconnect"), persist},
    });
    QVariantMap wireless{
        {QStringLiteral("ssid"), ssid.toUtf8()},
        {QStringLiteral("mode"), QStringLiteral("infrastructure")},
    };
    QVariantMap wirelessSecurity;
    switch (security) {
    case NetworkManager::NoneSecurity:
        break;
    case NetworkManager::WpaPsk:
    case NetworkManager::Wpa2Psk:
        if (password.isEmpty()) {
            setError(tr("This Wi-Fi network requires a password."));
            return;
        }
        wireless.insert(QStringLiteral("security"), QStringLiteral("802-11-wireless-security"));
        wirelessSecurity.insert(QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk"));
        wirelessSecurity.insert(QStringLiteral("psk"), password);
        break;
    case NetworkManager::SAE:
        if (password.isEmpty()) {
            setError(tr("This Wi-Fi network requires a password."));
            return;
        }
        wireless.insert(QStringLiteral("security"), QStringLiteral("802-11-wireless-security"));
        wirelessSecurity.insert(QStringLiteral("key-mgmt"), QStringLiteral("sae"));
        wirelessSecurity.insert(QStringLiteral("psk"), password);
        break;
    case NetworkManager::OWE:
        wireless.insert(QStringLiteral("security"), QStringLiteral("802-11-wireless-security"));
        wirelessSecurity.insert(QStringLiteral("key-mgmt"), QStringLiteral("owe"));
        break;
    default:
        setError(tr("This security type needs the advanced NetworkManager settings UI."));
        return;
    }
    settings.insert(QStringLiteral("802-11-wireless"), wireless);
    if (!wirelessSecurity.isEmpty()) {
        settings.insert(QStringLiteral("802-11-wireless-security"), wirelessSecurity);
    }
    settings.insert(QStringLiteral("ipv4"), QVariantMap{{QStringLiteral("method"), QStringLiteral("auto")}});
    settings.insert(QStringLiteral("ipv6"), QVariantMap{{QStringLiteral("method"), QStringLiteral("auto")}});

    setBusy(true);
    const auto reply = NetworkManager::addAndActivateConnection2(
        settings, m_wifiDevice->uni(), accessPoint->uni(),
        // NetworkManager owns the profile and credentials. A volatile profile
        // is never written to disk and is removed when it disconnects.
        QVariantMap{{QStringLiteral("persist"), persist ? QStringLiteral("disk") : QStringLiteral("volatile")}});
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QDBusObjectPath, QDBusObjectPath, QVariantMap> result = *watcher;
                if (result.isError()) {
                    setError(result.error().message());
                }
                setBusy(false);
                publishChanged();
                watcher->deleteLater();
            });
}

void NetworkBackend::disconnectCurrent()
{
    clearError();
    if (!m_wifiDevice || !m_wifiDevice->activeConnection()) {
        return;
    }
    setBusy(true);
    const auto reply = NetworkManager::deactivateConnection(m_wifiDevice->activeConnection()->path());
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<> result = *watcher;
                if (result.isError()) {
                    setError(result.error().message());
                }
                setBusy(false);
                publishChanged();
                watcher->deleteLater();
            });
}

void NetworkBackend::refreshDevice()
{
    const auto nextDevice = firstWirelessDevice();
    if (nextDevice == m_wifiDevice) {
        setAvailable(!NetworkManager::networkInterfaces().isEmpty());
        return;
    }
    if (m_wifiDevice) {
        disconnect(m_wifiDevice.data(), nullptr, this, nullptr);
    }
    m_wifiDevice = nextDevice;
    bindDevice();
    setAvailable(!NetworkManager::networkInterfaces().isEmpty());
}

void NetworkBackend::bindDevice()
{
    if (!m_wifiDevice) {
        return;
    }
    connect(m_wifiDevice.data(), &NetworkManager::WirelessDevice::networkAppeared, this,
            [this](const QString &) { publishChanged(); });
    connect(m_wifiDevice.data(), &NetworkManager::WirelessDevice::networkDisappeared, this,
            [this](const QString &) { publishChanged(); });
    connect(m_wifiDevice.data(), &NetworkManager::WirelessDevice::activeAccessPointChanged, this,
            [this](const QString &) { publishChanged(); });
    connect(m_wifiDevice.data(), &NetworkManager::WirelessDevice::lastScanChanged, this,
            [this](const QDateTime &) {
                setScanning(false);
                publishChanged();
            });
    connect(m_wifiDevice.data(), &NetworkManager::Device::connectionStateChanged, this,
            [this] { publishChanged(); });
}

void NetworkBackend::publishChanged()
{
    setAvailable(!NetworkManager::networkInterfaces().isEmpty());
    Q_EMIT changed();
}

void NetworkBackend::setScanning(const bool scanning)
{
    if (m_scanning == scanning) {
        return;
    }
    m_scanning = scanning;
    Q_EMIT changed();
}

NetworkManager::Connection::Ptr NetworkBackend::savedConnectionForSsid(const QString &ssid) const
{
    const auto connections = NetworkManager::listConnections();
    for (const auto &connection : connections) {
        if (!connection) {
            continue;
        }
        const auto settings = connection->settings();
        if (!settings || settings->connectionType() != NetworkManager::ConnectionSettings::Wireless) {
            continue;
        }
        const auto genericWireless = settings->setting(NetworkManager::Setting::Wireless);
        const auto wireless = qSharedPointerDynamicCast<NetworkManager::WirelessSetting>(genericWireless);
        if (wireless && QString::fromUtf8(wireless->ssid()) == ssid) {
            return connection;
        }
    }
    return {};
}

QString NetworkBackend::securityLabel(const NetworkManager::WirelessSecurityType security) const
{
    switch (security) {
    case NetworkManager::NoneSecurity: return tr("Open");
    case NetworkManager::WpaPsk: return tr("WPA");
    case NetworkManager::Wpa2Psk: return tr("WPA2");
    case NetworkManager::SAE: return tr("WPA3");
    case NetworkManager::OWE: return tr("Enhanced open");
    default: return tr("Secured");
    }
}
