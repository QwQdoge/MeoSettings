#include "wallpaperbackend.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>

WallpaperBackend::WallpaperBackend(QObject *parent) : BackendBase(parent)
{
    refresh();
}

void WallpaperBackend::refresh()
{
    m_tool = QStandardPaths::findExecutable(QStringLiteral("plasma-apply-wallpaperimage"), {QStringLiteral("/usr/bin")});
    setAvailable(QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.plasmashell")));
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc")); config->reparseConfiguration();
    m_wallpapers.clear();
    const auto containments = config->group(QStringLiteral("Containments"));
    for (const auto &id : containments.groupList()) {
        const auto containment = containments.group(id);
        if (containment.readEntry("plugin", QString()) != "org.kde.plasma.desktop") continue;
        const auto image = containment.group("Wallpaper").group("org.kde.image").group("General");
        m_wallpapers.append(QVariantMap{{"id", id}, {"screen", containment.readEntry("lastScreen", -1)},
            {"image", image.readEntry("Image", QString())}, {"plugin", containment.readEntry("wallpaperplugin", QString())}});
    }
    m_images.clear();
    QSet<QString> seen;
    const QString managed = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/meo-settings/wallpapers");
    QStringList roots = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("wallpapers"), QStandardPaths::LocateDirectory);
    roots.prepend(managed);
    for (const QString &root : roots) {
        QDirIterator files(root, {"*.png", "*.jpg", "*.jpeg", "*.webp", "*.bmp"}, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (files.hasNext() && m_images.size() < 256) {
            files.next(); const QFileInfo file = files.fileInfo();
            const QString path = file.canonicalFilePath();
            if (path.isEmpty() || seen.contains(path) || file.size() > 100 * 1024 * 1024) continue;
            seen.insert(path);
            m_images.append(QVariantMap{{"image", QUrl::fromLocalFile(path)}, {"name", file.completeBaseName()},
                {"imported", QFileInfo(path).absolutePath() == QFileInfo(managed).canonicalFilePath()}});
        }
        if (m_images.size() >= 256) break;
    }
    Q_EMIT changed();
}

QString WallpaperBackend::validatedImage(const QUrl &url)
{
    const QFileInfo file(url.toLocalFile());
    const QString path = url.isLocalFile() ? file.canonicalFilePath() : QString();
    QImageReader reader(path);
    if (path.isEmpty() || !file.isFile() || !file.isReadable() || file.size() > 100 * 1024 * 1024 || !reader.canRead()) {
        setError(tr("Choose a readable local image smaller than 100 MiB.")); return {};
    }
    return path;
}

void WallpaperBackend::applyImage(const QUrl &url, const QString &fillMode, int screen)
{
    if (busy()) return;
    const QMap<QString, int> modes{{"stretch", 0}, {"preserveAspectFit", 1}, {"preserveAspectCrop", 2}, {"pad", 6}};
    bool screenFound = screen == -1;
    for (const auto &wallpaper : m_wallpapers) if (wallpaper.toMap().value("screen").toInt() == screen) screenFound = true;
    if (!available() || !modes.contains(fillMode) || !screenFound || screen < -1) {
        setError(tr("Choose an available desktop screen and supported wallpaper layout.")); return;
    }
    const QString path = validatedImage(url); if (path.isEmpty()) return;
    // This is the same public Plasma scripting authority used by the upstream
    // wallpaper tool. All script structure is fixed; the only string value is
    // a validated image encoded as a JSON literal, never raw script input.
    QByteArray literal = QJsonDocument(QJsonArray{QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded)}).toJson(QJsonDocument::Compact);
    literal = literal.mid(1, literal.size() - 2);
    const QString script = QStringLiteral("var count=0; var ds=desktops(); for(var i=0;i<ds.length;i++){var d=ds[i];"
        "if(%1===-1 || d.screen===%1){d.wallpaperPlugin='org.kde.image';"
        "d.currentConfigGroup=['Wallpaper','org.kde.image','General'];d.writeConfig('Image',%2);"
        "d.writeConfig('FillMode',%3); count++;}} if(count===0) throw new Error('Desktop screen unavailable');")
        .arg(screen).arg(QString::fromUtf8(literal)).arg(modes.value(fillMode));
    auto message = QDBusMessage::createMethodCall("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript");
    message << script; clearError(); setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 15000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished) {
        const QDBusPendingReply<QString> reply = *finished; finished->deleteLater(); setBusy(false);
        if (reply.isError()) setError(reply.error().message());
        else Q_EMIT imageApplied();
        // Plasma persists containment changes after the scripting call returns.
        QTimer::singleShot(300, this, &WallpaperBackend::refresh);
    });
}

QUrl WallpaperBackend::importImage(const QUrl &image)
{
    if (busy()) return {};
    const QString path = validatedImage(image); if (path.isEmpty()) return {};
    QFile source(path); if (!source.open(QIODevice::ReadOnly)) { setError(source.errorString()); return {}; }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&source)) { setError(tr("Cannot read this image.")); return {}; }
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/meo-settings/wallpapers";
    if (!QDir().mkpath(root)) { setError(tr("Cannot create the wallpaper library.")); return {}; }
    const QString target = root + '/' + QString::fromLatin1(hash.result().toHex()) + '.' + QFileInfo(path).suffix().toLower();
    if (!QFileInfo::exists(target) && !QFile::copy(path, target)) { setError(tr("Cannot import this wallpaper.")); return {}; }
    clearError(); refresh(); return QUrl::fromLocalFile(target);
}

void WallpaperBackend::removeImported(const QUrl &image)
{
    if (busy()) return;
    const QFileInfo file(image.toLocalFile());
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/meo-settings/wallpapers";
    if (!image.isLocalFile() || file.isSymLink() || file.canonicalFilePath().isEmpty()
        || file.absolutePath() != QFileInfo(root).canonicalFilePath()) { setError(tr("Only imported library images can be removed.")); return; }
    refresh();
    for (const auto &wallpaper : m_wallpapers) {
        const QUrl current(wallpaper.toMap().value("image").toString());
        if (QFileInfo(current.isLocalFile() ? current.toLocalFile() : current.toString()).canonicalFilePath() == file.canonicalFilePath()) {
            setError(tr("Choose another wallpaper before removing the current image.")); return;
        }
    }
    if (!QFile::remove(file.absoluteFilePath())) { setError(tr("Cannot remove this imported wallpaper.")); return; }
    clearError(); refresh();
}
