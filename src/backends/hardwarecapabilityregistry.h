#pragma once

#include <QObject>
#include <QVariantList>

// Converts evidence from stable kernel interfaces into product-facing
// capabilities.  Consumers must use capability IDs, never DMI model strings.
class HardwareCapabilityRegistry final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit HardwareCapabilityRegistry(QObject *parent = nullptr);

    QVariantList capabilities() const;
    QString summary() const;

    Q_INVOKABLE QVariantMap capability(const QString &id) const;
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    QVariantList m_capabilities;
};
