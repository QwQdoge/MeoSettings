#pragma once

#include "../core/backendbase.h"

#include <QProcess>

// Owns only the per-user city preference and the explicit invocation of the
// bounded MeoKDE cache refresher.  The locker consumes the cache without a
// network request and never talks to this backend.
class WeatherBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QString city READ city NOTIFY changed)
    Q_PROPERTY(QString refresherPath READ refresherPath NOTIFY changed)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY changed)

public:
    explicit WeatherBackend(QObject *parent = nullptr);

    QString city() const;
    QString refresherPath() const;
    QString lastResult() const;

    static QString normalizeCity(const QString &city, QString *error = nullptr);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setCity(const QString &city);
    Q_INVOKABLE void refreshNow();

Q_SIGNALS:
    void changed();

private:
    QString m_city;
    QString m_refresherPath;
    QString m_lastResult;
    QProcess m_process;
};
