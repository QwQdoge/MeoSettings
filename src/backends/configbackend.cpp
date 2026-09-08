#include "configbackend.h"

#include <QFile>
#include <QFileInfo>

#include <utility>

namespace
{
constexpr auto kNvidiaConfig = "/etc/modprobe.d/meo-nvidia.conf";

QVariantMap config(QString id, QString title, QString path, QString documentation)
{
    return {{QStringLiteral("id"), std::move(id)}, {QStringLiteral("title"), std::move(title)},
            {QStringLiteral("path"), std::move(path)}, {QStringLiteral("managedBy"), QStringLiteral("Meo")},
            {QStringLiteral("documentation"), std::move(documentation)}};
}
}

ConfigBackend::ConfigBackend(QObject *parent)
    : BackendBase(parent)
    , m_configurations{config(QStringLiteral("nvidia.modprobe"), tr("NVIDIA"),
                              QString::fromLatin1(kNvidiaConfig),
                              QStringLiteral("https://wiki.archlinux.org/title/NVIDIA"))}
{
    setAvailable(true);
}

QVariantList ConfigBackend::configurations() const { return m_configurations; }

QVariantMap ConfigBackend::configuration(const QString &id) const
{
    for (const auto &entry : m_configurations) {
        const auto candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")) == id) {
            return candidate;
        }
    }
    return {};
}

QVariantMap ConfigBackend::read(const QString &id) const
{
    const auto item = configuration(id);
    if (item.isEmpty()) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("error"), tr("Unknown configuration provider.")}};
    }
    const auto path = item.value(QStringLiteral("path")).toString();
    QFile file(path);
    QString content;
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
    }
    auto result = item;
    result.insert(QStringLiteral("content"), content);
    result.insert(QStringLiteral("exists"), QFileInfo::exists(path));
    result.insert(QStringLiteral("valid"), true);
    return result;
}

QVariantMap ConfigBackend::validate(const QString &id, const QString &content) const
{
    const auto item = configuration(id);
    if (item.isEmpty()) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("error"), tr("Unknown configuration provider.")}};
    }
    if (content.contains(QChar::Null) || content.size() > 64 * 1024) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("error"), tr("Configuration text is not safe to submit.")}};
    }
    if (id == QLatin1String("nvidia.modprobe") && content.contains(QLatin1String("install "))) {
        return {{QStringLiteral("valid"), false}, {QStringLiteral("error"), tr("Meo-managed NVIDIA configuration cannot define executable install hooks.")}};
    }
    return {{QStringLiteral("valid"), true}, {QStringLiteral("error"), QString()}};
}

QVariantMap ConfigBackend::preview(const QString &id, const QString &content) const
{
    auto validation = validate(id, content);
    if (!validation.value(QStringLiteral("valid")).toBool()) {
        return validation;
    }
    const auto before = read(id).value(QStringLiteral("content")).toString();
    return {{QStringLiteral("valid"), true}, {QStringLiteral("id"), id},
            {QStringLiteral("before"), before}, {QStringLiteral("after"), content},
            {QStringLiteral("changed"), before != content},
            {QStringLiteral("requiresRecoveryPoint"), true},
            {QStringLiteral("requiresRestart"), id == QLatin1String("nvidia.modprobe")}};
}

void ConfigBackend::apply(const QString &id, const QString &content)
{
    const auto request = preview(id, content);
    if (!request.value(QStringLiteral("valid")).toBool()) {
        setError(request.value(QStringLiteral("error")).toString());
        return;
    }
    Q_EMIT transactionRequested(id, QStringLiteral("apply"), request);
}

void ConfigBackend::rollback(const QString &id)
{
    if (configuration(id).isEmpty()) {
        setError(tr("Unknown configuration provider."));
        return;
    }
    Q_EMIT transactionRequested(id, QStringLiteral("rollback"), {{QStringLiteral("id"), id}});
}

void ConfigBackend::resetToDefault(const QString &id)
{
    if (configuration(id).isEmpty()) {
        setError(tr("Unknown configuration provider."));
        return;
    }
    Q_EMIT transactionRequested(id, QStringLiteral("reset"), {{QStringLiteral("id"), id}});
}

QVariantMap ConfigBackend::getOrigin(const QString &id) const
{
    const auto item = configuration(id);
    if (item.isEmpty()) {
        return {};
    }
    return {{QStringLiteral("path"), item.value(QStringLiteral("path"))},
            {QStringLiteral("managedBy"), item.value(QStringLiteral("managedBy"))}};
}

QVariantMap ConfigBackend::getDocumentation(const QString &id) const
{
    const auto item = configuration(id);
    if (item.isEmpty()) {
        return {};
    }
    return {{QStringLiteral("url"), item.value(QStringLiteral("documentation"))},
            {QStringLiteral("privacy"), tr("Online documentation is loaded only after you request it.")}};
}
