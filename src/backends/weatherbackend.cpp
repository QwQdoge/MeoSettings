#include "weatherbackend.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace
{
constexpr auto kWeatherCityKey = "Weather/city";

QString processMessage(QProcess &process)
{
    const QString standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return standardError.isEmpty() ? QString::fromUtf8(process.readAllStandardOutput()).trimmed()
                                   : standardError;
}
}

WeatherBackend::WeatherBackend(QObject *parent)
    : BackendBase(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setError(m_process.errorString().trimmed().isEmpty()
                     ? tr("The Meo weather refresher could not start.")
                     : m_process.errorString().trimmed());
        setBusy(false);
        Q_EMIT changed();
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                const QString result = processMessage(m_process);
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    setError(result.isEmpty() ? tr("Weather could not refresh. Existing cached weather was kept.")
                                              : result);
                } else {
                    m_lastResult = tr("Weather cache refreshed.");
                    clearError();
                }
                setBusy(false);
                Q_EMIT changed();
            });
    refresh();
}

QString WeatherBackend::city() const { return m_city; }
QString WeatherBackend::refresherPath() const { return m_refresherPath; }
QString WeatherBackend::lastResult() const { return m_lastResult; }

QString WeatherBackend::normalizeCity(const QString &input, QString *error)
{
    if (error) {
        error->clear();
    }
    const QString city = input.simplified().left(96);
    if (city.isEmpty()) {
        if (error) *error = QObject::tr("Enter a city name.");
        return {};
    }
    if (city.contains(QChar::Null)) {
        if (error) *error = QObject::tr("The city name contains an invalid character.");
        return {};
    }
    return city;
}

void WeatherBackend::refresh()
{
    const QString city = QSettings(QStringLiteral("MeoArch"), QStringLiteral("MeoWeather"))
                             .value(QString::fromLatin1(kWeatherCityKey)).toString();
    m_city = normalizeCity(city);
    m_refresherPath = QStandardPaths::findExecutable(QStringLiteral("meo-weather-refresh"));
    setAvailable(!m_refresherPath.isEmpty());
    Q_EMIT changed();
}

void WeatherBackend::setCity(const QString &input)
{
    QString error;
    const QString city = normalizeCity(input, &error);
    if (city.isEmpty()) {
        setError(error);
        return;
    }
    QSettings preferences(QStringLiteral("MeoArch"), QStringLiteral("MeoWeather"));
    preferences.setValue(QString::fromLatin1(kWeatherCityKey), city);
    preferences.sync();
    m_city = city;
    clearError();
    Q_EMIT changed();
}

void WeatherBackend::refreshNow()
{
    if (busy()) {
        return;
    }
    if (m_city.isEmpty()) {
        setError(tr("Choose a city before refreshing weather."));
        return;
    }
    if (m_refresherPath.isEmpty()) {
        setError(tr("The installed Meo weather refresher is unavailable."));
        return;
    }
    clearError();
    setBusy(true);
    m_process.start(m_refresherPath, {QStringLiteral("--city"), m_city});
}
