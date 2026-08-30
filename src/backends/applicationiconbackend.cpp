#include "applicationiconbackend.h"

#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUuid>
#include <QUrl>

#include <utility>

namespace
{
constexpr auto defaultPrompt =
    "Preserve the application's recognizable original identity, silhouette, internal cuts, and key visual features. "
    "Create one centered Pixel / Material You app icon on a transparent canvas with a single container and a unified "
    "wallpaper-derived Monet palette. Keep it readable at small sizes. No words, watermark, device mockup, screenshot, "
    "perspective, extra logo, second badge, or second background plate. Mild Easel-like material texture is allowed.";

QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/meo-icon-studio/manifest.json");
}

QString processMessage(QProcess &process)
{
    const QString error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!error.isEmpty()) {
        return error;
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}
}

ApplicationIconBackend::ApplicationIconBackend(QObject *parent)
    : BackendBase(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError) {
                m_aiImageFile.reset();
                setError(m_process.errorString().trimmed().isEmpty()
                             ? tr("Meo application icon studio could not start.")
                             : m_process.errorString().trimmed());
                setBusy(false);
                Q_EMIT changed();
            });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                const QString message = processMessage(m_process);
                m_aiImageFile.reset();
                const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
                if (!succeeded) {
                    setError(message.isEmpty()
                                 ? tr("Meo application icons could not be updated.")
                                 : message);
                } else {
                    m_lastResult = message.isEmpty()
                        ? tr("Application icons were updated.") : message;
                    clearError();
                    refresh();
                }
                if (m_applyingAiBatch && succeeded) {
                    m_aiBatchDirectory.reset();
                    m_aiBatchId.clear();
                    m_aiBatchPreviews.clear();
                }
                m_applyingAiBatch = false;
                setBusy(false);
                Q_EMIT changed();
            });
    refresh();
}

ApplicationIconBackend::~ApplicationIconBackend() = default;

QString ApplicationIconBackend::toolPath() const { return m_toolPath; }
QString ApplicationIconBackend::style() const { return m_style; }
QString ApplicationIconBackend::shape() const { return m_shape; }
QString ApplicationIconBackend::prompt() const { return m_prompt; }
QVariantList ApplicationIconBackend::applications() const { return m_applications; }
QString ApplicationIconBackend::lastResult() const { return m_lastResult; }
QString ApplicationIconBackend::aiBatchId() const { return m_aiBatchId; }
QVariantList ApplicationIconBackend::aiBatchPreviews() const { return m_aiBatchPreviews; }

bool ApplicationIconBackend::isSupportedStyle(const QString &value)
{
    return value == QLatin1String("monet") || value == QLatin1String("original")
        || value == QLatin1String("pure")
        || value == QLatin1String("mono");
}

bool ApplicationIconBackend::isSupportedShape(const QString &value)
{
    return value == QLatin1String("pixel") || value == QLatin1String("circle")
        || value == QLatin1String("squircle") || value == QLatin1String("rounded");
}

QStringList ApplicationIconBackend::applyArguments(const QString &requestedStyle,
                                                   const QString &requestedShape,
                                                   const QString &requestedPrompt,
                                                   QString *error,
                                                   const QStringList &applicationIds)
{
    if (error) {
        error->clear();
    }
    QString style = requestedStyle.trimmed().toLower();
    if (style == QLatin1String("pure")) {
        style = QStringLiteral("monet");
    }
    const QString shape = requestedShape.trimmed().toLower();
    const QString prompt = requestedPrompt.trimmed();
    if (!isSupportedStyle(style)) {
        if (error) {
            *error = QObject::tr("Choose Monet, Original, or Black & white.");
        }
        return {};
    }
    if (!isSupportedShape(shape)) {
        if (error) {
            *error = QObject::tr("Choose Pixel flower, Circle, Squircle, or Rounded square.");
        }
        return {};
    }
    if (prompt.isEmpty() || prompt.size() > 4000 || prompt.contains(QChar::Null)) {
        if (error) {
            *error = QObject::tr("The icon prompt must contain 1 to 4000 characters.");
        }
        return {};
    }
    QStringList arguments{QStringLiteral("--apply")};
    for (const QString &rawId : applicationIds) {
        const QString id = rawId.trimmed();
        if (id.isEmpty() || id.size() > 512 || id.contains(QChar::Null)
            || id.contains(QLatin1Char('\n')) || id.contains(QLatin1Char('\r'))) {
            if (error) {
                *error = QObject::tr("An application identifier is invalid.");
            }
            return {};
        }
        arguments << QStringLiteral("--app") << id;
    }
    arguments << QStringLiteral("--style") << style
              << QStringLiteral("--shape") << shape
              << QStringLiteral("--prompt") << prompt;
    return arguments;
}

