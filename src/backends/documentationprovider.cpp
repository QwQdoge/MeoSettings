#include "documentationprovider.h"

#include <QDesktopServices>
#include <QUrl>

DocumentationProvider::DocumentationProvider(QObject *parent)
    : QObject(parent)
{
}

QVariantMap DocumentationProvider::guide(const QString &id) const
{
    if (id == QLatin1String("nvidia")) {
        return {{QStringLiteral("localExplanation"), tr("NVIDIA driver changes require a restart and boot validation." )},
                {QStringLiteral("consequences"), tr("Meo creates a recovery point before a privileged driver transaction." )},
                {QStringLiteral("recommendedValue"), tr("Keep the current working driver until an update passes preflight." )},
                {QStringLiteral("originalUrl"), QStringLiteral("https://wiki.archlinux.org/title/NVIDIA")},
                {QStringLiteral("onlineCacheState"), QStringLiteral("not-loaded")}};
    }
    if (id == QLatin1String("systemd")) {
        return {{QStringLiteral("localExplanation"), tr("Use Meo-managed drop-ins instead of editing package-provided units." )},
                {QStringLiteral("consequences"), tr("Stopping critical services can end sessions or disconnect the network." )},
                {QStringLiteral("recommendedValue"), tr("Use structured controls in standard mode." )},
                {QStringLiteral("originalUrl"), QStringLiteral("https://wiki.archlinux.org/title/Systemd")},
                {QStringLiteral("onlineCacheState"), QStringLiteral("not-loaded")}};
    }
    return {};
}

bool DocumentationProvider::openOriginal(const QString &id) const
{
    const auto entry = guide(id);
    const auto url = entry.value(QStringLiteral("originalUrl")).toString();
    return !url.isEmpty() && QDesktopServices::openUrl(QUrl(url));
}
