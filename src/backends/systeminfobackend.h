#pragma once

#include <QObject>
#include <QVariantList>

class SystemInfoBackend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList facts READ facts NOTIFY changed)
    Q_PROPERTY(QString userName READ userName NOTIFY changed)
    Q_PROPERTY(QString userAvatarSource READ userAvatarSource NOTIFY changed)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY changed)
    Q_PROPERTY(QString operatingSystemName READ operatingSystemName NOTIFY changed)

public:
    explicit SystemInfoBackend(QObject *parent = nullptr);

    QVariantList facts() const;
    QString userName() const;
    QString userAvatarSource() const;
    QString deviceName() const;
    QString operatingSystemName() const;

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    QVariantList m_facts;
    QString m_userName;
    QString m_userAvatarSource;
    QString m_deviceName;
    QString m_operatingSystemName;
};