void ApplicationIconBackend::refresh()
{
    const QString nextPath = QStandardPaths::findExecutable(QStringLiteral("meo-app-icon-studio"));
    QString nextStyle = QStringLiteral("monet");
    QString nextShape = QStringLiteral("circle");
    QString nextPrompt = QString::fromUtf8(defaultPrompt);
    QFile stored(configPath());
    if (stored.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(stored.readAll()).object();
        QString candidateStyle = object.value(QStringLiteral("style")).toString();
        if (candidateStyle == QLatin1String("pure")) {
            candidateStyle = QStringLiteral("monet");
        }
        const QString candidateShape = object.value(QStringLiteral("shape")).toString();
        const QString candidatePrompt = object.value(QStringLiteral("prompt")).toString();
        if (isSupportedStyle(candidateStyle)) {
            nextStyle = candidateStyle;
        }
        if (isSupportedShape(candidateShape)) {
            nextShape = candidateShape;
        }
        if (!candidatePrompt.trimmed().isEmpty() && candidatePrompt.size() <= 4000) {
            nextPrompt = candidatePrompt;
        }
    }
    bool stateChanged = m_toolPath != nextPath || m_style != nextStyle
        || m_shape != nextShape || m_prompt != nextPrompt;
    m_toolPath = nextPath;
    m_style = nextStyle;
    m_shape = nextShape;
    m_prompt = nextPrompt;
    QVariantList nextApplications;
    if (!nextPath.isEmpty()) {
        QProcess listing;
        listing.start(nextPath, {QStringLiteral("--list")});
        if (listing.waitForFinished(5000) && listing.exitStatus() == QProcess::NormalExit
            && listing.exitCode() == 0) {
            const QJsonArray array = QJsonDocument::fromJson(listing.readAllStandardOutput()).array();
            for (const QJsonValue &value : array) {
                const QJsonObject object = value.toObject();
                nextApplications.append(QVariantMap{
                    {QStringLiteral("desktopId"), object.value(QStringLiteral("desktop_id")).toString()},
                    {QStringLiteral("name"), object.value(QStringLiteral("name")).toString()},
                    {QStringLiteral("icon"), object.value(QStringLiteral("icon")).toString()},
                });
            }
        }
    }
    if (nextApplications != m_applications) {
        m_applications = nextApplications;
        stateChanged = true;
    }
    setAvailable(!m_toolPath.isEmpty());
    if (stateChanged) {
        Q_EMIT changed();
    }
}

void ApplicationIconBackend::start(const QStringList &arguments, const QString &successMessage)
{
    clearError();
    if (busy()) {
        return;
    }
    if (m_toolPath.isEmpty()) {
        setError(tr("The installed Meo application icon studio is unavailable."));
        return;
    }
    m_lastResult = successMessage;
    setBusy(true);
    m_process.start(m_toolPath, arguments);
}

void ApplicationIconBackend::apply(const QString &requestedStyle, const QString &requestedShape,
                                   const QString &requestedPrompt)
{
    QString error;
    const QStringList arguments = applyArguments(requestedStyle, requestedShape, requestedPrompt, &error);
    if (arguments.isEmpty()) {
        setError(error);
        return;
    }
    start(arguments, tr("Updating application icons…"));
}

void ApplicationIconBackend::applyToApplications(const QStringList &applicationIds,
                                                  const QString &requestedStyle,
                                                  const QString &requestedShape,
                                                  const QString &requestedPrompt)
{
    if (applicationIds.isEmpty()) {
        setError(tr("Choose at least one application."));
        return;
    }
    QString error;
    const QStringList arguments = applyArguments(requestedStyle, requestedShape,
                                                 requestedPrompt, &error, applicationIds);
    if (arguments.isEmpty()) {
        setError(error);
        return;
    }
    start(arguments, tr("Updating selected application icons…"));
}

void ApplicationIconBackend::applyAiImage(const QString &applicationId,
                                          const QString &imageSource,
                                          const QString &requestedShape,
                                          const QString &requestedPrompt)
{
    if (busy()) return;
    const QString id = applicationId.trimmed();
    const QString shape = requestedShape.trimmed().toLower();
    const QString prompt = requestedPrompt.trimmed();
    constexpr auto prefix = "data:image/png;base64,";
    if (id.isEmpty() || id.size() > 512 || id.contains(QChar::Null)
        || !isSupportedShape(shape) || prompt.isEmpty() || prompt.size() > 4000
        || !imageSource.startsWith(QLatin1String(prefix))) {
        setError(tr("The generated icon request is invalid."));
        return;
    }
    const QByteArray bytes = QByteArray::fromBase64(
        imageSource.mid(QLatin1String(prefix).size()).toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty() || bytes.size() > 12'000'000 || QImage::fromData(bytes).isNull()) {
        setError(tr("Meo Account returned an invalid generated image."));
        return;
    }
    auto temporary = std::make_unique<QTemporaryFile>();
    temporary->setAutoRemove(true);
    if (!temporary->open() || temporary->write(bytes) != bytes.size() || !temporary->flush()) {
        setError(tr("The generated image could not be prepared locally."));
        return;
    }
    const QString path = temporary->fileName();
    m_aiImageFile = std::move(temporary);
    start({QStringLiteral("--apply"), QStringLiteral("--app"), id,
           QStringLiteral("--ai-image"), path,
           QStringLiteral("--shape"), shape,
           QStringLiteral("--prompt"), prompt},
          tr("Applying the generated application icon…"));
}

