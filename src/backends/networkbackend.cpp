#include "networkbackend.h"

#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/Utils>
#include <NetworkManagerQt/WirelessNetwork>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/Ipv4Setting>

#include <QDBusPendingCallWatcher>
#include <QDBusConnection>
#include <QHostAddress>
#include <QRegularExpression>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDBusObjectPath>
#include <QUuid>
#include <QtEndian>

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

    connect(notifier, &NetworkManager::Notifier::networkingEnabledChanged, this, &NetworkBackend::publishChanged);
    connect(notifier, &NetworkManager::Notifier::activeConnectionsChanged, this, &NetworkBackend::publishChanged);
    auto *settings = NetworkManager::settingsNotifier();
    connect(settings, &NetworkManager::SettingsNotifier::connectionAdded, this, [this](const QString &path) {
        if (const auto connection = NetworkManager::findConnection(path))
            connect(connection.data(), &NetworkManager::Connection::updated, this, &NetworkBackend::publishChanged, Qt::UniqueConnection);
        publishChanged();
    });
    connect(settings, &NetworkManager::SettingsNotifier::connectionRemoved, this, &NetworkBackend::publishChanged);
    for (const auto &connection : NetworkManager::listConnections())
        connect(connection.data(), &NetworkManager::Connection::updated, this, &NetworkBackend::publishChanged, Qt::UniqueConnection);

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

bool NetworkBackend::systemConnected() const
{
    const auto state = NetworkManager::status();
    return state == NetworkManager::Connected
        || state == NetworkManager::ConnectedSiteOnly
        || state == NetworkManager::ConnectedLinkLocal;
}

bool NetworkBackend::internetAvailable() const
{
    return NetworkManager::connectivity() == NetworkManager::Full;
}

QString NetworkBackend::connectivityState() const
{
    switch (NetworkManager::connectivity()) {
    case NetworkManager::Full:
        return QStringLiteral("online");
    case NetworkManager::Portal:
        return QStringLiteral("portal");
    case NetworkManager::Limited:
        return QStringLiteral("limited");
    case NetworkManager::NoConnectivity:
        return QStringLiteral("offline");
    case NetworkManager::UnknownConnectivity:
    default:
        switch (NetworkManager::status()) {
        case NetworkManager::Connected:
            return QStringLiteral("connected");
        case NetworkManager::ConnectedSiteOnly:
        case NetworkManager::ConnectedLinkLocal:
            return QStringLiteral("limited");
        case NetworkManager::Connecting:
            return QStringLiteral("connecting");
        case NetworkManager::Disconnected:
        case NetworkManager::Disconnecting:
        case NetworkManager::Asleep:
            return QStringLiteral("offline");
        case NetworkManager::Unknown:
        default:
            return QStringLiteral("unknown");
        }
    }
}

