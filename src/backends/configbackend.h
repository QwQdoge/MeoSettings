#pragma once

#include "../core/backendbase.h"

#include <QVariantList>
#include <QVariantMap>

// A safe client contract for configuration providers.  It never writes a
// system file itself: privileged mutations are delegated to SystemTransaction1.
class ConfigBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList configurations READ configurations NOTIFY changed)

public:
    explicit ConfigBackend(QObject *parent = nullptr);

    QVariantList configurations() const;
    Q_INVOKABLE QVariantMap read(const QString &id) const;
    Q_INVOKABLE QVariantMap validate(const QString &id, const QString &content) const;
    Q_INVOKABLE QVariantMap preview(const QString &id, const QString &content) const;
    Q_INVOKABLE void apply(const QString &id, const QString &content);
    Q_INVOKABLE void rollback(const QString &id);
    Q_INVOKABLE void resetToDefault(const QString &id);
    Q_INVOKABLE QVariantMap getOrigin(const QString &id) const;
    Q_INVOKABLE QVariantMap getDocumentation(const QString &id) const;

Q_SIGNALS:
    void changed();
    void transactionRequested(QString configurationId, QString operation, QVariantMap payload);

private:
    QVariantMap configuration(const QString &id) const;
    QVariantList m_configurations;
};
