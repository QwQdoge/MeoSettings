#pragma once

#include <QObject>
#include <QVariantList>

class RecoveryBackend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY changed)

public:
    explicit RecoveryBackend(QObject *parent = nullptr);

    QString mode() const;
    QString summary() const;
    QVariantList capabilities() const;

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    QString m_mode;
    QVariantList m_capabilities;
};
