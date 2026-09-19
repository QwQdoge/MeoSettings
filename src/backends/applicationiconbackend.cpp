#include "applicationiconbackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QImageReader>
#include <QRegularExpression>
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
constexpr auto packageStudioPath = "/usr/bin/meo-app-icon-studio";
constexpr qsizetype maximumAiPackManifestBytes = 2'000'000;
constexpr qsizetype maximumAiPackImageBytes = 12'000'000;
constexpr qsizetype maximumAiPackPreviewBytes = 8'000'000;
constexpr qint64 maximumAiPackImagePixels = 16'000'000;

struct ValidatedAttestedAiPack
{
    QString manifestPath;
    QByteArray manifestHash;
    QString packId;
};

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

QString verifiedStudioPath(const QString &testOverride)
{
    const QString candidate = testOverride.isEmpty()
        ? QString::fromLatin1(packageStudioPath) : testOverride;
    const QFileInfo info(candidate);
    return info.isFile() && info.isExecutable() ? info.absoluteFilePath() : QString();
}

bool isSafeDesktopId(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,511}$"));
    return expression.match(value).hasMatch();
}

bool isLowerSha256(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral("^[a-f0-9]{64}$"));
    return expression.match(value).hasMatch();
}

QString sha256File(const QString &path, bool *ok = nullptr)
{
    if (ok) {
        *ok = false;
    }
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!input.atEnd()) {
        const QByteArray chunk = input.read(1024 * 1024);
        if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
            return {};
        }
        hash.addData(chunk);
    }
    if (ok) {
        *ok = true;
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool isInsideDirectory(const QString &candidate, const QString &directory)
{
    return candidate.startsWith(directory + QLatin1Char('/'));
}

bool isReadableBoundedImage(const QString &path, const qsizetype maximumBytes,
                            QString *error)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > maximumBytes) {
        if (error) {
            *error = QObject::tr("An AI icon image is missing, linked, or too large.");
        }
        return false;
    }
    QImageReader reader(path);
    reader.setAutoTransform(false);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() < 1 || size.height() < 1
        || qint64(size.width()) * qint64(size.height()) > maximumAiPackImagePixels) {
        if (error) {
            *error = QObject::tr("An AI icon image has an unsafe decoded size.");
        }
        return false;
    }
    if (reader.read().isNull()) {
        if (error) {
            *error = QObject::tr("An AI icon image could not be decoded.");
        }
        return false;
    }
    return true;
}

QString privatePreviewTemplate()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    return QDir(root).filePath(QStringLiteral("meo-ai-icon-preview-XXXXXX"));
}

bool setPrivateDirectoryPermissions(const QString &path)
{
    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                           | QFileDevice::ExeOwner);
}

