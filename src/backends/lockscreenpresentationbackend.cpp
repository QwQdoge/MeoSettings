#include "lockscreenpresentationbackend.h"

#include "../core/sessionentrydocument.h"

#include <QMetaType>
#include <QSet>

namespace
{
QVariantMap nestedMap(const QVariantMap &parent, const QString &key)
{
    return parent.value(key).toMap();
}

void replaceNestedMap(QVariantMap *parent, const QString &key, QVariantMap child)
{
    parent->insert(key, std::move(child));
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

QVariantMap flatSettingsFromDocument(const QVariantMap &document)
{
    const QVariantMap modules = nestedMap(document, QStringLiteral("modules"));
    const QVariantMap privacy = nestedMap(document, QStringLiteral("privacy"));
    return {
        {QStringLiteral("showWeather"), modules.value(QStringLiteral("weather")).toBool()},
        {QStringLiteral("showWeatherLocation"),
         privacy.value(QStringLiteral("weatherLocation")).toString() != QLatin1String("hidden")},
        {QStringLiteral("showMediaControls"), modules.value(QStringLiteral("media")).toBool()},
        {QStringLiteral("showAlbumArtwork"), privacy.value(QStringLiteral("showAlbumArtwork")).toBool()},
        {QStringLiteral("showAudioControls"), modules.value(QStringLiteral("audio")).toBool()},
        {QStringLiteral("notificationVisibility"),
         privacy.value(QStringLiteral("notificationVisibility")).toString()},
    };
}

QVariantMap documentWithSettings(QVariantMap document, const QVariantMap &settings)
{
    QVariantMap modules = nestedMap(document, QStringLiteral("modules"));
    modules[QStringLiteral("weather")] = settings.value(QStringLiteral("showWeather")).toBool();
    modules[QStringLiteral("media")] = settings.value(QStringLiteral("showMediaControls")).toBool();
    modules[QStringLiteral("audio")] = settings.value(QStringLiteral("showAudioControls")).toBool();
    replaceNestedMap(&document, QStringLiteral("modules"), modules);

    QVariantMap privacy = nestedMap(document, QStringLiteral("privacy"));
    privacy[QStringLiteral("weatherLocation")] =
        settings.value(QStringLiteral("showWeatherLocation")).toBool()
            ? QStringLiteral("city") : QStringLiteral("hidden");
    privacy[QStringLiteral("showAlbumArtwork")] =
        settings.value(QStringLiteral("showAlbumArtwork")).toBool();
    privacy[QStringLiteral("notificationVisibility")] =
        settings.value(QStringLiteral("notificationVisibility")).toString();
    replaceNestedMap(&document, QStringLiteral("privacy"), privacy);

    return document;
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
    return flatSettingsFromDocument(
        SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen));
}

QVariantMap LockScreenPresentationBackend::normalized(const QVariantMap &input, QString *error)
{
    if (error) {
        error->clear();
    }

    const QVariantMap fallback = defaults();
    const QSet<QString> allowedKeys{
        QStringLiteral("showWeather"),
        QStringLiteral("showWeatherLocation"),
        QStringLiteral("showMediaControls"),
        QStringLiteral("showAlbumArtwork"),
        QStringLiteral("showAudioControls"),
        QStringLiteral("notificationVisibility"),
    };
    for (auto it = input.constBegin(); it != input.constEnd(); ++it) {
        if (!allowedKeys.contains(it.key())) {
            if (error) {
                *error = QObject::tr("Unsupported lock-screen presentation setting: %1").arg(it.key());
            }
            return {};
        }
    }

    QVariantMap result;
    for (const QString &key : {
             QStringLiteral("showWeather"),
             QStringLiteral("showWeatherLocation"),
             QStringLiteral("showMediaControls"),
             QStringLiteral("showAlbumArtwork"),
             QStringLiteral("showAudioControls"),
         }) {
        if (!input.contains(key)) {
            result.insert(key, fallback.value(key));
            continue;
        }

        const QVariant candidate = input.value(key);
        if (candidate.metaType().id() != QMetaType::Bool) {
            if (error) {
                *error = QObject::tr("Lock-screen presentation toggles must be true or false.");
            }
            return {};
        }
        result.insert(key, candidate.toBool());
    }

    // Dependent privacy choices are enforced below QML so every caller reaches
    // the same safe document state.
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
    const auto loaded = m_store.load();
    if (!loaded.ok) {
        setError(loaded.error);
        return;
    }

    const QVariantMap flat = flatSettingsFromDocument(loaded.document);
    QString error;
    const QVariantMap sanitized = normalized(flat, &error);
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

    // Preserve appearance/layout/motion and any future validated fields owned
    // by the v1 document. The Settings page only projects the subset it owns.
    const auto loaded = m_store.load();
    if (!loaded.ok) {
        setError(loaded.error);
        return;
    }
    QVariantMap document = documentWithSettings(loaded.document, values);
    const auto validated = SessionEntryDocument::validate(
        document, SessionEntryDocument::Scope::LockScreen);
    if (!validated.ok) {
        setError(validated.path + QStringLiteral(": ") + validated.error);
        return;
    }
    if (!m_store.save(validated.document, &error)) {
        setError(error);
        return;
    }

    m_settings = values;
    clearError();
    Q_EMIT changed();
    Q_EMIT saved();
}

void LockScreenPresentationBackend::resetToDefaults()
{
    QString error;
    if (!m_store.resetToDefaults(&error)) {
        setError(error);
        return;
    }
    refresh();
    if (this->error().isEmpty()) {
        Q_EMIT saved();
    }
}
