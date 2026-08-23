#include "bluetoothpairingagent.h"

#include <QDBusConnection>
#include <QRegularExpression>

namespace {

constexpr auto agentPath = "/org/meo/settings/BluetoothAgent";
constexpr auto rejectedError = "org.bluez.Error.Rejected";
constexpr auto canceledError = "org.bluez.Error.Canceled";

QString normalizedAddress(const QString &address)
{
    return address.trimmed().toUpper();
}

QString paddedPasskey(const quint32 passkey)
{
    return QString::number(passkey).rightJustified(6, QLatin1Char('0'));
}

} // namespace

BluetoothPairingAgent::BluetoothPairingAgent(QObject *parent)
    : QObject(parent)
{
}

QDBusObjectPath BluetoothPairingAgent::objectPath()
{
    return QDBusObjectPath(QString::fromLatin1(agentPath));
}

QVariantMap BluetoothPairingAgent::authentication() const
{
    return m_authentication;
}

bool BluetoothPairingAgent::hasAuthentication() const
{
    return !m_authentication.isEmpty();
}

bool BluetoothPairingAgent::hasResponseRequest() const
{
    return m_pendingKind != PendingKind::None;
}

QString BluetoothPairingAgent::expectedDeviceUbi() const
{
    return m_expectedDeviceUbi;
}

void BluetoothPairingAgent::beginPairing(const QString &deviceUbi, const QString &deviceAddress,
                                         const QString &deviceName)
{
    cancelAuthentication();
    m_expectedDeviceUbi = deviceUbi;
    m_expectedDeviceAddress = normalizedAddress(deviceAddress);
    m_expectedDeviceName = deviceName;
}

void BluetoothPairingAgent::endPairing()
{
    // A delayed Agent1 request must always receive a terminal D-Bus reply.
    // Dropping the stored message would leave BlueZ waiting until its own
    // timeout when a device vanishes, the page closes, or Pair() completes
    // unexpectedly.
    cancelAuthentication();
    clearAuthentication();
    clearPendingRequest();
    m_expectedDeviceUbi.clear();
    m_expectedDeviceAddress.clear();
    m_expectedDeviceName.clear();
}

void BluetoothPairingAgent::cancelAuthentication()
{
    const bool hadAuthentication = hasAuthentication();
    if (hasResponseRequest()) {
        sendPendingError(QString::fromLatin1(canceledError), tr("Pairing was canceled in Meo Settings."));
    }
    clearAuthentication();
    if (hadAuthentication) {
        Q_EMIT authenticationCanceled();
    }
}

bool BluetoothPairingAgent::isValidPin(const QString &pin)
{
    static const QRegularExpression pinExpression(QStringLiteral("^[A-Za-z0-9]{1,16}$"));
    return pinExpression.match(pin).hasMatch();
}

std::optional<quint32> BluetoothPairingAgent::parsePasskey(const QString &passkey)
{
    static const QRegularExpression passkeyExpression(QStringLiteral("^[0-9]{1,6}$"));
    if (!passkeyExpression.match(passkey).hasMatch()) {
        return std::nullopt;
    }

    bool ok = false;
    const auto value = passkey.toUInt(&ok, 10);
    if (!ok || value > 999999U) {
        return std::nullopt;
    }
    return value;
}

bool BluetoothPairingAgent::submitPin(const QString &pin, QString *errorMessage)
{
    if (m_pendingKind != PendingKind::Pin) {
        setFailure(errorMessage, tr("There is no PIN request waiting for a response."));
        return false;
    }
    if (!isValidPin(pin)) {
        setFailure(errorMessage, tr("A Bluetooth PIN must contain 1 to 16 letters or digits."));
        return false;
    }

    sendPendingReply(pin);
    clearAuthentication();
    return true;
}

bool BluetoothPairingAgent::submitPasskey(const QString &passkey, QString *errorMessage)
{
    if (m_pendingKind != PendingKind::Passkey) {
        setFailure(errorMessage, tr("There is no passkey request waiting for a response."));
        return false;
    }
    const auto parsed = parsePasskey(passkey);
    if (!parsed) {
        setFailure(errorMessage, tr("A Bluetooth passkey must be a number from 0 to 999999."));
        return false;
    }

    sendPendingReply(*parsed);
    clearAuthentication();
    return true;
}

bool BluetoothPairingAgent::respondToAuthentication(const bool accepted, QString *errorMessage)
{
    if (m_pendingKind != PendingKind::Confirmation
        && m_pendingKind != PendingKind::Authorization
        && m_pendingKind != PendingKind::ServiceAuthorization) {
        setFailure(errorMessage, tr("There is no confirmation waiting for a response."));
        return false;
    }

    if (accepted) {
        sendPendingReply();
    } else {
        sendPendingError(QString::fromLatin1(rejectedError), tr("Pairing was not confirmed."));
    }
    clearAuthentication();
    return true;
}

void BluetoothPairingAgent::Release()
{
    Cancel();
    m_expectedDeviceUbi.clear();
    m_expectedDeviceAddress.clear();
    m_expectedDeviceName.clear();
    Q_EMIT released();
}

void BluetoothPairingAgent::RequestPinCode(const QDBusObjectPath &device)
{
    beginDelayedRequest(PendingKind::Pin, device);
}

void BluetoothPairingAgent::DisplayPinCode(const QDBusObjectPath &device, const QString &pinCode)
{
    if (!acceptsDevice(device)) {
        rejectUnexpected(device);
        return;
    }
    setDisplayState(QStringLiteral("display-pin"), device, pinCode);
}