bool validateAttestedAiPackManifest(const QString &requestedPath,
                                    const QVariantList &descriptors,
                                    ValidatedAttestedAiPack *result,
                                    QString *error)
{
    if (result) {
        *result = {};
    }
    if (error) {
        error->clear();
    }
    if (descriptors.isEmpty()) {
        if (error) {
            *error = QObject::tr("Describe the selected application identities before generating an AI icon pack.");
        }
        return false;
    }

    const QFileInfo manifestInfo(requestedPath);
    if (!manifestInfo.isFile() || manifestInfo.isSymLink() || manifestInfo.size() < 1
        || manifestInfo.size() > maximumAiPackManifestBytes) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack manifest is missing, linked, or too large.");
        }
        return false;
    }
    const QString stageRootPath = manifestInfo.dir().absolutePath();
    const QFileInfo stageRootInfo(stageRootPath);
    const QString canonicalRoot = stageRootInfo.canonicalFilePath();
    if (stageRootInfo.isSymLink() || canonicalRoot.isEmpty()) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack directory is invalid.");
        }
        return false;
    }

    bool manifestHashOk = false;
    const QString manifestHash = sha256File(manifestInfo.absoluteFilePath(), &manifestHashOk);
    if (!manifestHashOk) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack manifest could not be read.");
        }
        return false;
    }

    QFile manifest(manifestInfo.absoluteFilePath());
    if (!manifest.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack manifest could not be read.");
        }
        return false;
    }
    const QJsonDocument parsed = QJsonDocument::fromJson(manifest.readAll());
    const QJsonObject payload = parsed.object();
    if (parsed.isNull() || !parsed.isObject()
        || !payload.value(QStringLiteral("schema")).isDouble()
        || payload.value(QStringLiteral("schema")).toInt(-1) != 2) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack is not an attested schema-2 manifest.");
        }
        return false;
    }
    const QString packId = payload.value(QStringLiteral("packId")).toString().trimmed();
    const QString styleId = payload.value(QStringLiteral("styleId")).toString().trimmed();
    if (!isSafeDesktopId(packId) || !isSafeDesktopId(styleId)) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack identity is invalid.");
        }
        return false;
    }

    QHash<QString, QString> expectedHashes;
    for (const QVariant &value : descriptors) {
        const QVariantMap descriptor = value.toMap();
        const QString desktopId = descriptor.value(QStringLiteral("desktopId")).toString().trimmed();
        const QString identityHash = descriptor.value(QStringLiteral("canonicalIdentityHash")).toString().trimmed();
        if (!isSafeDesktopId(desktopId) || !isLowerSha256(identityHash)
            || expectedHashes.contains(desktopId)) {
            if (error) {
                *error = QObject::tr("The selected AI icon application identities are invalid.");
            }
            return false;
        }
        expectedHashes.insert(desktopId, identityHash);
    }

    const QJsonArray items = payload.value(QStringLiteral("items")).toArray();
    if (items.isEmpty() || items.size() > 128 || items.size() != expectedHashes.size()) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack does not match the selected applications.");
        }
        return false;
    }

    QHash<QString, bool> seen;
    QString materialHash;
    QString materialCanonicalPath;
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        const QString desktopId = item.value(QStringLiteral("desktopId")).toString().trimmed();
        const QString identityHash = item.value(QStringLiteral("sourceIconHash")).toString().trimmed();
        const QString imageHash = item.value(QStringLiteral("imageSha256")).toString().trimmed();
        const QString shape = item.value(QStringLiteral("shape")).toString().trimmed().toLower();
        const QString recipe = item.value(QStringLiteral("prompt")).toString().trimmed();
        const QString relativeImage = item.value(QStringLiteral("image")).toString().trimmed();
        if (!isSafeDesktopId(desktopId) || seen.contains(desktopId)
            || !expectedHashes.contains(desktopId) || expectedHashes.value(desktopId) != identityHash
            || !isLowerSha256(identityHash) || !isLowerSha256(imageHash)
            || !ApplicationIconBackend::isSupportedShape(shape)
            || recipe.isEmpty() || recipe.size() > 256
            || !recipe.startsWith(QLatin1String("meo-style:"))
            || relativeImage.isEmpty() || relativeImage.contains(QChar::Null)
            || QDir::isAbsolutePath(relativeImage)) {
            if (error) {
                *error = QObject::tr("An attested AI icon pack item is invalid.");
            }
            return false;
        }
        const QString cleanedImage = QDir::cleanPath(relativeImage);
        if (cleanedImage == QLatin1String(".") || cleanedImage == QLatin1String("..")
            || cleanedImage.startsWith(QLatin1String("../"))) {
            if (error) {
                *error = QObject::tr("An attested AI icon image escapes its staging directory.");
            }
            return false;
        }
        const QFileInfo imageInfo(QDir(stageRootPath).absoluteFilePath(cleanedImage));
        const QString canonicalImage = imageInfo.canonicalFilePath();
        if (canonicalImage.isEmpty() || !isInsideDirectory(canonicalImage, canonicalRoot)
            || !isReadableBoundedImage(imageInfo.absoluteFilePath(), maximumAiPackImageBytes, error)) {
            return false;
        }
        bool imageHashOk = false;
        const QString actualImageHash = sha256File(imageInfo.absoluteFilePath(), &imageHashOk);
        if (!imageHashOk || actualImageHash != imageHash) {
            if (error) {
                *error = QObject::tr("An attested AI icon image no longer matches its manifest.");
            }
            return false;
        }
        if (materialHash.isEmpty()) {
            materialHash = imageHash;
            materialCanonicalPath = canonicalImage;
        } else if (materialHash != imageHash || materialCanonicalPath != canonicalImage) {
            if (error) {
                *error = QObject::tr("A Pixel-style AI icon pack must use one shared approved material.");
            }
            return false;
        }
        seen.insert(desktopId, true);
    }
    if (seen.size() != expectedHashes.size()) {
        if (error) {
            *error = QObject::tr("The staged AI icon pack does not include every selected application.");
        }
        return false;
    }
    if (result) {
        result->manifestPath = manifestInfo.absoluteFilePath();
        result->manifestHash = manifestHash.toLatin1();
        result->packId = packId;
    }
    return true;
}

