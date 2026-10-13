#pragma once

#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QMetaType>
#include <QStringList>

#include <algorithm>

inline void MeoAccountBackend::saveLocalAiConnectionWithCapabilities(
    const QString &id,
    const QString &provider,
    const QString &displayName,
    const QString &endpoint,
    const QString &defaultModel,
    const QString &apiKey,
    const QVariantList &visionModels,
    const bool automaticInferenceEnabled)
{
    clearError();
    if (!m_serviceRunning) {
        const QString message = tr("Meo Account must be running to access the password wallet.");
        setError(message);
        Q_EMIT localAiConnectionSaveFailed(message);
        Q_EMIT changed();
        return;
    }

    if (visionModels.size() > 128) {
        const QString message = tr("Choose at most 128 image-capable AI models.");
        setError(message);
        Q_EMIT localAiConnectionSaveFailed(message);
        Q_EMIT changed();
        return;
    }

    QStringList normalizedVisionModels;
    normalizedVisionModels.reserve(visionModels.size());
    for (const QVariant &value : visionModels) {
        if (value.metaType().id() != QMetaType::QString) {
            const QString message = tr("The image-capable model list is invalid.");
            setError(message);
            Q_EMIT localAiConnectionSaveFailed(message);
            Q_EMIT changed();
            return;
        }
        const QString model = value.toString().trimmed();
        const bool invalid = model.isEmpty() || model.size() > 160
            || std::any_of(model.cbegin(), model.cend(), [](const QChar character) {
                   return character.isNull() || character.category() == QChar::Other_Control;
               });
        if (invalid) {
            const QString message = tr("The image-capable model list is invalid.");
            setError(message);
            Q_EMIT localAiConnectionSaveFailed(message);
            Q_EMIT changed();
            return;
        }
        if (!normalizedVisionModels.contains(model)) normalizedVisionModels.append(model);
    }
    normalizedVisionModels.sort(Qt::CaseSensitive);

    const QVariantMap connection{
        {QStringLiteral("id"), id},
        {QStringLiteral("provider"), provider},
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("endpoint"), endpoint},
        {QStringLiteral("defaultModel"), defaultModel},
        {QStringLiteral("visionModels"), normalizedVisionModels},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("automaticInferenceEnabled"), automaticInferenceEnabled},
    };

    QDBusInterface broker(QStringLiteral("org.meo.Accounts1"),
                          QStringLiteral("/org/meo/Accounts1"),
                          QStringLiteral("org.meo.Accounts1"),
                          QDBusConnection::sessionBus());
    if (!broker.isValid()) {
        const QString message = tr("The Meo Account service could not be reached.");
        setError(message);
        Q_EMIT localAiConnectionSaveFailed(message);
        Q_EMIT changed();
        return;
    }

    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("UpsertLocalAiConnection"), connection, apiKey), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QVariantMap> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    const QString message = tr("The local AI connection could not be saved to the password wallet.");
                    setError(message);
                    Q_EMIT localAiConnectionSaveFailed(message);
                    Q_EMIT changed();
                    return;
                }
                Q_EMIT localAiConnectionSaved(
                    reply.value().value(QStringLiteral("id")).toString());
                refreshLocalAiConnections();
            });
}