void BluetoothPairingAgent::RequestPasskey(const QDBusObjectPath &device)
{
    beginDelayedRequest(PendingKind::Passkey, device);
}

void BluetoothPairingAgent::DisplayPasskey(const QDBusObjectPath &device, const quint32 passkey,
                                            const quint16 entered)
{
    if (!acceptsDevice(device)) {
        rejectUnexpected(device);
        return;
    }
    setDisplayState(QStringLiteral("display-passkey"), device, paddedPasskey(passkey),
                    QString::number(entered));
}

void BluetoothPairingAgent::RequestConfirmation(const QDBusObjectPath &device, const quint32 passkey)
{
    beginDelayedRequest(PendingKind::Confirmation, device, paddedPasskey(passkey));
}

void BluetoothPairingAgent::RequestAuthorization(const QDBusObjectPath &device)
{
    beginDelayedRequest(PendingKind::Authorization, device);
}

void BluetoothPairingAgent::AuthorizeService(const QDBusObjectPath &device, const QString &uuid)
{
    beginDelayedRequest(PendingKind::ServiceAuthorization, device, {}, {}, uuid);
}

void BluetoothPairingAgent::Cancel()
{
    const bool hadAuthentication = hasAuthentication();
    clearPendingRequest();
    clearAuthentication();
    if (hadAuthentication) {
        Q_EMIT authenticationCanceled();
    }
}

bool BluetoothPairingAgent::acceptsDevice(const QDBusObjectPath &device) const
{
    return !m_expectedDeviceUbi.isEmpty() && device.path() == m_expectedDeviceUbi;
}

bool BluetoothPairingAgent::beginDelayedRequest(const PendingKind kind, const QDBusObjectPath &device,
                                                 const QString &code, const QString &entered,
                                                 const QString &serviceUuid)
{
    if (!acceptsDevice(device)) {
        rejectUnexpected(device);
        return false;
    }

    // An Agent1 implementation must not leave two reply-required D-Bus calls
    // open. Cancel the old one rather than letting a later UI decision answer
    // the wrong pairing transaction.
    cancelAuthentication();
    if (!calledFromDBus()) {
        return false;
    }

    setDelayedReply(true);
    m_pendingMessage = message();
    m_pendingKind = kind;

    QString kindName;
    switch (kind) {
    case PendingKind::Pin: kindName = QStringLiteral("pin"); break;
    case PendingKind::Passkey: kindName = QStringLiteral("passkey"); break;
    case PendingKind::Confirmation: kindName = QStringLiteral("confirmation"); break;
    case PendingKind::Authorization: kindName = QStringLiteral("authorization"); break;
    case PendingKind::ServiceAuthorization: kindName = QStringLiteral("service-authorization"); break;
    case PendingKind::None: return false;
    }

    m_authentication = {
        {QStringLiteral("kind"), kindName},
        {QStringLiteral("deviceUbi"), device.path()},
        {QStringLiteral("deviceAddress"), m_expectedDeviceAddress},
        {QStringLiteral("deviceName"), deviceNameFor(device)},
        {QStringLiteral("code"), code},
        {QStringLiteral("entered"), entered},
        {QStringLiteral("serviceUuid"), serviceUuid},
        {QStringLiteral("requiresResponse"), true},
    };
    Q_EMIT authenticationChanged();
    return true;
}

void BluetoothPairingAgent::setDisplayState(const QString &kind, const QDBusObjectPath &device,
                                            const QString &code, const QString &entered)
{
    if (hasResponseRequest()) {
        cancelAuthentication();
    } else {
        clearAuthentication();
    }
    m_authentication = {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("deviceUbi"), device.path()},
        {QStringLiteral("deviceAddress"), m_expectedDeviceAddress},
        {QStringLiteral("deviceName"), deviceNameFor(device)},
        {QStringLiteral("code"), code},
        {QStringLiteral("entered"), entered},
        {QStringLiteral("serviceUuid"), QString{}},
        {QStringLiteral("requiresResponse"), false},
    };
    Q_EMIT authenticationChanged();
}

void BluetoothPairingAgent::sendPendingReply(const QVariant &argument)
{
    if (!hasResponseRequest()) {
        return;
    }
    const QDBusMessage reply = argument.isValid()
        ? m_pendingMessage.createReply(argument)
        : m_pendingMessage.createReply();
    connection().send(reply);
    clearPendingRequest();
}

void BluetoothPairingAgent::sendPendingError(const QString &name, const QString &message)
{
    if (!hasResponseRequest()) {
        return;
    }
    connection().send(m_pendingMessage.createErrorReply(name, message));
    clearPendingRequest();
}

void BluetoothPairingAgent::clearAuthentication()
{
    if (m_authentication.isEmpty()) {
        return;
    }
    m_authentication.clear();
    Q_EMIT authenticationChanged();
}

void BluetoothPairingAgent::clearPendingRequest()
{
    m_pendingKind = PendingKind::None;
    m_pendingMessage = QDBusMessage{};
}

void BluetoothPairingAgent::rejectUnexpected(const QDBusObjectPath &device)
{
    if (calledFromDBus()) {
        sendErrorReply(QString::fromLatin1(rejectedError),
                       tr("This pairing request was not started by Meo Settings."));
    }
    Q_EMIT unexpectedAuthentication(device.path());
}

void BluetoothPairingAgent::setFailure(QString *errorMessage, const QString &message) const
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

QString BluetoothPairingAgent::deviceNameFor(const QDBusObjectPath &device) const
{
    return device.path() == m_expectedDeviceUbi && !m_expectedDeviceName.isEmpty()
        ? m_expectedDeviceName
        : m_expectedDeviceAddress;
}