bool parseAttestedAiPackPreview(const QByteArray &rawOutput, const QString &previewRootPath,
                                const QVariantList &descriptors, const QString &expectedPackId,
                                QVariantList *result, QString *error)
{
    if (result) {
        result->clear();
    }
    if (error) {
        error->clear();
    }
    const QJsonDocument document = QJsonDocument::fromJson(rawOutput);
    const QJsonObject payload = document.object();
    if (document.isNull() || !document.isObject()
        || !payload.value(QStringLiteral("schema")).isDouble()
        || payload.value(QStringLiteral("schema")).toInt(-1) != 1
        || payload.value(QStringLiteral("packId")).toString() != expectedPackId) {
        if (error) {
            *error = QObject::tr("Meo Icon Studio returned an invalid AI icon preview.");
        }
        return false;
    }
    const QFileInfo rootInfo(previewRootPath);
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    if (rootInfo.isSymLink() || canonicalRoot.isEmpty()) {
        if (error) {
            *error = QObject::tr("The private AI icon preview directory is invalid.");
        }
        return false;
    }
    QHash<QString, QVariantMap> expected;
    for (const QVariant &value : descriptors) {
        const QVariantMap descriptor = value.toMap();
        const QString desktopId = descriptor.value(QStringLiteral("desktopId")).toString();
        const QString identityHash = descriptor.value(QStringLiteral("canonicalIdentityHash")).toString();
        if (!isSafeDesktopId(desktopId) || !isLowerSha256(identityHash)
            || expected.contains(desktopId)) {
            if (error) {
                *error = QObject::tr("The selected AI icon application identities are invalid.");
            }
            return false;
        }
        expected.insert(desktopId, descriptor);
    }
    const QJsonArray previews = payload.value(QStringLiteral("previews")).toArray();
    if (previews.size() != expected.size()
        || payload.value(QStringLiteral("applicationCount")).toInt(-1) != expected.size()) {
        if (error) {
            *error = QObject::tr("The AI icon preview does not match the selected applications.");
        }
        return false;
    }
    QHash<QString, bool> seen;
    QVariantList parsedPreviews;
    for (const QJsonValue &value : previews) {
        const QJsonObject preview = value.toObject();
        const QString desktopId = preview.value(QStringLiteral("desktopId")).toString().trimmed();
        const QString sourceHash = preview.value(QStringLiteral("sourceIconHash")).toString().trimmed();
        const QString shape = preview.value(QStringLiteral("shape")).toString().trimmed().toLower();
        const QString previewPath = preview.value(QStringLiteral("preview")).toString().trimmed();
        if (!isSafeDesktopId(desktopId) || seen.contains(desktopId) || !expected.contains(desktopId)
            || sourceHash != expected.value(desktopId).value(QStringLiteral("canonicalIdentityHash")).toString()
            || !isLowerSha256(sourceHash) || !ApplicationIconBackend::isSupportedShape(shape)
            || !QDir::isAbsolutePath(previewPath)) {
            if (error) {
                *error = QObject::tr("A rendered AI icon preview is invalid.");
            }
            return false;
        }
        const QFileInfo previewInfo(previewPath);
        const QString canonicalPreview = previewInfo.canonicalFilePath();
        if (canonicalPreview.isEmpty() || !isInsideDirectory(canonicalPreview, canonicalRoot)
            || !isReadableBoundedImage(previewInfo.absoluteFilePath(), maximumAiPackPreviewBytes, error)) {
            return false;
        }
        const QVariantMap descriptor = expected.value(desktopId);
        parsedPreviews.append(QVariantMap{
            {QStringLiteral("desktopId"), desktopId},
            {QStringLiteral("name"), descriptor.value(QStringLiteral("name")).toString()},
            {QStringLiteral("preview"), QUrl::fromLocalFile(previewInfo.absoluteFilePath()).toString()},
            {QStringLiteral("sourceIconHash"), sourceHash},
            {QStringLiteral("shape"), shape},
        });
        seen.insert(desktopId, true);
    }
    if (seen.size() != expected.size()) {
        if (error) {
            *error = QObject::tr("The AI icon preview does not include every selected application.");
        }
        return false;
    }
    if (result) {
        *result = parsedPreviews;
    }
    return true;
}
}

