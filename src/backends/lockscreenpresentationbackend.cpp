#include "lockscreenpresentationbackend.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace
{
constexpr auto kConfigFile = "kscreenlockerrc";

KConfigGroup presentationGroup()
{
    const auto config = KSharedConfig::openConfig(QString::fromLatin1(kConfigFile));
    return config->group(QStringLiteral("Greeter")).group(QStringLiteral("LnF"));
}

bool readBool(const KConfigGroup &group, const char *key, bool fallback)
{
    return group.readEntry(QString::fromLatin1(key), fallback);
}

QString normalizedPrivacy(const QVariant &value, QString *error)
{
    const QString privacy = value.toString().trimmed();
    if (privacy == QLatin1String("hidden")
        || privacy == QLatin1String("count")
        || privacy == QLatin1String("app-name")
        || privacy == QLatin1String("full-content")) {
        return privacy;
    }

    if (error) {
        *error = QObject::tr("Notification visibility must be hidden, count, app-name, or full-content.");
    }
    return {};
}
}

LockScreenPresentationBackend::LockScreenPresentationBackend(QObject *parent)
    : BackendBase(parent)
{
    setAvailable(true);
    refresh();
}

QVariantMap LockScreenPresentationBackend::settings() const
{
    return m_settings;
}

QVariantMap LockScreenPresentationBackend::defaults()
{
    return {
        {QStringLiteral("showWeather"), true},
        {QStringLiteral("showWeatherLocation"), false},
        {QStringLiteral("showMediaControls"), true},
        {QStringLiteral("showAlbumArtwork"), true},
        {QStringLiteral("showAudioControls"), true},
        {QStringLiteral("showPerformance"), true},
        {QStringLiteral("showSystemSummary"), true},
        {QStringLiteral("showSessionControls"), true},
        {QStringLiteral("notificationVisibility"), QStringLiteral("count")},
    };
}

QVariantMap LockScreenPresentationBackend::normalized(const QVariantMap &input, QString *error)
{
    if (error) {
        error->clear();
    }

    const QVariantMap fallback = defaults();
    QVariantMap result;
    for (const QString &key : {
             QStringLiteral("showWeather"),
             QStringLiteral("showWeatherLocation"),
             QStringLiteral("showMediaControls"),
             QStringLiteral("showAlbumArtwork"),
             QStringLiteral("showAudioControls"),
             QStringLiteral("showPerformance"),
             QStringLiteral("showSystemSummary"),
             QStringLiteral("showSessionControls"),
         }) {
        result.insert(key, input.contains(key) ? input.value(key).toBool() : fallback.value(key));
    }

    // Dependent privacy toggles are normalized here rather than in QML so
    // every caller preserves the same contract.
    if (!result.value(QStringLiteral("showWeather")).toBool()) {
        result[QStringLiteral("showWeatherLocation")] = false;
    }
    if (!result.value(QStringLiteral("showMediaControls")).toBool()) {
        result[QStringLiteral("showAlbumArtwork")] = false;
    }

    const QVariant privacyValue = input.contains(QStringLiteral("notificationVisibility"))
        ? input.value(QStringLiteral("notificationVisibility"))
        : fallback.value(QStringLiteral("notificationVisibility"));
    const QString privacy = normalizedPrivacy(privacyValue, error);
    if (privacy.isEmpty()) {
        return {};
    }
    result.insert(QStringLiteral("notificationVisibility"), privacy);
    return result;
}

void LockScreenPresentationBackend::refresh()
{
    const KConfigGroup group = presentationGroup();
    QVariantMap loaded = defaults();
    loaded[QStringLiteral("showWeather")] =
        readBool(group, "showWeather", loaded.value(QStringLiteral("showWeather")).toBool());
    loaded[QStringLiteral("showWeatherLocation")] =
        readBool(group, "showWeatherLocation", loaded.value(QStringLiteral("showWeatherLocation")).toBool());
    loaded[QStringLiteral("showMediaControls")] =
        readBool(group, "showMediaControls", loaded.value(QStringLiteral("showMediaControls")).toBool());
    loaded[QStringLiteral("showAlbumArtwork")] =
        readBool(group, "showAlbumArtwork", loaded.value(QStringLiteral("showAlbumArtwork")).toBool());
    loaded[QStringLiteral("showAudioControls")] =
        readBool(group, "showAudioControls", loaded.value(QStringLiteral("showAudioControls")).toBool());
    loaded[QStringLiteral("showPerformance")] =
        readBool(group, "showPerformance", loaded.value(QStringLiteral("showPerformance")).toBool());
    loaded[QStringLiteral("showSystemSummary")] =
        readBool(group, "showSystemSummary", loaded.value(QStringLiteral("showSystemSummary")).toBool());
    loaded[QStringLiteral("showSessionControls")] =
        readBool(group, "showSessionControls", loaded.value(QStringLiteral("showSessionControls")).toBool());
    loaded[QStringLiteral("notificationVisibility")] =
        group.readEntry(QStringLiteral("lockScreenNotificationVisibility"),
                        loaded.value(QStringLiteral("notificationVisibility")).toString());

    QString error;
    const QVariantMap sanitized = normalized(loaded, &error);
    if (sanitized.isEmpty()) {
        setError(error);
        return;
    }

    clearError();
    if (m_settings == sanitized) {
        return;
    }
    m_settings = sanitized;
    Q_EMIT changed();
}

void LockScreenPresentationBackend::save(const QVariantMap &input)
{
    QString error;
    const QVariantMap values = normalized(input, &error);
    if (values.isEmpty()) {
        setError(error);
        return;
    }

    KConfigGroup group = presentationGroup();
    group.writeEntry(QStringLiteral("showWeather"), values.value(QStringLiteral("showWeather")).toBool());
    group.writeEntry(QStringLiteral("showWeatherLocation"), values.value(QStringLiteral("showWeatherLocation")).toBool());
    group.writeEntry(QStringLiteral("showMediaControls"), values.value(QStringLiteral("showMediaControls")).toBool());
    group.writeEntry(QStringLiteral("showAlbumArtwork"), values.value(QStringLiteral("showAlbumArtwork")).toBool());
    group.writeEntry(QStringLiteral("showAudioControls"), values.value(QStringLiteral("showAudioControls")).toBool());
    group.writeEntry(QStringLiteral("showPerformance"), values.value(QStringLiteral("showPerformance")).toBool());
    group.writeEntry(QStringLiteral("showSystemSummary"), values.value(QStringLiteral("showSystemSummary")).toBool());
    group.writeEntry(QStringLiteral("showSessionControls"), values.value(QStringLiteral("showSessionControls")).toBool());
    group.writeEntry(QStringLiteral("lockScreenNotificationVisibility"),
                     values.value(QStringLiteral("notificationVisibility")).toString());
    group.sync();

    m_settings = values;
    clearError();
    Q_EMIT changed();
    Q_EMIT saved();
}

void LockScreenPresentationBackend::resetToDefaults()
{
    KConfigGroup group = presentationGroup();
    for (const QString &key : {
             QStringLiteral("showWeather"),
             QStringLiteral("showWeatherLocation"),
             QStringLiteral("showMediaControls"),
             QStringLiteral("showAlbumArtwork"),
             QStringLiteral("showAudioControls"),
             QStringLiteral("showPerformance"),
             QStringLiteral("showSystemSummary"),
             QStringLiteral("showSessionControls"),
             QStringLiteral("lockScreenNotificationVisibility"),
         }) {
        group.deleteEntry(key);
    }
    group.sync();
    refresh();
    Q_EMIT saved();
}
