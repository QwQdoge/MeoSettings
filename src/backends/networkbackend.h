#pragma once

#include "../core/backendbase.h"

#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/Utils>
#include <NetworkManagerQt/WirelessDevice>

#include <QVariantList>

class NetworkBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY changed)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY changed)

public:
    explicit NetworkBackend(QObject *parent = nullptr);

    bool wifiAvailable() const;
    bool wifiEnabled() const;
    bool connected() const;
    QString connectionName() const;
    bool scanning() const;
    QVariantList networks() const;

    void setWifiEnabled(bool enabled);

    Q_INVOKABLE void requestScan();
    Q_INVOKABLE void connectNetwork(const QString &ssid, const QString &password = {}, bool persist = true);
    Q_INVOKABLE void disconnectCurrent();

Q_SIGNALS:
    void changed();

private:
    void refreshDevice();
    void bindDevice();
    void publishChanged();
    void setScanning(bool scanning);
    NetworkManager::Connection::Ptr savedConnectionForSsid(const QString &ssid) const;
    QString securityLabel(NetworkManager::WirelessSecurityType security) const;

    NetworkManager::WirelessDevice::Ptr m_wifiDevice;
    bool m_scanning = false;
};
