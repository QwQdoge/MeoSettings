#pragma once

#include <QObject>

class BackendBase : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit BackendBase(QObject *parent = nullptr);

    bool available() const;
    bool busy() const;
    QString error() const;

    Q_INVOKABLE void clearError();

Q_SIGNALS:
    void availableChanged();
    void busyChanged();
    void errorChanged();

protected:
    void setAvailable(bool available);
    void setBusy(bool busy);
    void setError(const QString &error);

private:
    bool m_available = false;
    bool m_busy = false;
    QString m_error;
};
