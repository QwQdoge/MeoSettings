#pragma once

#include "../core/backendbase.h"

#include <QProcess>
#include <QVariantList>

#include <memory>

class QTemporaryFile;
class QTemporaryDir;

// Narrow adapter for MeoKDE's package-owned app-icon studio. The renderer owns
// the user-level icon-theme overlay and its narrowly audited fallback; Settings
// never resolves an arbitrary executable from the user's PATH.
class ApplicationIconBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QString toolPath READ toolPath NOTIFY changed)
    Q_PROPERTY(QString style READ style NOTIFY changed)
    Q_PROPERTY(QString shape READ shape NOTIFY changed)
    Q_PROPERTY(QString prompt READ prompt NOTIFY changed)
    Q_PROPERTY(QVariantList applications READ applications NOTIFY changed)
    Q_PROPERTY(bool applicationsLoading READ applicationsLoading NOTIFY changed)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY changed)
    Q_PROPERTY(QString aiBatchId READ aiBatchId NOTIFY changed)
    Q_PROPERTY(QVariantList aiBatchPreviews READ aiBatchPreviews NOTIFY changed)
    Q_PROPERTY(bool aiPackDescribing READ aiPackDescribing NOTIFY changed)
    Q_PROPERTY(bool aiPackPreviewing READ aiPackPreviewing NOTIFY changed)
    Q_PROPERTY(bool aiPackPreviewReady READ aiPackPreviewReady NOTIFY changed)
    Q_PROPERTY(bool aiPackCommitted READ aiPackCommitted NOTIFY changed)
    Q_PROPERTY(QString aiPackId READ aiPackId NOTIFY changed)
    Q_PROPERTY(QVariantList aiPackDescriptors READ aiPackDescriptors NOTIFY changed)
    Q_PROPERTY(QVariantList aiPackPreviews READ aiPackPreviews NOTIFY changed)

public:
    explicit ApplicationIconBackend(QObject *parent = nullptr,
                                    const QString &testToolPath = {});
    ~ApplicationIconBackend() override;

    QString toolPath() const;
    QString style() const;
    QString shape() const;
    QString prompt() const;
    QVariantList applications() const;
    bool applicationsLoading() const;
    QString lastResult() const;
    QString aiBatchId() const;
    QVariantList aiBatchPreviews() const;
    bool aiPackDescribing() const;
    bool aiPackPreviewing() const;
    bool aiPackPreviewReady() const;
    bool aiPackCommitted() const;
    QString aiPackId() const;
    QVariantList aiPackDescriptors() const;
    QVariantList aiPackPreviews() const;

    static bool isSupportedStyle(const QString &style);
    static bool isSupportedShape(const QString &shape);
    static QStringList applyArguments(const QString &style, const QString &shape, const QString &prompt,
                                      QString *error = nullptr,
                                      const QStringList &applicationIds = {});

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void apply(const QString &style, const QString &shape, const QString &prompt);
    Q_INVOKABLE void applyToApplications(const QStringList &applicationIds, const QString &style,
                                         const QString &shape, const QString &prompt);
    Q_INVOKABLE void applyAiImage(const QString &applicationId, const QString &imageSource,
                                  const QString &shape, const QString &prompt);
    Q_INVOKABLE bool beginAiBatch();
    Q_INVOKABLE bool stageAiImage(const QString &applicationId, const QString &applicationName,
                                  const QString &imageSource, const QString &shape,
                                  const QString &prompt);
    Q_INVOKABLE void applyAiBatch();
    Q_INVOKABLE void cancelAiBatch();
    // The production Account flow is intentionally separate from the legacy
    // per-image bridge above. Account first receives only Studio's canonical
    // identity descriptors, then returns a locally staged schema-2 pack.
    // Preview runs the exact Studio renderer without mutating theme state;
    // apply is enabled only after that preview succeeds.
    Q_INVOKABLE void describeAiPackApplications(const QStringList &applicationIds);
    Q_INVOKABLE void previewAttestedAiPack(const QString &manifestPath);
    Q_INVOKABLE void applyAttestedAiPack();
    Q_INVOKABLE void discardAttestedAiPack();
    Q_INVOKABLE void resetApplications(const QStringList &applicationIds);
    Q_INVOKABLE void reset();

Q_SIGNALS:
    void changed();

private:
    void start(const QStringList &arguments, const QString &successMessage);
    void requestApplicationListing();
    void clearAttestedAiPack(bool clearDescriptors = true);

    QString m_toolPath;
    QString m_style = QStringLiteral("monet");
    QString m_shape = QStringLiteral("circle");
    QString m_prompt;
    QString m_lastResult;
    QVariantList m_applications;
    bool m_applicationsLoading = false;
    bool m_listingRefreshQueued = false;
    QString m_listingToolPath;
    const QString m_testToolPath;
    QString m_aiBatchId;
    QVariantList m_aiBatchPreviews;
    QProcess m_process;
    QProcess m_listingProcess;
    QProcess m_aiPackDescribeProcess;
    QProcess m_aiPackPreviewProcess;
    std::unique_ptr<QTemporaryFile> m_aiImageFile;
    std::unique_ptr<QTemporaryDir> m_aiBatchDirectory;
    std::unique_ptr<QTemporaryDir> m_aiPackPreviewDirectory;
    bool m_applyingAiBatch = false;
    bool m_applyingAttestedAiPack = false;
    bool m_resettingAttestedAiPack = false;
    bool m_aiPackDescribing = false;
    bool m_aiPackPreviewing = false;
    bool m_aiPackPreviewReady = false;
    bool m_aiPackCommitted = false;
    QString m_aiPackId;
    QString m_aiPackManifestPath;
    QByteArray m_aiPackManifestHash;
    QStringList m_aiPackRequestedIds;
    QVariantList m_aiPackDescriptors;
    QVariantList m_aiPackPreviews;
};
