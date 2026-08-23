#pragma once

#include "../core/backendbase.h"
#include "bluetoothpairingagent.h"

#include <BluezQt/Manager>

#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

/**
 * The primary Meo Settings BlueZ controller.
 *
 * It deliberately keeps the system service authoritative: BluezQt supplies
 * state and normal device calls while a short-lived private Agent1 object
 * handles only an explicit Pair() request initiated here. The agent is never
 * made BlueZ's default agent, so it cannot take over incoming/background
 * pairing from the desktop Bluetooth service.
 */
class BluetoothBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool discovering READ discovering NOTIFY changed)
    Q_PROPERTY(QVariantList adapters READ adapters NOTIFY changed)
    Q_PROPERTY(QString activeAdapterUbi READ activeAdapterUbi WRITE setActiveAdapterUbi NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(bool rfkillBlocked READ rfkillBlocked NOTIFY changed)
    Q_PROPERTY(bool pairing READ pairing NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingDeviceUbi READ pairingDeviceUbi NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingDeviceName READ pairingDeviceName NOTIFY pairingChanged)
    Q_PROPERTY(bool pairingAgentReady READ pairingAgentReady NOTIFY pairingAgentChanged)
    Q_PROPERTY(QVariantMap authentication READ authentication NOTIFY authenticationChanged)
    Q_PROPERTY(bool authenticationPending READ authenticationPending NOTIFY authenticationChanged)

public:
    explicit BluetoothBackend(QObject *parent = nullptr);
    ~BluetoothBackend() override;

    bool enabled() const;
    bool discovering() const;
    QVariantList adapters() const;
    QString activeAdapterUbi() const;
    QVariantList devices() const;
    bool rfkillBlocked() const;
    bool pairing() const;
    QString pairingDeviceUbi() const;
    QString pairingDeviceName() const;
    bool pairingAgentReady() const;
    QVariantMap authentication() const;
    bool authenticationPending() const;

    void setEnabled(bool enabled);
    void setActiveAdapterUbi(const QString &ubi);

    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void stopDiscovery();
    Q_INVOKABLE void setAdapterPowered(const QString &adapterUbi, bool powered);
    Q_INVOKABLE void setAdapterDiscoverable(const QString &adapterUbi, bool discoverable);
    Q_INVOKABLE void setAdapterPairable(const QString &adapterUbi, bool pairable);
    Q_INVOKABLE void setAdapterDiscoverableTimeout(const QString &adapterUbi, int seconds);
    Q_INVOKABLE void setAdapterPairableTimeout(const QString &adapterUbi, int seconds);

    /** Starts a pairing flow only after an explicit Settings UI action. */
    Q_INVOKABLE void pairDevice(const QString &ubi);
    /** Cancels the active Meo Settings pairing flow and any pending prompt. */
    Q_INVOKABLE void cancelPairing();
    Q_INVOKABLE void connectDevice(const QString &ubi);
    Q_INVOKABLE void disconnectDevice(const QString &ubi);
    Q_INVOKABLE void setDeviceTrusted(const QString &ubi, bool trusted);
    Q_INVOKABLE void setDeviceBlocked(const QString &ubi, bool blocked);
    Q_INVOKABLE void renameDevice(const QString &ubi, const QString &name);
    Q_INVOKABLE void unpairDevice(const QString &ubi);
    Q_INVOKABLE void submitPin(const QString &pin);
    Q_INVOKABLE void submitPasskey(const QString &passkey);
    Q_INVOKABLE void respondToAuthentication(bool accepted);

    // Compatibility for first-milestone callers. Address-only actions are
    // rejected when they could identify more than one adapter/device; the
    // native UI always calls the UBI/object-path APIs above.
    Q_INVOKABLE void toggleDevice(const QString &address);
    Q_INVOKABLE void forgetDevice(const QString &address);

Q_SIGNALS:
    void changed();
    void pairingChanged();
    void pairingAgentChanged();
    void authenticationChanged();

private:
    bool beginOperation(const QString &description);
    void finishOperation();
    BluezQt::AdapterPtr activeAdapter() const;
    BluezQt::AdapterPtr adapterForAction(const QString &ubi, const QString &operation,
                                         bool requirePowered = false);
    BluezQt::DevicePtr deviceForAction(const QString &ubi, const QString &operation,
                                       bool requirePoweredAdapter = true);
    BluezQt::DevicePtr deviceForLegacyAddress(const QString &address, const QString &operation);
    void ensurePairingAgent(const BluezQt::DevicePtr &device, quint64 generation);
    void startPairing(const BluezQt::DevicePtr &device, quint64 generation);
    void finishPairing(quint64 generation, const QString &error = {}, bool canceled = false);
    void unregisterPairingAgent();
    void setPairingState(bool pairing, const BluezQt::DevicePtr &device = {});
    void connectAdapterSignals();
    void publishChanged();
    QString materialIcon(const BluezQt::DevicePtr &device) const;

    BluezQt::Manager m_manager;
    BluetoothPairingAgent m_pairingAgent;
    QTimer m_pairingTimeout;
    QString m_activeAdapterUbi;
    bool m_pairing = false;
    bool m_pairingCancellationRequested = false;
    bool m_pairingAgentObjectExported = false;
    bool m_pairingAgentRegistered = false;
    bool m_pairingAgentRegistrationInProgress = false;
    bool m_pairingAgentUnregistrationInProgress = false;
    quint64 m_pairingGeneration = 0;
    QString m_pairingDeviceUbi;
    QString m_pairingDeviceName;
    // Qt cannot combine Qt::UniqueConnection with functor/lambda slots. Keep
    // one explicit connection bundle per live adapter instead of silently
    // registering duplicate publish callbacks every time BlueZ refreshes.
    QSet<QObject *> m_connectedAdapterObjects;
};