bool ApplicationIconBackend::beginAiBatch()
{
    if (busy()) return false;
    auto directory = std::make_unique<QTemporaryDir>(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/meo-ai-icon-pack-XXXXXX"));
    if (!directory->isValid()) {
        setError(tr("A private AI icon staging directory could not be created."));
        return false;
    }
    directory->setAutoRemove(true);
    m_aiBatchDirectory = std::move(directory);
    m_aiBatchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_aiBatchPreviews.clear();
    clearError();
    Q_EMIT changed();
    return true;
}

bool ApplicationIconBackend::stageAiImage(const QString &applicationId,
                                          const QString &applicationName,
                                          const QString &imageSource,
                                          const QString &requestedShape,
                                          const QString &requestedPrompt)
{
    if (busy() || !m_aiBatchDirectory || !m_aiBatchDirectory->isValid()) return false;
    const QString id = applicationId.trimmed();
    const QString name = applicationName.trimmed();
    const QString shape = requestedShape.trimmed().toLower();
    const QString prompt = requestedPrompt.trimmed();
    constexpr auto prefix = "data:image/png;base64,";
    if (id.isEmpty() || id.size() > 512 || name.isEmpty() || name.size() > 256
        || !isSupportedShape(shape) || prompt.isEmpty() || prompt.size() > 4000
        || !imageSource.startsWith(QLatin1String(prefix))) {
        setError(tr("A generated AI icon pack item is invalid."));
        return false;
    }
    for (const QVariant &existing : std::as_const(m_aiBatchPreviews)) {
        if (existing.toMap().value(QStringLiteral("desktopId")).toString() == id) {
            setError(tr("The AI icon pack contains the same application twice."));
            return false;
        }
    }
    const QByteArray bytes = QByteArray::fromBase64(
        imageSource.mid(QLatin1String(prefix).size()).toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty() || bytes.size() > 12'000'000 || QImage::fromData(bytes).isNull()) {
        setError(tr("Meo Account returned an invalid generated image."));
        return false;
    }
    const QString fileName = QStringLiteral("icon-%1.png").arg(m_aiBatchPreviews.size());
    QFile output(m_aiBatchDirectory->filePath(fileName));
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size()
        || !output.flush()) {
        setError(tr("A generated icon could not be staged locally."));
        return false;
    }
    output.close();
    m_aiBatchPreviews.append(QVariantMap{
        {QStringLiteral("desktopId"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("preview"), QUrl::fromLocalFile(output.fileName()).toString()},
        {QStringLiteral("image"), fileName},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("prompt"), prompt},
    });
    clearError();
    Q_EMIT changed();
    return true;
}

void ApplicationIconBackend::applyAiBatch()
{
    if (busy() || !m_aiBatchDirectory || m_aiBatchPreviews.isEmpty()) return;
    QJsonArray items;
    for (const QVariant &value : std::as_const(m_aiBatchPreviews)) {
        const QVariantMap item = value.toMap();
        items.append(QJsonObject{
            {QStringLiteral("desktopId"), item.value(QStringLiteral("desktopId")).toString()},
            {QStringLiteral("image"), item.value(QStringLiteral("image")).toString()},
            {QStringLiteral("shape"), item.value(QStringLiteral("shape")).toString()},
            {QStringLiteral("prompt"), item.value(QStringLiteral("prompt")).toString()},
        });
    }
    const QString prompt = m_aiBatchPreviews.first().toMap()
                               .value(QStringLiteral("prompt")).toString();
    const QByteArray promptHash = QCryptographicHash::hash(
        prompt.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QJsonObject payload{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("packId"), m_aiBatchId},
        {QStringLiteral("stylePack"), QStringLiteral("easel-monet")},
        {QStringLiteral("promptHash"), QString::fromLatin1(promptHash)},
        {QStringLiteral("items"), items},
    };
    const QString manifestPath = m_aiBatchDirectory->filePath(QStringLiteral("pack.json"));
    QFile manifest(manifestPath);
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifest.write(QJsonDocument(payload).toJson(QJsonDocument::Indented)) < 1
        || !manifest.flush()) {
        setError(tr("The staged AI icon pack manifest could not be written."));
        return;
    }
    manifest.close();
    start({QStringLiteral("--apply"), QStringLiteral("--ai-pack"), manifestPath},
          tr("Applying the complete AI application icon pack…"));
    m_applyingAiBatch = busy();
}

void ApplicationIconBackend::cancelAiBatch()
{
    if (busy()) return;
    m_aiBatchDirectory.reset();
    m_aiBatchId.clear();
    m_aiBatchPreviews.clear();
    clearError();
    Q_EMIT changed();
}

void ApplicationIconBackend::reset()
{
    start({QStringLiteral("--reset")}, tr("Restoring original application icons…"));
}
