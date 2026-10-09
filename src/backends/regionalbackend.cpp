#include "regionalbackend.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QTimeZone>
#include <QSet>
#include <QLocale>
#include <QFile>
#include <QRegularExpression>
#include <locale.h>

RegionalBackend::RegionalBackend(QObject *parent) : BackendBase(parent)
{
    QSet<QString> seen;
    for (const auto &locale : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory)) {
        const QString id = locale.name();
        if (id == "C" || seen.contains(id)) continue;
        seen.insert(id);
        m_languages.append(QVariantMap{{"id", id}, {"label", locale.nativeLanguageName() + " · " + locale.nativeTerritoryName()}});
    }
    QFile generated(QStringLiteral("/etc/locale.gen"));
    if (generated.open(QIODevice::ReadOnly)) {
        while (!generated.atEnd()) {
            const QString line = QString::fromUtf8(generated.readLine()).trimmed();
            const auto match = QRegularExpression(QStringLiteral("^([A-Za-z_]+\\.UTF-8)\\s+UTF-8(?:\\s|$)")).match(line);
            if (!match.hasMatch()) continue;
            const QString id = match.captured(1);
            locale_t native = newlocale(LC_ALL_MASK, id.toUtf8().constData(), nullptr);
            if (!native) continue;
            freelocale(native);
            const QLocale locale(id.section('.', 0, 0));
            m_formats.append(QVariantMap{{"id", id}, {"label", locale.nativeLanguageName() + " · " + locale.nativeTerritoryName()}});
        }
    }
    refresh();
}

QStringList RegionalBackend::timezones() const
{
    QStringList result;
    for (const auto &id : QTimeZone::availableTimeZoneIds()) result.append(QString::fromUtf8(id));
    return result;
}

void RegionalBackend::refresh()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("plasma-localerc"));
    config->reparseConfiguration();
    const KConfigGroup translations(config, QStringLiteral("Translations"));
    const KConfigGroup formats(config, QStringLiteral("Formats"));
    m_preferences = {{"language", translations.readEntry("LANGUAGE", QString()).section(':', 0, 0)},
        {"format", formats.readEntry("LANG", QString())}};
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.timedate1"),
        QStringLiteral("/org/freedesktop/timedate1"), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message << QStringLiteral("org.freedesktop.timedate1");
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QVariantMap> result = *call;
        setAvailable(!result.isError());
        if (result.isError()) m_clock.clear(); else m_clock = result.value();
        call->deleteLater(); Q_EMIT changed();
    });
    Q_EMIT changed();
}

void RegionalBackend::setLanguage(const QString &locale)
{
    bool valid = locale.isEmpty();
    for (const auto &item : m_languages) if (item.toMap().value("id").toString() == locale) valid = true;
    if (!valid) { setError(tr("Choose a supported language.")); return; }
    clearError(); auto config = KSharedConfig::openConfig(QStringLiteral("plasma-localerc"));
    KConfigGroup group(config, QStringLiteral("Translations"));
    if (locale.isEmpty()) group.deleteEntry("LANGUAGE"); else group.writeEntry("LANGUAGE", locale);
    if (!group.sync()) { setError(tr("The language preference could not be saved.")); return; }
    m_restartRequired = true; refresh();
}

void RegionalBackend::setFormats(const QString &locale)
{
    bool valid = locale.isEmpty();
    for (const auto &item : m_formats) if (item.toMap().value("id").toString() == locale) valid = true;
    if (!valid) { setError(tr("This regional locale is not generated on the system.")); return; }
    clearError(); auto config = KSharedConfig::openConfig(QStringLiteral("plasma-localerc"));
    KConfigGroup group(config, QStringLiteral("Formats"));
    for (const auto &key : {"LANG", "LC_NUMERIC", "LC_TIME", "LC_MONETARY", "LC_MEASUREMENT", "LC_PAPER", "LC_ADDRESS", "LC_NAME", "LC_TELEPHONE"}) {
        if (locale.isEmpty()) group.deleteEntry(key); else group.writeEntry(key, locale);
    }
    if (!group.sync()) { setError(tr("Regional formats could not be saved.")); return; }
    m_restartRequired = true; refresh();
}

void RegionalBackend::timeRequest(const QString &method, const QVariantList &arguments)
{
    if (busy()) return;
    clearError(); setBusy(true);
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.timedate1"),
        QStringLiteral("/org/freedesktop/timedate1"), QStringLiteral("org.freedesktop.timedate1"), method);
    message.setArguments(arguments);
    // timedated owns Polkit authorization. No elevated settings process or
    // local success mirror exists; after completion re-read its properties.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 120000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<> result = *call;
        if (result.isError()) setError(result.error().message());
        setBusy(false); call->deleteLater(); refresh();
    });
}

void RegionalBackend::setTimezone(const QString &zone)
{
    if (!QTimeZone::isTimeZoneIdAvailable(zone.toUtf8())) { setError(tr("Invalid time zone.")); return; }
    timeRequest(QStringLiteral("SetTimezone"), {zone, true});
}

void RegionalBackend::setNetworkTime(bool enabled)
{
    if (!m_clock.value("CanNTP").toBool()) { setError(tr("Network time is unavailable.")); return; }
    timeRequest(QStringLiteral("SetNTP"), {enabled, true});
}

void RegionalBackend::setTime(const QString &isoDateTime)
{
    const QDateTime value = QDateTime::fromString(isoDateTime, Qt::ISODate);
    if (!value.isValid() || value.date().year() < 1970 || value.date().year() > 9999) {
        setError(tr("Enter a valid date and time.")); return;
    }
    if (m_clock.value("NTP").toBool()) { setError(tr("Turn off network time before setting the clock manually.")); return; }
    timeRequest(QStringLiteral("SetTime"), {QVariant::fromValue<qlonglong>(value.toMSecsSinceEpoch() * 1000), false, true});
}
