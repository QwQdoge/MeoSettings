#include <QCommandLineParser>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "backends/fingerprintbackend.h"
#include "core/qmlimportpolicy.h"

class WelcomeState final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool completed READ completed NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    bool completed() const
    {
        return QSettings().value(QStringLiteral("welcome/completed"), false).toBool();
    }

    QString error() const
    {
        return m_error;
    }

    Q_INVOKABLE void complete()
    {
        QSettings().setValue(QStringLiteral("welcome/completed"), true);
        Q_EMIT changed();
        QCoreApplication::quit();
    }

    Q_INVOKABLE bool openSettings(const QString &route)
    {
        m_error.clear();
        const QString launcher = QStandardPaths::findExecutable(QStringLiteral("meo-settings"));
        if (launcher.isEmpty()) {
            m_error = tr("Meo Settings is not installed.");
            Q_EMIT changed();
            return false;
        }
        if (!QProcess::startDetached(launcher, {QStringLiteral("--route"), route})) {
            m_error = tr("Unable to open Meo Settings.");
            Q_EMIT changed();
            return false;
        }
        Q_EMIT changed();
        return true;
    }

Q_SIGNALS:
    void changed();

private:
    QString m_error;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Meo Welcome"));
    app.setApplicationDisplayName(QStringLiteral("Welcome to Meo"));
    app.setOrganizationName(QStringLiteral("MeoArch"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption showOption(QStringLiteral("show"),
                                        QStringLiteral("Show the welcome flow even after completion."));
    const QCommandLineOption smokeOption(QStringLiteral("smoke"),
                                         QStringLiteral("Load the welcome flow, then exit after the startup event loop."));
    parser.addOption(showOption);
    parser.addOption(smokeOption);
    parser.process(app);

    WelcomeState state;
    FingerprintBackend fingerprintBackend;
    if (state.completed() && !parser.isSet(showOption)) {
        return 0;
    }

    QQmlApplicationEngine engine;
#ifdef MEOUI_IMPORT_ROOT_PATH
    MeoQmlImportPolicy::prioritizeMeoUi(engine, QStringLiteral(MEOUI_IMPORT_ROOT_PATH));
#else
    MeoQmlImportPolicy::prioritizeMeoUi(engine);
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("WelcomeState"), &state);
    // The welcome flow receives only the privacy-safe fprintd projection. It
    // cannot read, enroll, export, or otherwise handle biometric data.
    engine.rootContext()->setContextProperty(QStringLiteral("FingerprintBackend"), &fingerprintBackend);
    engine.loadFromModule(QStringLiteral("org.meo.welcome"), QStringLiteral("Welcome"));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    if (parser.isSet(smokeOption)) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }
    return app.exec();
}

#include "welcome-main.moc"
