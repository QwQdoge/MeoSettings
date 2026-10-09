#include "portalpermissionsbackend.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <memory>

namespace {
const QString service = QStringLiteral("org.freedesktop.impl.portal.PermissionStore");
const QString path = QStringLiteral("/org/freedesktop/impl/portal/PermissionStore");
struct Resource { const char *key; const char *table; const char *id; bool canAsk; };
constexpr Resource resources[] = {{"camera", "devices", "camera", true}, {"microphone", "devices", "microphone", true},
    {"speakers", "devices", "speakers", true}, {"notifications", "notifications", "notification", false}, {"background", "background", "background", true}};
QDBusMessage request(const QString &method) { return QDBusMessage::createMethodCall(service, path, service, method); }
}

PortalPermissionsBackend::PortalPermissionsBackend(QObject *parent) : BackendBase(parent)
{
    qDBusRegisterMetaType<PortalPermissionMap>();
    auto *watcher = new QDBusServiceWatcher(service, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &PortalPermissionsBackend::refresh);
    refresh();
}

void PortalPermissionsBackend::refresh()
{
    if (busy()) return;
    const int generation = ++m_generation;
    setAvailable(QDBusConnection::sessionBus().interface()->isServiceRegistered(service));
    if (!available()) { m_permissions.clear(); Q_EMIT changed(); return; }
    const QStringList names{tr("Camera"), tr("Microphone"), tr("Speakers"), tr("Notifications"), tr("Background activity")};
    auto results = std::make_shared<QMap<int, QVariantList>>();
    auto remaining = std::make_shared<int>(int(std::size(resources)));
    for (int index = 0; index < int(std::size(resources)); ++index) {
        const Resource resource = resources[index];
        auto message = request(QStringLiteral("Lookup")); message << QString::fromLatin1(resource.table) << QString::fromLatin1(resource.id);
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 3000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, results, remaining, resource, index, names](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<PortalPermissionMap, QDBusVariant> reply = *call; call->deleteLater();
            if (generation != m_generation) return;
            if (!reply.isError()) {
                const auto grants = reply.argumentAt<0>();
                for (auto app = grants.begin(); app != grants.end(); ++app) {
                    if (app.key().isEmpty()) continue; // Empty application ID is a global policy, not an individual app.
                    QStringList choices{QStringLiteral("yes"), QStringLiteral("no")};
                    if (resource.canAsk) choices.append(QStringLiteral("ask"));
                    (*results)[index].append(QVariantMap{{"resource", QString::fromLatin1(resource.key)}, {"label", names[index]}, {"appId", app.key()},
                        {"decision", app.value().value(0)}, {"choices", choices}, {"editable", app.value().size() == 1 && choices.contains(app.value().first())}});
                }
            } else if (reply.error().name() != QStringLiteral("org.freedesktop.portal.Error.NotFound")) setError(reply.error().message());
            if (--*remaining == 0) {
                m_permissions.clear();
                for (const auto &rows : *results) m_permissions.append(rows);
                Q_EMIT changed();
            }
        });
    }
}

void PortalPermissionsBackend::setPermission(const QString &key, const QString &appId, const QString &decision)
{
    if (busy()) return;
    const Resource *resource = nullptr;
    for (const auto &candidate : resources) if (key == QLatin1String(candidate.key)) resource = &candidate;
    bool discovered = false;
    for (const auto &row : m_permissions) {
        const auto grant = row.toMap();
        if (grant.value("resource").toString() == key && grant.value("appId").toString() == appId && grant.value("editable").toBool()) discovered = true;
    }
    if (!resource || !discovered || (decision != "yes" && decision != "no" && !(resource->canAsk && decision == "ask"))) {
        setError(tr("Choose a supported decision for an existing application grant.")); return;
    }
    setBusy(true); clearError(); ++m_generation;
    auto message = request(QStringLiteral("SetPermission"));
    message << QString::fromLatin1(resource->table) << false << QString::fromLatin1(resource->id) << appId << QStringList{decision};
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<> result = *call;
        if (result.isError()) setError(result.error().message());
        call->deleteLater(); setBusy(false); refresh();
    });
}
