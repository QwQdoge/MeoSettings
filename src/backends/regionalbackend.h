#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QVariantMap>

class RegionalBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY changed)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QVariantList formats READ formats CONSTANT)
    Q_PROPERTY(QStringList timezones READ timezones CONSTANT)
    Q_PROPERTY(QVariantMap clock READ clock NOTIFY changed)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY changed)
public:
    explicit RegionalBackend(QObject *parent = nullptr);
    QVariantMap preferences() const { return m_preferences; }
    QVariantMap clock() const { return m_clock; }
    QVariantList languages() const { return m_languages; }
    QVariantList formats() const { return m_formats; }
    QStringList timezones() const;
    bool restartRequired() const { return m_restartRequired; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setLanguage(const QString &locale);
    Q_INVOKABLE void setFormats(const QString &locale);
    Q_INVOKABLE void setTimezone(const QString &timezone);
    Q_INVOKABLE void setNetworkTime(bool enabled);
    Q_INVOKABLE void setTime(const QString &isoDateTime);
Q_SIGNALS:
    void changed();
private:
    void timeRequest(const QString &method, const QVariantList &arguments);
    QVariantMap m_preferences, m_clock;
    QVariantList m_languages, m_formats;
    bool m_restartRequired = false;
};