QString NetworkBackend::primaryConnectionName() const
{
    const auto primary = NetworkManager::primaryConnection();
    if (primary && !primary->id().isEmpty()) {
        return primary->id();
    }
    return connectionName();
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

void NetworkBackend::forgetNetwork(const QString &ssid)
{
    clearError();
    if (ssid.isEmpty()) {
        return;
    }
    if (busy()) {
        setError(tr("Another network operation is still in progress."));
        return;
    }

    // Deliberately resolve only an existing NetworkManager profile.  The UI
    // never deletes a visible access point or writes a credential itself.
    const auto saved = savedConnectionForSsid(ssid);
    if (!saved) {
        setError(tr("This saved Wi-Fi network is no longer available."));
        return;
    }

    setBusy(true);
    const auto reply = saved->remove();
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

QVariantList NetworkBackend::devices() const
{
    QVariantList result;
    for (const auto &device : NetworkManager::networkInterfaces()) {
        if (!device) continue;
        const bool wired = device->type() == NetworkManager::Device::Ethernet;
        const bool wireless = device->type() == NetworkManager::Device::Wifi;
        if (!wired && !wireless) continue;
        result.append(QVariantMap{{"name", device->interfaceName()}, {"wired", wired},
            {"connected", device->state() == NetworkManager::Device::Activated}, {"managed", device->managed()}});
    }
    return result;
}

QVariantList NetworkBackend::profiles() const
{
    QVariantList result;
    for (const auto &connection : NetworkManager::listConnections()) {
        const auto settings = connection->settings()->toMap();
        const auto identity = settings.value(QStringLiteral("connection"));
        const QString uuid = identity.value(QStringLiteral("uuid")).toString();
        if (uuid.isEmpty()) continue;
        bool active = false, connecting = false;
        for (const auto &candidate : NetworkManager::activeConnections()) if (candidate && candidate->uuid() == uuid) {
            active = candidate->state() == NetworkManager::ActiveConnection::Activated;
            connecting = candidate->state() == NetworkManager::ActiveConnection::Activating;
        }
        const auto ip = settings.value(QStringLiteral("ipv4"));
        const auto ipv4 = connection->settings()->setting(NetworkManager::Setting::Ipv4).dynamicCast<NetworkManager::Ipv4Setting>();
        QStringList dns; QString address, gateway; int prefix = 24;
        if (ipv4) {
            for (const auto &server : ipv4->dns()) dns.append(server.toString());
            if (!ipv4->addresses().isEmpty()) {
                const auto current = ipv4->addresses().first();
                address = current.ip().toString(); prefix = current.prefixLength(); gateway = current.gateway().isNull() ? QString() : current.gateway().toString();
            }
        }
        // Never publish the opaque VPN/security sections or connection secrets to QML.
        result.append(QVariantMap{{"uuid", uuid}, {"name", connection->name()},
            {"type", identity.value(QStringLiteral("type"))}, {"active", active}, {"connecting", connecting},
            {"autoconnect", identity.value(QStringLiteral("autoconnect"), true)},
            {"metered", identity.value(QStringLiteral("metered"), 0)}, {"ipv4Method", ip.value(QStringLiteral("method"))},
            {"address", address}, {"prefix", prefix}, {"gateway", gateway}, {"dns", dns.join(QStringLiteral(", "))},
            {"basicIpv4", ipv4 && ipv4->addresses().size() <= 1 && (ip.value("method") == "auto" || ip.value("method") == "manual")}});
    }
    return result;
}

void NetworkBackend::activateProfile(const QString &uuid)
{
    if (busy()) return;
    const auto connection = NetworkManager::findConnectionByUuid(uuid);
    if (!connection) { setError(tr("This connection no longer exists.")); return; }
    setBusy(true); clearError();
    // NetworkManager chooses a compatible device; its secret agent owns VPN/enterprise authentication.
    auto *watcher = new QDBusPendingCallWatcher(NetworkManager::activateConnection(connection->path(), QStringLiteral("/"), QStringLiteral("/")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QDBusObjectPath> result = *call;
        if (result.isError()) setError(result.error().message());
        call->deleteLater(); setBusy(false); publishChanged();
    });
}

void NetworkBackend::deactivateProfile(const QString &uuid)
{
    if (busy()) return;
    for (const auto &active : NetworkManager::activeConnections()) if (active && active->uuid() == uuid) {
        setBusy(true); clearError();
        auto *watcher = new QDBusPendingCallWatcher(NetworkManager::deactivateConnection(active->path()), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<> result = *call;
            if (result.isError()) setError(result.error().message());
            call->deleteLater(); setBusy(false); publishChanged();
        });
        return;
    }
    setError(tr("This connection is not active."));
}

void NetworkBackend::configureProfile(const QString &uuid, const QVariantMap &changes)
{
    if (busy()) return;
    const auto connection = NetworkManager::findConnectionByUuid(uuid);
    const QStringList allowed{QStringLiteral("autoconnect"), QStringLiteral("metered"), QStringLiteral("ipv4Method"), QStringLiteral("address"), QStringLiteral("prefix"), QStringLiteral("gateway"), QStringLiteral("dns")};
    if (!connection || changes.isEmpty()) { setError(tr("Select an existing connection.")); return; }
    for (auto it = changes.begin(); it != changes.end(); ++it)
        if (!allowed.contains(it.key())) { setError(tr("Unsupported network setting.")); return; }
    QVariantMap identity, ip;
    if (changes.contains("autoconnect")) {
        if (changes.value("autoconnect").metaType().id() != QMetaType::Bool) { setError(tr("Invalid automatic connection setting.")); return; }
        identity.insert("autoconnect", changes.value("autoconnect"));
    }
    if (changes.contains("metered")) {
        bool valid = false; const int value = changes.value("metered").toInt(&valid);
        if (!valid || value < 0 || value > 2) { setError(tr("Invalid metered connection setting.")); return; }
        identity.insert("metered", value);
    }
    if (changes.contains("ipv4Method")) {
        const auto current = connection->settings();
        const auto ipv4 = current->setting(NetworkManager::Setting::Ipv4).dynamicCast<NetworkManager::Ipv4Setting>();
        const auto type = current->toMap().value("connection").value("type").toString();
        if (!ipv4 || (type != "802-3-ethernet" && type != "802-11-wireless") || ipv4->addresses().size() > 1
            || (ipv4->method() != NetworkManager::Ipv4Setting::Automatic && ipv4->method() != NetworkManager::Ipv4Setting::Manual)) {
            setError(tr("This connection uses an advanced address configuration.")); return;
        }
        const QString method = changes.value("ipv4Method").toString();
        if (method != "auto" && method != "manual") { setError(tr("Choose automatic or manual IPv4 configuration.")); return; }
        ip.insert("method", method);
        if (method == "manual") {
            const QHostAddress address(changes.value("address").toString());
            const QHostAddress gateway(changes.value("gateway").toString());
            bool valid = false; const int prefix = changes.value("prefix").toInt(&valid);
            if (address.protocol() != QAbstractSocket::IPv4Protocol || !valid || prefix < 1 || prefix > 32
                || (!changes.value("gateway").toString().isEmpty() && gateway.protocol() != QAbstractSocket::IPv4Protocol)) {
                setError(tr("Enter a valid IPv4 address, prefix and optional gateway.")); return;
            }
            ip.insert("address-data", QVariant::fromValue(QList<QVariantMap>{{{"address", address.toString()}, {"prefix", uint(prefix)}}}));
            ip.insert("gateway", gateway.isNull() ? QString() : gateway.toString());
        }
        QVariantList servers;
        for (const auto &server : changes.value("dns").toString().split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts)) {
            const QHostAddress address(server);
            if (address.protocol() != QAbstractSocket::IPv4Protocol) { setError(tr("Enter valid IPv4 DNS servers.")); return; }
            servers.append(uint(address.toIPv4Address()));
        }
        // Legacy IPv4 DNS integers use network byte order in the NetworkManager D-Bus contract.
        QList<uint> encodedDns;
        for (const auto &server : servers) encodedDns.append(qToBigEndian(server.toUInt()));
        ip.insert("dns", QVariant::fromValue(encodedDns)); ip.insert("ignore-auto-dns", !servers.isEmpty());
    } else if (changes.contains("address") || changes.contains("prefix") || changes.contains("gateway") || changes.contains("dns")) {
        setError(tr("Choose an IPv4 method before changing addresses.")); return;
    }
    setBusy(true); clearError();
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.NetworkManager"), connection->path(), QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection"), QStringLiteral("GetSettings"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, connection, identity, ip](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<NMVariantMapMap> reply = *call; call->deleteLater();
        if (reply.isError()) { setBusy(false); setError(reply.error().message()); return; }
        auto settings = reply.value();
        auto connectionValues = settings.value("connection");
        for (auto it = identity.begin(); it != identity.end(); ++it) connectionValues.insert(it.key(), it.value());
        settings.insert("connection", connectionValues);
        if (!ip.isEmpty()) {
            auto ipv4 = settings.value("ipv4"); ipv4.remove("addresses"); ipv4.remove("address-data"); ipv4.remove("gateway");
            for (auto it = ip.begin(); it != ip.end(); ++it) ipv4.insert(it.key(), it.value());
            settings.insert("ipv4", ipv4);
        }
        // GetSettings contains no secrets. Preserve that property: NetworkManager's
        // update_auth_cb merges its stored and agent-owned secrets when the new
        // settings contain no secrets. Do not fetch credentials into the UI or
        // inject a partially populated secret group that would replace that cache.
        auto *update = new QDBusPendingCallWatcher(connection->update(settings), this);
        connect(update, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished) {
            const QDBusPendingReply<> result = *finished;
            if (result.isError()) setError(result.error().message());
            finished->deleteLater(); setBusy(false); publishChanged();
        });
    });
}

