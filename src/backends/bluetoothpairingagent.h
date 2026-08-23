#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

#include <optional>

/**
 * A process-local implementation of org.bluez.Agent1 used only for an
 * explicit Meo Settings Device1.Pair request.
 *
 * BluezQt's Agent wrapper cannot advertise BlueZ's KeyboardDisplay
 * capability. Registering this object directly lets the Settings pairing
 * sheet safely handle PIN input, passkey input, numeric comparison and display
 * workflows. It is never made the system default agent, so background and
 * incoming pairings remain owned by the existing desktop Bluetooth agent.
 */
class BluetoothPairingAgent final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")
    Q_CLASSINFO("D-Bus Introspection", ""
        "  <interface name=\"org.bluez.Agent1\">\n"
        "    <method name=\"Release\"/>\n"
        "    <method name=\"RequestPinCode\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "      <arg direction=\"out\" type=\"s\"/>\n"
        "    </method>\n"
        "    <method name=\"DisplayPinCode\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "      <arg direction=\"in\" type=\"s\"/>\n"
        "    </method>\n"
        "    <method name=\"RequestPasskey\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "      <arg direction=\"out\" type=\"u\"/>\n"
        "    </method>\n"
        "    <method name=\"DisplayPasskey\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "      <arg direction=\"in\" type=\"u\"/>\n"
        "      <arg direction=\"in\" type=\"q\"/>\n"
        "    </method>\n"
        "    <method name=\"RequestConfirmation\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "      <arg direction=\"in\" type=\"u\"/>\n"
        "    </method>\n"
        "    <method name=\"RequestAuthorization\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "    </method>\n"
        "    <method name=\"AuthorizeService\">\n"
        "      <arg direction=\"in\" type=\"o\"/>\n"
        "      <arg direction=\"in\" type=\"s\"/>\n"
        "    </method>\n"
        "    <method name=\"Cancel\"/>\n"
        "  </interface>\n"
    )

public:
    explicit BluetoothPairingAgent(QObject *parent = nullptr);

    static QDBusObjectPath objectPath();
    QVariantMap authentication() const;
    bool hasAuthentication() const;
    bool hasResponseRequest() const;
    QString expectedDeviceUbi() const;

    /** Arms the agent for exactly one, user-selected BlueZ object path. */
    void beginPairing(const QString &deviceUbi, const QString &deviceAddress,
                      const QString &deviceName);
    /** Clears a completed pairing's display state without another BlueZ call. */
    void endPairing();
    /** Replies org.bluez.Error.Canceled to an outstanding agent request. */
    void cancelAuthentication();

    bool submitPin(const QString &pin, QString *errorMessage = nullptr);
    bool submitPasskey(const QString &passkey, QString *errorMessage = nullptr);
    bool respondToAuthentication(bool accepted, QString *errorMessage = nullptr);

    static bool isValidPin(const QString &pin);
    static std::optional<quint32> parsePasskey(const QString &passkey);

public Q_SLOTS:
    void Release();
    void RequestPinCode(const QDBusObjectPath &device);
    void DisplayPinCode(const QDBusObjectPath &device, const QString &pinCode);
    void RequestPasskey(const QDBusObjectPath &device);
    void DisplayPasskey(const QDBusObjectPath &device, quint32 passkey, quint16 entered);
    void RequestConfirmation(const QDBusObjectPath &device, quint32 passkey);
    void RequestAuthorization(const QDBusObjectPath &device);
    void AuthorizeService(const QDBusObjectPath &device, const QString &uuid);
    void Cancel();

Q_SIGNALS:
    /** Emits every time a PIN, passkey, confirmation, or display state changes. */
    void authenticationChanged();
    /** Emits when BlueZ cancels a pairing request or unregisters this agent. */
    void authenticationCanceled();
    /** A request for a device other than the user-selected device was rejected. */
    void unexpectedAuthentication(const QString &deviceUbi);
    /** BlueZ called Release; the backend must register again before a new pair. */
    void released();

private:
    enum class PendingKind {
        None,
        Pin,
        Passkey,
        Confirmation,
        Authorization,
        ServiceAuthorization,
    };

    bool acceptsDevice(const QDBusObjectPath &device) const;
    bool beginDelayedRequest(PendingKind kind, const QDBusObjectPath &device,
                             const QString &code = {}, const QString &entered = {},
                             const QString &serviceUuid = {});
    void setDisplayState(const QString &kind, const QDBusObjectPath &device,
                         const QString &code = {}, const QString &entered = {});
    void sendPendingReply(const QVariant &argument = {});
    void sendPendingError(const QString &name, const QString &message);
    void clearAuthentication();
    void clearPendingRequest();
    void rejectUnexpected(const QDBusObjectPath &device);
    void setFailure(QString *errorMessage, const QString &message) const;
    QString deviceNameFor(const QDBusObjectPath &device) const;

    QString m_expectedDeviceUbi;
    QString m_expectedDeviceAddress;
    QString m_expectedDeviceName;
    QVariantMap m_authentication;
    PendingKind m_pendingKind = PendingKind::None;
    QDBusMessage m_pendingMessage;
};
