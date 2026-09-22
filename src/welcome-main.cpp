#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QTranslator>
#include <QVariant>

#include "backends/fingerprintbackend.h"
#include "backends/meoaccountbackend.h"
#include "backends/networkbackend.h"
#include "core/qmlimportpolicy.h"

#ifndef MEO_WELCOME_TRANSLATIONS_BUILD_DIR
#define MEO_WELCOME_TRANSLATIONS_BUILD_DIR ""
#endif

#ifndef MEO_WELCOME_TRANSLATIONS_INSTALL_DIR
#define MEO_WELCOME_TRANSLATIONS_INSTALL_DIR ""
#endif

#ifndef MEOUI_TRANSLATIONS_DEVELOPMENT_DIR
#define MEOUI_TRANSLATIONS_DEVELOPMENT_DIR ""
#endif

#ifndef MEOUI_TRANSLATIONS_INSTALL_DIR
#define MEOUI_TRANSLATIONS_INSTALL_DIR ""
#endif

namespace
{
QString normalizedUiLanguage(const QString &requestedLanguage)
{
    const QLocale locale(requestedLanguage.trimmed().isEmpty()
                             ? QLocale::system()
                             : QLocale(requestedLanguage));
    if (locale.language() == QLocale::Chinese) {
        // Welcome currently ships a Simplified Chinese catalog. Do not fall
        // back to English merely because the system uses another Chinese
        // regional locale.
        return QStringLiteral("zh_CN");
    }
    const QString language = locale.name();
    return language == QStringLiteral("C") ? QStringLiteral("en_US") : language;
}

QString bundledTranslationDirectory(const QString &moduleName)
{
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/%1/translations").arg(moduleName)));
}

QStringList translationPaths(const char *environmentVariable,
                             const QStringList &fallbackDirectories)
{
    QStringList paths;
    const QString environmentDirectory = qEnvironmentVariable(environmentVariable).trimmed();
    if (!environmentDirectory.isEmpty()) {
        paths.append(environmentDirectory);
    }
    for (const QString &directory : fallbackDirectories) {
        if (!directory.isEmpty() && !paths.contains(directory)) {
            paths.append(directory);
        }
    }
    return paths;
}

bool installCatalog(QGuiApplication &app,
                    QTranslator &translator,
                    const QString &catalog,
                    const QStringList &paths)
{
    for (const QString &path : paths) {
        if (translator.load(catalog, path)) {
            app.installTranslator(&translator);
            return true;
        }
    }
    return false;
}
}

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
    app.setOrganizationName(QStringLiteral("MeoArch"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption showOption(QStringLiteral("show"),
                                        QStringLiteral("Show the welcome flow even after completion."));
    const QCommandLineOption smokeOption(QStringLiteral("smoke"),
                                         QStringLiteral("Load the welcome flow, then exit after the startup event loop."));
    const QCommandLineOption uiLanguageOption(
        QStringLiteral("ui-language"),
        QStringLiteral("Use an explicit UI language for development and validation."),
        QStringLiteral("locale"));
    parser.addOption(showOption);
    parser.addOption(smokeOption);
    parser.addOption(uiLanguageOption);
    parser.process(app);

    // This affects only the Welcome process. It never writes a Plasma/session
    // setting, and the command-line override is intentionally non-persistent.
    const QString uiLanguage = normalizedUiLanguage(parser.value(uiLanguageOption));
    const QLocale locale(uiLanguage);
    QLocale::setDefault(locale);

    QTranslator meoUiTranslator;
    QTranslator welcomeTranslator;
    bool meoUiCatalogLoaded = false;
    bool welcomeCatalogLoaded = false;
    if (locale.name() == QStringLiteral("zh_CN")) {
        meoUiCatalogLoaded = installCatalog(
            app,
            meoUiTranslator,
            QStringLiteral("meoui_zh_CN"),
            translationPaths(
                "MEOUI_TRANSLATIONS",
                {bundledTranslationDirectory(QStringLiteral("meoui-qml")),
                 QString::fromUtf8(MEOUI_TRANSLATIONS_DEVELOPMENT_DIR),
                 QString::fromUtf8(MEOUI_TRANSLATIONS_INSTALL_DIR)}));
        welcomeCatalogLoaded = installCatalog(
            app,
            welcomeTranslator,
            QStringLiteral("meo_welcome_zh_CN"),
            translationPaths(
                "MEO_WELCOME_TRANSLATIONS",
                {bundledTranslationDirectory(QStringLiteral("meo-settings")),
                 QString::fromUtf8(MEO_WELCOME_TRANSLATIONS_BUILD_DIR),
                 QString::fromUtf8(MEO_WELCOME_TRANSLATIONS_INSTALL_DIR)}));
    }
    app.setApplicationDisplayName(QCoreApplication::translate("WelcomeApplication", "Welcome to Meo"));
    qInfo().nospace() << "Meo Welcome UI language=" << locale.name()
                      << ", MeoUI zh_CN catalog=" << meoUiCatalogLoaded
                      << ", Welcome zh_CN catalog=" << welcomeCatalogLoaded;

    WelcomeState state;
    NetworkBackend networkBackend;
    MeoAccountBackend accountBackend;
    FingerprintBackend fingerprintBackend;
    if (state.completed() && !parser.isSet(showOption)) {
        return 0;
    }

    QQmlApplicationEngine engine;
    engine.setUiLanguage(locale.bcp47Name());
#ifdef MEOUI_IMPORT_ROOT_PATH
    MeoQmlImportPolicy::prioritizeMeoUi(engine, QStringLiteral(MEOUI_IMPORT_ROOT_PATH));
#else
    MeoQmlImportPolicy::prioritizeMeoUi(engine);
#endif
    // The welcome flow receives only the privacy-safe fprintd projection. It
    // cannot read, enroll, export, or otherwise handle biometric data.
    engine.setInitialProperties({
        {QStringLiteral("welcomeState"), QVariant::fromValue(&state)},
        {QStringLiteral("networkBackend"), QVariant::fromValue(&networkBackend)},
        {QStringLiteral("accountBackend"), QVariant::fromValue(&accountBackend)},
        {QStringLiteral("fingerprintBackend"), QVariant::fromValue(&fingerprintBackend)}
    });
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
