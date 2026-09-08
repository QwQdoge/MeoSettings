#pragma once

#include <QObject>

class LoginAuthBackend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString provider READ provider NOTIFY changed)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY changed)
    Q_PROPERTY(bool passwordFallbackAvailable READ passwordFallbackAvailable CONSTANT)
    Q_PROPERTY(bool loginFingerprintSupported READ loginFingerprintSupported NOTIFY changed)

public:
    explicit LoginAuthBackend(QObject *parent = nullptr);

    QString provider() const;
    QString providerLabel() const;
    bool passwordFallbackAvailable() const;
    bool loginFingerprintSupported() const;

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    QString m_provider;
};
