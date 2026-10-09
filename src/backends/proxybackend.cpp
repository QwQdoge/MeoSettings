#include "proxybackend.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QRegularExpression>
#include <QUrl>

namespace {
const QStringList endpoints{QStringLiteral("httpProxy"), QStringLiteral("httpsProxy"), QStringLiteral("ftpProxy"), QStringLiteral("socksProxy")};
QString displayProxy(const QString &encoded)
{
    const auto separator = encoded.lastIndexOf(' ');
    if (separator < 0) return encoded;
    bool valid = false; const int port = encoded.mid(separator + 1).toInt(&valid);
    QUrl url(encoded.left(separator));
    if (!valid || port < 1 || port > 65535 || !url.isValid()) return encoded;
    url.setPort(port); return url.toString();
}
}

ProxyBackend::ProxyBackend(QObject *parent) : BackendBase(parent) { setAvailable(true); refresh(); }

void ProxyBackend::refresh()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kioslaverc")); config->reparseConfiguration();
    const auto group = config->group(QStringLiteral("Proxy Settings"));
    const int mode = group.readEntry("ProxyType", 0);
    m_configuration = {{"mode", mode}, {"pac", group.readEntry("Proxy Config Script", QString())},
        {"exceptions", group.readEntry("NoProxyFor", QString())}, {"reverse", group.readEntry("ReversedException", false)}};
    for (const auto &endpoint : endpoints) {
        const QString value = group.readEntry(endpoint, QString());
        m_configuration.insert(endpoint, mode == 1 ? displayProxy(value) : value);
    }
    Q_EMIT changed();
}

void ProxyBackend::apply(const QVariantMap &configuration)
{
    const QStringList allowed = endpoints + QStringList{"mode", "pac", "exceptions", "reverse"};
    for (auto it = configuration.begin(); it != configuration.end(); ++it)
        if (!allowed.contains(it.key())) { setError(tr("Unsupported proxy setting.")); return; }
    bool valid = false; const int mode = configuration.value("mode").toInt(&valid);
    if (!valid || mode < 0 || mode > 4 || configuration.value("reverse").metaType().id() != QMetaType::Bool) {
        setError(tr("Choose a supported proxy mode.")); return;
    }
    QVariantMap changes{{"ProxyType", mode}, {"ReversedException", configuration.value("reverse")}};
    bool hasEndpoint = false;
    if (mode == 1 || mode == 4) for (const auto &endpoint : endpoints) {
        const QString value = configuration.value(endpoint).toString().trimmed();
        if (value.isEmpty()) { changes.insert(endpoint, QString()); continue; }
        hasEndpoint = true;
        if (mode == 4) {
            if (!QRegularExpression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,127}$")).match(value).hasMatch()) { setError(tr("Enter an environment variable name for each system proxy.")); return; }
            changes.insert(endpoint, value); continue;
        }
        QUrl url(value, QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty() || url.port() < 1 || url.port() > 65535 || !url.userName().isEmpty()
            || !url.password().isEmpty() || url.hasQuery() || url.hasFragment() || (!url.path().isEmpty() && url.path() != "/")
            || !QStringList{"http", "https", "ftp", "socks", "socks5"}.contains(url.scheme()) || value.size() > 4096) {
            setError(tr("Enter a proxy URL with a host and port; credentials belong in the authentication prompt.")); return;
        }
        const int port = url.port(); url.setPort(-1); url.setPath(QString());
        changes.insert(endpoint, url.toString(QUrl::FullyEncoded) + ' ' + QString::number(port));
    }
    if ((mode == 1 || mode == 4) && !hasEndpoint) { setError(tr("Enter at least one proxy endpoint.")); return; }
    if (mode == 2) {
        const QUrl script(configuration.value("pac").toString(), QUrl::StrictMode);
        if (!script.isValid() || !QStringList{"http", "https", "file"}.contains(script.scheme()) || script.toString().size() > 4096
            || (!script.isLocalFile() && script.host().isEmpty()) || !script.userName().isEmpty() || !script.password().isEmpty()) {
            setError(tr("Enter a local, HTTP or HTTPS automatic proxy script URL.")); return;
        }
        changes.insert("Proxy Config Script", script.toString(QUrl::FullyEncoded));
    }
    const QString exceptions = configuration.value("exceptions").toString().trimmed();
    if (exceptions.size() > 4096 || exceptions.contains('\n') || exceptions.contains('\r')) { setError(tr("Proxy exceptions must be a single line.")); return; }
    changes.insert("NoProxyFor", exceptions);
    auto group = KSharedConfig::openConfig(QStringLiteral("kioslaverc"))->group(QStringLiteral("Proxy Settings"));
    for (auto it = changes.begin(); it != changes.end(); ++it) group.writeEntry(it.key(), it.value(), KConfig::Notify);
    if (!group.sync()) { setError(tr("The proxy configuration could not be saved.")); return; }
    auto signal = QDBusMessage::createSignal(QStringLiteral("/KIO/Scheduler"), QStringLiteral("org.kde.KIO.Scheduler"), QStringLiteral("reparseSlaveConfiguration"));
    signal << QString();
    if (!QDBusConnection::sessionBus().send(signal)) setError(tr("The proxy was saved. Restart running KDE applications to use the new configuration."));
    else clearError();
    refresh();
}