bool NetworkBackend::hotspotSupported() const
{
    return m_wifiDevice && m_wifiDevice->wirelessCapabilities().testFlag(NetworkManager::WirelessDevice::ApCap)
        && m_wifiDevice->wirelessCapabilities().testFlag(NetworkManager::WirelessDevice::Rsn);
}

bool NetworkBackend::hotspotActive() const
{
    if (!m_wifiDevice || !m_wifiDevice->activeConnection() || !m_wifiDevice->activeConnection()->connection()) return false;
    return m_wifiDevice->activeConnection()->connection()->settings()->toMap().value(QStringLiteral("802-11-wireless")).value(QStringLiteral("mode")).toString() == "ap";
}

bool NetworkBackend::networkingEnabled() const { return NetworkManager::isNetworkingEnabled(); }

void NetworkBackend::setNetworkingEnabled(bool enabled)
{
    if (busy()) return;
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("/org/freedesktop/NetworkManager"), QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("Enable"));
    message << enabled; setBusy(true); clearError();
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 30000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<> result = *call;
        if (result.isError()) setError(result.error().message());
        call->deleteLater(); setBusy(false); publishChanged();
    });
}

void NetworkBackend::startHotspot(const QString &ssid, const QString &password)
{
    if (busy()) return;
    if (!hotspotSupported() || !wifiEnabled() || !networkingEnabled() || ssid.toUtf8().isEmpty() || ssid.toUtf8().size() > 32
        || password.size() < 8 || password.size() > 63 || !QRegularExpression(QStringLiteral("^[ -~]+$")).match(password).hasMatch()) {
        setError(tr("Choose a network name up to 32 bytes and a password of 8–63 printable ASCII characters on an adapter supporting WPA2 hotspot mode.")); return;
    }
    if (hotspotActive()) { setError(tr("A hotspot is already active on this adapter.")); return; }
    NMVariantMapMap settings{
        {"connection", {{"id", ssid}, {"uuid", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"type", "802-11-wireless"}, {"autoconnect", false}}},
        {"802-11-wireless", {{"ssid", ssid.toUtf8()}, {"mode", "ap"}, {"security", "802-11-wireless-security"}}},
        {"802-11-wireless-security", {{"key-mgmt", "wpa-psk"}, {"psk", password}, {"proto", QStringList{"rsn"}}}},
        {"ipv4", {{"method", "shared"}}}, {"ipv6", {{"method", "disabled"}}}
    };
    setBusy(true); clearError();
    // The secret/profile exists only while active; stopping sharing removes the volatile profile.
    auto *watcher = new QDBusPendingCallWatcher(NetworkManager::addAndActivateConnection2(settings, m_wifiDevice->uni(), QStringLiteral("/"), {{"persist", "volatile"}}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusMessage result = call->reply();
        if (result.type() == QDBusMessage::ErrorMessage) setError(result.errorMessage());
        call->deleteLater(); setBusy(false); publishChanged();
    });
}

void NetworkBackend::stopHotspot()
{
    if (!hotspotActive()) { setError(tr("No hotspot is active on this adapter.")); return; }
    deactivateProfile(m_wifiDevice->activeConnection()->uuid());
}
