#pragma once

#include "../core/backendbase.h"

#include <QProcess>
#include <QVariantList>

#include <memory>

class QTemporaryFile;
class QTemporaryDir;

// Narrow adapter for MeoKDE's installed app-icon studio.  It never changes the
// global KDE icon theme: the tool creates only user-local application desktop
// entry overrides with unique hicolor icon names.
class ApplicationIconBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QString toolPath READ toolPath NOTIFY changed)
    Q_PROPERTY(QString style READ style NOTIFY changed)
    Q_PROPERTY(QString shape READ shape NOTIFY changed)
    Q_PROPERTY(QString prompt READ prompt NOTIFY changed)
    Q_PROPERTY(QVariantList applications READ applications NOTIFY changed)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY changed)
    Q_PROPERTY(QString aiBatchId READ aiBatchId NOTIFY changed)
    Q_PROPERTY(QVariantList aiBatchPreviews READ aiBatchPreviews NOTIFY changed)

public:
    explicit ApplicationIconBackend(QObject *parent = nullptr);
    ~ApplicationIconBackend() override;

    QString toolPath() const;
    QString style() const;
    QString shape() const;
    QString prompt() const;
    QVariantList applications() const;
    QString lastResult() const;
    QString aiBatchId() const;
    QVariantList aiBatchPreviews() const;

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
    Q_INVOKABLE void reset();

Q_SIGNALS:
    void changed();

private:
    void start(const QStringList &arguments, const QString &successMessage);

    QString m_toolPath;
    QString m_style = QStringLiteral("monet");
    QString m_shape = QStringLiteral("circle");
    QString m_prompt;
    QString m_lastResult;
    QVariantList m_applications;
    QString m_aiBatchId;
    QVariantList m_aiBatchPreviews;
    QProcess m_process;
    std::unique_ptr<QTemporaryFile> m_aiImageFile;
    std::unique_ptr<QTemporaryDir> m_aiBatchDirectory;
    bool m_applyingAiBatch = false;
};
