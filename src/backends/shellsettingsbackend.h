#pragma once

#include "../core/backendbase.h"

#include <QVariantMap>

class ShellSettingsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap shelf READ shelf NOTIFY changed)
    Q_PROPERTY(QVariantMap notifications READ notifications NOTIFY changed)
    Q_PROPERTY(QVariantMap timeCenter READ timeCenter NOTIFY changed)
    Q_PROPERTY(QVariantMap topTasks READ topTasks NOTIFY changed)

public:
    explicit ShellSettingsBackend(QObject *parent = nullptr);

    QVariantMap shelf() const;
    QVariantMap notifications() const;
    QVariantMap timeCenter() const;
    QVariantMap topTasks() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void saveShelf(const QVariantMap &settings);
    Q_INVOKABLE void saveNotifications(const QVariantMap &settings);
    Q_INVOKABLE void saveTimeCenter(const QVariantMap &settings);
    Q_INVOKABLE void saveTopTasks(const QVariantMap &settings);
    Q_INVOKABLE void resetShelf();
    Q_INVOKABLE void resetNotifications();
    Q_INVOKABLE void resetTimeCenter();
    Q_INVOKABLE void resetTopTasks();

    static QVariantMap normalizedShelf(const QVariantMap &settings);
    static QVariantMap normalizedNotifications(const QVariantMap &settings);
    static QVariantMap normalizedTimeCenter(const QVariantMap &settings);
    static QVariantMap normalizedTopTasks(const QVariantMap &settings);

    static QVariantMap serializeShelf(const QVariantMap &settings, QString *error = nullptr);
    static QVariantMap serializeNotifications(const QVariantMap &settings, QString *error = nullptr);
    static QVariantMap serializeTimeCenter(const QVariantMap &settings, QString *error = nullptr);
    static QVariantMap serializeTopTasks(const QVariantMap &settings, QString *error = nullptr);

    static QString readScript();
    static QString writeShelfScript(const QVariantMap &settings);
    static QString writeNotificationsScript(const QVariantMap &settings);
    static QString writeTimeCenterScript(const QVariantMap &settings);
    static QString writeTopTasksScript(const QVariantMap &settings);

Q_SIGNALS:
    void changed();
    void shelfSaved();
    void notificationsSaved();
    void timeCenterSaved();
    void topTasksSaved();

private:
    void saveSurface(const QString &surface, const QVariantMap &settings);
    void setSurface(const QString &surface, const QVariantMap &settings, int count);
    QString errorForScriptReason(const QString &surface, const QString &reason) const;

    QVariantMap m_shelf;
    QVariantMap m_notifications;
    QVariantMap m_timeCenter;
    QVariantMap m_topTasks;
};