ApplicationIconBackend::ApplicationIconBackend(QObject *parent, const QString &testToolPath)
    : BackendBase(parent)
    , m_testToolPath(testToolPath)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError) {
                m_aiImageFile.reset();
                m_applyingAiBatch = false;
                m_applyingAttestedAiPack = false;
                m_resettingAttestedAiPack = false;
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
                const bool appliedAttestedAiPack = m_applyingAttestedAiPack && succeeded;
                const bool resetAttestedAiPack = m_resettingAttestedAiPack && succeeded;
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
                if (appliedAttestedAiPack) {
                    m_aiPackCommitted = true;
                }
                m_applyingAttestedAiPack = false;
                if (resetAttestedAiPack) {
                    m_aiPackCommitted = false;
                }
                m_resettingAttestedAiPack = false;
                setBusy(false);
                Q_EMIT changed();
            });

    // Application discovery is intentionally separate from the mutation
    // process.  The previous implementation waited up to five seconds in
    // refresh(), which could block the Qt Quick GUI thread before the first
    // frame of Settings was presented.
    m_listingProcess.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_listingProcess, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError) {
                m_applicationsLoading = false;
                Q_EMIT changed();
                if (m_listingRefreshQueued) {
                    m_listingRefreshQueued = false;
                    requestApplicationListing();
                }
            });
    connect(&m_listingProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                QVariantList nextApplications;
                const bool resultMatchesCurrentTool = m_listingToolPath == m_toolPath;
                if (resultMatchesCurrentTool && exitStatus == QProcess::NormalExit && exitCode == 0) {
                    const QJsonArray array = QJsonDocument::fromJson(
                        m_listingProcess.readAllStandardOutput()).array();
                    for (const QJsonValue &value : array) {
                        const QJsonObject object = value.toObject();
                        nextApplications.append(QVariantMap{
                            {QStringLiteral("desktopId"), object.value(QStringLiteral("desktop_id")).toString()},
                            {QStringLiteral("name"), object.value(QStringLiteral("name")).toString()},
                            {QStringLiteral("icon"), object.value(QStringLiteral("icon")).toString()},
                        });
                    }
                }
                if (resultMatchesCurrentTool && nextApplications != m_applications) {
                    m_applications = nextApplications;
                }
                m_applicationsLoading = false;
                Q_EMIT changed();
                if (m_listingRefreshQueued || !resultMatchesCurrentTool) {
                    m_listingRefreshQueued = false;
                    requestApplicationListing();
                }
            });

    m_aiPackDescribeProcess.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_aiPackDescribeProcess, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError) {
                if (!m_aiPackDescribing) {
                    return;
                }
                m_aiPackDescribing = false;
                m_aiPackRequestedIds.clear();
                m_aiPackDescriptors.clear();
                setError(m_aiPackDescribeProcess.errorString().trimmed().isEmpty()
                             ? tr("Meo Icon Studio could not describe the selected applications.")
                             : m_aiPackDescribeProcess.errorString().trimmed());
                setBusy(false);
                Q_EMIT changed();
            });
    connect(&m_aiPackDescribeProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                if (!m_aiPackDescribing) {
                    return;
                }
                m_aiPackDescribing = false;
                const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
                QVariantList nextDescriptors;
                QString parseError;
                if (succeeded) {
                    const QJsonDocument document = QJsonDocument::fromJson(
                        m_aiPackDescribeProcess.readAllStandardOutput());
                    const QJsonArray values = document.array();
                    QHash<QString, QJsonObject> returned;
                    if (!document.isArray() || values.size() != m_aiPackRequestedIds.size()) {
                        parseError = tr("Meo Icon Studio returned an incomplete application identity description.");
                    } else {
                        for (const QJsonValue &value : values) {
                            const QJsonObject descriptor = value.toObject();
                            const QString desktopId = descriptor.value(QStringLiteral("desktopId")).toString().trimmed();
                            const QString name = descriptor.value(QStringLiteral("name")).toString().trimmed();
                            const QString identityHash = descriptor.value(QStringLiteral("canonicalIdentityHash")).toString().trimmed();
                            if (!isSafeDesktopId(desktopId) || !m_aiPackRequestedIds.contains(desktopId)
                                || returned.contains(desktopId) || name.isEmpty() || name.size() > 256
                                || !descriptor.value(QStringLiteral("canonicalIdentityAvailable")).toBool()
                                || !isLowerSha256(identityHash)) {
                                parseError = tr("Meo Icon Studio returned an invalid application identity description.");
                                break;
                            }
                            returned.insert(desktopId, descriptor);
                        }
                    }
                    if (parseError.isEmpty()) {
                        for (const QString &desktopId : std::as_const(m_aiPackRequestedIds)) {
                            if (!returned.contains(desktopId)) {
                                parseError = tr("Meo Icon Studio did not describe every selected application.");
                                break;
                            }
                            const QJsonObject descriptor = returned.value(desktopId);
                            nextDescriptors.append(QVariantMap{
                                {QStringLiteral("desktopId"), desktopId},
                                {QStringLiteral("name"), descriptor.value(QStringLiteral("name")).toString().trimmed()},
                                {QStringLiteral("canonicalIdentityHash"), descriptor.value(QStringLiteral("canonicalIdentityHash")).toString().trimmed()},
                            });
                        }
                    }
                }
                m_aiPackRequestedIds.clear();
                const QString failureMessage = succeeded ? QString()
                    : processMessage(m_aiPackDescribeProcess);
                if (!succeeded || !parseError.isEmpty()) {
                    m_aiPackDescriptors.clear();
                    setError(!parseError.isEmpty() ? parseError
                        : (failureMessage.isEmpty()
                            ? tr("Meo Icon Studio could not describe the selected applications.")
                            : failureMessage));
                } else {
                    m_aiPackDescriptors = nextDescriptors;
                    clearError();
                }
                setBusy(false);
                Q_EMIT changed();
            });

    m_aiPackPreviewProcess.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_aiPackPreviewProcess, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError) {
                if (!m_aiPackPreviewing) {
                    return;
                }
                m_aiPackPreviewing = false;
                m_aiPackPreviewReady = false;
                m_aiPackPreviews.clear();
                m_aiPackPreviewDirectory.reset();
                setError(m_aiPackPreviewProcess.errorString().trimmed().isEmpty()
                             ? tr("Meo Icon Studio could not render the AI icon preview.")
                             : m_aiPackPreviewProcess.errorString().trimmed());
                setBusy(false);
                Q_EMIT changed();
            });
    connect(&m_aiPackPreviewProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                if (!m_aiPackPreviewing) {
                    return;
                }
                m_aiPackPreviewing = false;
                const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
                QVariantList previews;
                QString parseError;
                if (succeeded) {
                    if (!m_aiPackPreviewDirectory || !m_aiPackPreviewDirectory->isValid()) {
                        parseError = tr("The private AI icon preview directory is unavailable.");
                    } else {
                        parseAttestedAiPackPreview(m_aiPackPreviewProcess.readAllStandardOutput(),
                                                    m_aiPackPreviewDirectory->path(),
                                                    m_aiPackDescriptors, m_aiPackId,
                                                    &previews, &parseError);
                    }
                }
                const QString failureMessage = succeeded ? QString()
                    : processMessage(m_aiPackPreviewProcess);
                if (!succeeded || !parseError.isEmpty()) {
                    m_aiPackPreviewReady = false;
                    m_aiPackPreviews.clear();
                    m_aiPackPreviewDirectory.reset();
                    setError(!parseError.isEmpty() ? parseError
                        : (failureMessage.isEmpty()
                            ? tr("Meo Icon Studio could not render the AI icon preview.")
                            : failureMessage));
                } else {
                    m_aiPackPreviews = previews;
                    m_aiPackPreviewReady = true;
                    clearError();
                }
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
bool ApplicationIconBackend::applicationsLoading() const { return m_applicationsLoading; }
QString ApplicationIconBackend::lastResult() const { return m_lastResult; }
QString ApplicationIconBackend::aiBatchId() const { return m_aiBatchId; }
QVariantList ApplicationIconBackend::aiBatchPreviews() const { return m_aiBatchPreviews; }
bool ApplicationIconBackend::aiPackDescribing() const { return m_aiPackDescribing; }
bool ApplicationIconBackend::aiPackPreviewing() const { return m_aiPackPreviewing; }
bool ApplicationIconBackend::aiPackPreviewReady() const { return m_aiPackPreviewReady; }
bool ApplicationIconBackend::aiPackCommitted() const { return m_aiPackCommitted; }
QString ApplicationIconBackend::aiPackId() const { return m_aiPackId; }
QVariantList ApplicationIconBackend::aiPackDescriptors() const { return m_aiPackDescriptors; }
QVariantList ApplicationIconBackend::aiPackPreviews() const { return m_aiPackPreviews; }

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
    // Original is a lookup reset, not a rendering request.  Requiring an old
    // rendering prompt here made the visible Original action fail after a user
    // had cleared the optional AI text field.
    if ((style != QLatin1String("original") && prompt.isEmpty())
        || prompt.size() > 4000 || prompt.contains(QChar::Null)) {
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
              << QStringLiteral("--shape") << shape;
    if (style != QLatin1String("original")) {
        arguments << QStringLiteral("--prompt") << prompt;
    }
    return arguments;
}

void ApplicationIconBackend::refresh()
{
    const QString nextPath = verifiedStudioPath(m_testToolPath);
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
    if (nextPath.isEmpty() && !m_applications.isEmpty()) {
        m_applications.clear();
        stateChanged = true;
    }
    setAvailable(!m_toolPath.isEmpty());
    if (stateChanged) {
        Q_EMIT changed();
    }
    requestApplicationListing();
}

void ApplicationIconBackend::requestApplicationListing()
{
    if (m_toolPath.isEmpty()) {
        if (m_applicationsLoading) {
            m_applicationsLoading = false;
            Q_EMIT changed();
        }
        return;
    }
    if (m_listingProcess.state() != QProcess::NotRunning) {
        m_listingRefreshQueued = true;
        return;
    }
    m_listingToolPath = m_toolPath;
    m_applicationsLoading = true;
    Q_EMIT changed();
    m_listingProcess.start(m_listingToolPath, {QStringLiteral("--list")});
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

void ApplicationIconBackend::clearAttestedAiPack(const bool clearDescriptors)
{
    m_aiPackPreviewDirectory.reset();
    m_aiPackPreviewing = false;
    m_aiPackPreviewReady = false;
    m_aiPackCommitted = false;
    m_aiPackId.clear();
    m_aiPackManifestPath.clear();
    m_aiPackManifestHash.clear();
    m_aiPackPreviews.clear();
    if (clearDescriptors) {
        m_aiPackRequestedIds.clear();
        m_aiPackDescriptors.clear();
    }
}

void ApplicationIconBackend::describeAiPackApplications(const QStringList &applicationIds)
{
    if (busy()) {
        return;
    }
    if (m_toolPath.isEmpty()) {
        setError(tr("The installed Meo application icon studio is unavailable."));
        return;
    }
    if (applicationIds.isEmpty() || applicationIds.size() > 128) {
        setError(tr("Choose between 1 and 128 applications for an AI icon pack."));
        return;
    }
    QStringList normalizedIds;
    for (const QString &rawId : applicationIds) {
        const QString desktopId = rawId.trimmed();
        if (!isSafeDesktopId(desktopId) || normalizedIds.contains(desktopId)) {
            setError(tr("The selected AI icon applications are invalid."));
            return;
        }
        normalizedIds.append(desktopId);
    }
    clearAttestedAiPack();
    m_aiPackRequestedIds = normalizedIds;
    QStringList arguments{QStringLiteral("--describe")};
    for (const QString &desktopId : std::as_const(m_aiPackRequestedIds)) {
        arguments << QStringLiteral("--app") << desktopId;
    }
    clearError();
    m_aiPackDescribing = true;
    setBusy(true);
    Q_EMIT changed();
    m_aiPackDescribeProcess.start(m_toolPath, arguments);
}

void ApplicationIconBackend::previewAttestedAiPack(const QString &manifestPath)
{
    if (busy()) {
        return;
    }
    if (m_toolPath.isEmpty()) {
        setError(tr("The installed Meo application icon studio is unavailable."));
        return;
    }
    ValidatedAttestedAiPack validated;
    QString validationError;
    if (!validateAttestedAiPackManifest(manifestPath, m_aiPackDescriptors,
                                        &validated, &validationError)) {
        clearAttestedAiPack(false);
        setError(validationError);
        Q_EMIT changed();
        return;
    }
    clearAttestedAiPack(false);
    auto previewDirectory = std::make_unique<QTemporaryDir>(privatePreviewTemplate());
    previewDirectory->setAutoRemove(true);
    if (!previewDirectory->isValid()
        || !setPrivateDirectoryPermissions(previewDirectory->path())) {
        setError(tr("A private AI icon preview directory could not be created."));
        Q_EMIT changed();
        return;
    }
    m_aiPackId = validated.packId;
    m_aiPackManifestPath = validated.manifestPath;
    m_aiPackManifestHash = validated.manifestHash;
    m_aiPackPreviewDirectory = std::move(previewDirectory);
    clearError();
    m_aiPackPreviewing = true;
    setBusy(true);
    Q_EMIT changed();
    m_aiPackPreviewProcess.start(m_toolPath,
                                 {QStringLiteral("--preview-ai-pack"), m_aiPackManifestPath,
                                  QStringLiteral("--preview-output"),
                                  m_aiPackPreviewDirectory->path()});
}

void ApplicationIconBackend::applyAttestedAiPack()
{
    if (busy()) {
        return;
    }
    if (m_toolPath.isEmpty()) {
        setError(tr("The installed Meo application icon studio is unavailable."));
        return;
    }
    if (!m_aiPackPreviewReady || m_aiPackCommitted || m_aiPackManifestPath.isEmpty()) {
        setError(tr("Preview the complete AI icon pack before applying it."));
        return;
    }
    ValidatedAttestedAiPack validated;
    QString validationError;
    if (!validateAttestedAiPackManifest(m_aiPackManifestPath, m_aiPackDescriptors,
                                        &validated, &validationError)
        || validated.packId != m_aiPackId || validated.manifestHash != m_aiPackManifestHash) {
        m_aiPackPreviewReady = false;
        m_aiPackPreviews.clear();
        m_aiPackPreviewDirectory.reset();
        setError(validationError.isEmpty()
                     ? tr("The staged AI icon pack changed after preview. Generate a new preview.")
                     : validationError);
        Q_EMIT changed();
        return;
    }
    m_applyingAttestedAiPack = true;
    start({QStringLiteral("--apply"), QStringLiteral("--ai-pack"), m_aiPackManifestPath},
          tr("Applying the complete AI application icon pack…"));
}

void ApplicationIconBackend::discardAttestedAiPack()
{
    if (busy()) {
        return;
    }
    clearAttestedAiPack();
    clearError();
    Q_EMIT changed();
}

void ApplicationIconBackend::resetApplications(const QStringList &applicationIds)
{
    if (busy()) {
        return;
    }
    if (m_toolPath.isEmpty()) {
        setError(tr("The installed Meo application icon studio is unavailable."));
        return;
    }
    if (applicationIds.isEmpty() || applicationIds.size() > 128) {
        setError(tr("Choose between 1 and 128 applications to restore."));
        return;
    }
    QStringList arguments{QStringLiteral("--reset")};
    QStringList seen;
    for (const QString &rawId : applicationIds) {
        const QString desktopId = rawId.trimmed();
        if (!isSafeDesktopId(desktopId) || seen.contains(desktopId)) {
            setError(tr("The selected application identifiers are invalid."));
            return;
        }
        seen.append(desktopId);
        arguments << QStringLiteral("--app") << desktopId;
    }
    m_resettingAttestedAiPack = true;
    start(arguments, tr("Restoring selected original application icons…"));
}

void ApplicationIconBackend::reset()
{
    start({QStringLiteral("--reset")}, tr("Restoring original application icons…"));
}
