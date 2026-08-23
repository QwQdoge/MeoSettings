#include "dynamiccolorbackend.h"

#include <QColor>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace
{
QString normalizedProcessMessage(QProcess &process)
{
    const QString standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!standardError.isEmpty()) {
        return standardError;
    }

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString normalizedSourceMode(const QString &sourceMode)
{
    return sourceMode.trimmed().toLower();
}

bool supportedSourceMode(const QString &sourceMode)
{
    return sourceMode == QLatin1String("accent") || sourceMode == QLatin1String("wallpaper")
        || sourceMode == QLatin1String("manual");
}

QString sourceConfigPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("meo-dynamic-colorsrc"));
}
}

DynamicColorBackend::DynamicColorBackend(QObject *parent)
    : BackendBase(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError) {
        const QString message = m_process.errorString().trimmed();
        setError(message.isEmpty() ? tr("Meo dynamic color could not start.") : message);
        setBusy(false);
        Q_EMIT changed();
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                const QString result = normalizedProcessMessage(m_process);
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    setError(result.isEmpty()
                                 ? tr("Meo dynamic color could not apply the current scheme.")
                                 : result);
                } else {
                    m_lastResult = result.isEmpty()
                                       ? tr("Applied the current Meo dynamic color scheme.")
                                       : result;
                    clearError();
                    // The generator persists the selected source only after
                    // its scheme work succeeds. Reload that truthful state
                    // instead of assuming the requested source was applied.
                    refresh();
                }
                setBusy(false);
                Q_EMIT changed();
            });
    refresh();
}

QString DynamicColorBackend::toolPath() const
{
    return m_toolPath;
}

QString DynamicColorBackend::lastResult() const
{
    return m_lastResult;
}

QString DynamicColorBackend::sourceMode() const
{
    return m_sourceMode;
}

QString DynamicColorBackend::manualColor() const
{
    return m_manualColor;
}

QStringList DynamicColorBackend::applyArguments(const QString &requestedSource,
                                                const QString &requestedManualColor,
                                                QString *error)
{
    if (error) {
        error->clear();
    }
    const QString source = normalizedSourceMode(requestedSource);
    if (!supportedSourceMode(source)) {
        if (error) {
            *error = QObject::tr("Choose KDE accent, wallpaper, or a manual color source.");
        }
        return {};
    }

    QStringList arguments{QStringLiteral("--source"), source};
    if (source == QLatin1String("manual")) {
        const QColor color(requestedManualColor.trimmed());
        if (!color.isValid() || requestedManualColor.trimmed().size() != 7
            || !requestedManualColor.trimmed().startsWith(QLatin1Char('#'))) {
            if (error) {
                *error = QObject::tr("Manual dynamic color must use #RRGGBB.");
            }
            return {};
        }
        arguments << QStringLiteral("--accent") << color.name(QColor::HexRgb);
    }
    arguments << QStringLiteral("--apply") << QStringLiteral("--remember-source");
    return arguments;
}

void DynamicColorBackend::refresh()
{
    const QString nextPath = QStandardPaths::findExecutable(QStringLiteral("meo-dynamic-colors"));
    QSettings sourceSettings(sourceConfigPath(), QSettings::IniFormat);
    QString nextSource = normalizedSourceMode(sourceSettings.value(QStringLiteral("Source/Mode"),
                                                                     QStringLiteral("accent")).toString());
    if (!supportedSourceMode(nextSource)) {
        nextSource = QStringLiteral("accent");
    }
    const QColor nextManualColor(sourceSettings.value(QStringLiteral("Source/ManualColor")).toString());
    const QString nextManual = nextManualColor.isValid()
        ? nextManualColor.name(QColor::HexRgb) : QString{};
    const bool changed = m_toolPath != nextPath || m_sourceMode != nextSource
        || m_manualColor != nextManual;
    m_toolPath = nextPath;
    m_sourceMode = nextSource;
    m_manualColor = nextManual;
    setAvailable(!m_toolPath.isEmpty());
    if (changed) {
        Q_EMIT this->changed();
    }
}

void DynamicColorBackend::applyCurrentKdeSeed()
{
    applySource(QStringLiteral("accent"));
}

void DynamicColorBackend::applySource(const QString &requestedSource, const QString &requestedManualColor)
{
    clearError();
    if (busy()) {
        return;
    }
    if (m_toolPath.isEmpty()) {
        setError(tr("The installed Meo dynamic-color generator is unavailable."));
        return;
    }

    QString argumentError;
    const QStringList arguments = applyArguments(requestedSource, requestedManualColor, &argumentError);
    if (arguments.isEmpty()) {
        setError(argumentError);
        return;
    }

    // Do not synthesize a palette in Settings. The native MeoKDE tool resolves
    // one source, derives every role with HCT/CAM16, records the source, and
    // notifies KDE as one user-approved operation.
    setBusy(true);
    m_process.start(m_toolPath, arguments);
}
