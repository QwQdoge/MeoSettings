#pragma once

#include "../core/backendbase.h"

#include <QStringList>
#include <QProcess>

// Thin, deliberately narrow adapter for MeoKDE's installed dynamic-color
// generator.  The generator is the sole owner of applying a complete HCT/MD3
// scheme to KDE; Settings only exposes an explicit, user-initiated request.
class DynamicColorBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QString toolPath READ toolPath NOTIFY changed)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY changed)
    Q_PROPERTY(QString sourceMode READ sourceMode NOTIFY changed)
    Q_PROPERTY(QString manualColor READ manualColor NOTIFY changed)

public:
    explicit DynamicColorBackend(QObject *parent = nullptr);

    QString toolPath() const;
    QString lastResult() const;
    QString sourceMode() const;
    QString manualColor() const;

    /// Produces the fixed, argument-safe native generator invocation used by
    /// Settings. Kept public for a focused contract test; it never launches a
    /// process or writes a preference on its own.
    static QStringList applyArguments(const QString &sourceMode, const QString &manualColor,
                                      QString *error = nullptr);

    Q_INVOKABLE void refresh();
    /// Applies and remembers a single source only after an explicit UI
    /// confirmation. The native MeoKDE generator remains sole owner of HCT/MD3
    /// generation, persistence, KDE projection, and session notification.
    Q_INVOKABLE void applySource(const QString &sourceMode, const QString &manualColor = {});
    Q_INVOKABLE void applyCurrentKdeSeed();

Q_SIGNALS:
    void changed();

private:
    QString m_toolPath;
    QString m_lastResult;
    QString m_sourceMode = QStringLiteral("accent");
    QString m_manualColor;
    QProcess m_process;
};
