#include "backends/audiobackend.h"
#include "backends/applicationiconbackend.h"
#include "backends/apppermissionsbackend.h"
#include "backends/bluetoothbackend.h"
#include "backends/controlcenterbackend.h"
#include "backends/shelfbackend.h"
#include "backends/shellsettingsbackend.h"
#include "backends/dynamiccolorbackend.h"
#include "backends/displaybackend.h"
#include "backends/documentationprovider.h"
#include "backends/fingerprintbackend.h"
#include "backends/hardwarecapabilityregistry.h"
#include "backends/kcmbridge.h"
#include "backends/loginauthbackend.h"
#include "backends/meoaccountbackend.h"
#include "backends/networkbackend.h"
#include "backends/omnistoreappsbackend.h"
#include "backends/packageinventorybackend.h"
#include "backends/powerbackend.h"
#include "backends/repairlauncher.h"
#include "backends/recoverybackend.h"
#include "backends/storagebackend.h"
#include "backends/systeminfobackend.h"
#include "backends/systemtransactionbackend.h"
#include "backends/configbackend.h"
#include "backends/updatesbackend.h"
#include "backends/welcomebackend.h"
#include "backends/weatherbackend.h"
#include "core/capabilitymanager.h"
#include "core/qmlimportpolicy.h"
#include "core/settingsregistry.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QLocale>
#include <QRegularExpression>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QTranslator>

#include <cstdio>
#include <memory>

namespace
{
QString normalizedUiLanguage(const QString &requestedLanguage)
{
    const QLocale requestedLocale(requestedLanguage.trimmed().isEmpty()
                                      ? QLocale::system()
                                      : QLocale(requestedLanguage));
    if (requestedLocale.language() == QLocale::Chinese) {
        // Meo Settings currently ships a Simplified Chinese catalog. Resolve
        // all Chinese system locales to it rather than silently falling back
        // to English for a Chinese-language desktop.
        return QStringLiteral("zh_CN");
    }
    const QString name = requestedLocale.name();
    return name == QStringLiteral("C") ? QStringLiteral("en_US") : name;
}

QString bundledTranslationDirectory()
{
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/meoui-qml/translations")));
}

QString bundledSettingsTranslationDirectory()
{
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/meo-settings/translations")));
}

bool installCatalog(QGuiApplication &app,
                    QTranslator &translator,
                    const QString &catalog,
                    const QStringList &paths)
{
    for (const QString &path : paths) {
        if (!path.isEmpty() && translator.load(catalog, path)) {
            app.installTranslator(&translator);
            return true;
        }
    }
    return false;
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Meo Settings"));
    app.setApplicationDisplayName(QStringLiteral("Meo Settings"));
    app.setDesktopFileName(QStringLiteral("org.meo.settings"));
    app.setOrganizationName(QStringLiteral("MeoArch"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meo Settings"));
    parser.addHelpOption();
    const QCommandLineOption smokeOption(QStringLiteral("smoke"),
                                          QStringLiteral("Load the application, then exit after the startup event loop."));
    const QCommandLineOption routeOption(QStringLiteral("route"),
                                          QStringLiteral("Open a route without invoking any setting action."),
                                          QStringLiteral("route"));
    const QCommandLineOption sizeOption(QStringLiteral("size"),
                                         QStringLiteral("Set a validation window size as WIDTHxHEIGHT."),
                                         QStringLiteral("size"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                               QStringLiteral("Save a non-interactive application screenshot, then exit."),
                                               QStringLiteral("file"));
    const QCommandLineOption sessionEntryLayoutEditorOption(
        QStringLiteral("session-entry-layout-editor"),
        QStringLiteral("Open the non-persistent lock-screen layout editor for validation."));
    const QCommandLineOption uiLanguageOption(
        QStringLiteral("ui-language"),
        QStringLiteral("Use an explicit UI language for development and validation."),
        QStringLiteral("locale"));
    parser.addOption(smokeOption);
    parser.addOption(routeOption);
    parser.addOption(sizeOption);
    parser.addOption(screenshotOption);
    parser.addOption(sessionEntryLayoutEditorOption);
    parser.addOption(uiLanguageOption);
    parser.process(app);

    // The default follows the device locale. The command-line choice exists
    // only for isolated validation and is not persisted or written into the
    // Plasma session, system locale, or Account service.
    const QLocale uiLocale(normalizedUiLanguage(parser.value(uiLanguageOption)));
    QLocale::setDefault(uiLocale);
    QTranslator meoUiTranslator;
    QTranslator meoSettingsTranslator;
    bool meoUiCatalogLoaded = false;
    bool meoSettingsCatalogLoaded = false;
    if (uiLocale.name() == QStringLiteral("zh_CN")) {
        QStringList translationPaths;
        const QString configuredTranslations = qEnvironmentVariable("MEOUI_TRANSLATIONS").trimmed();
        if (!configuredTranslations.isEmpty()) {
            translationPaths.append(configuredTranslations);
        }
        translationPaths.append(bundledTranslationDirectory());
#ifdef MEOUI_TRANSLATIONS_DEVELOPMENT_DIR
        translationPaths.append(QString::fromUtf8(MEOUI_TRANSLATIONS_DEVELOPMENT_DIR));
#endif
#ifdef MEOUI_TRANSLATIONS_INSTALL_DIR
        translationPaths.append(QString::fromUtf8(MEOUI_TRANSLATIONS_INSTALL_DIR));
#endif
        meoUiCatalogLoaded = installCatalog(app, meoUiTranslator,
                                            QStringLiteral("meoui_zh_CN"),
                                            translationPaths);

        QStringList settingsTranslationPaths;
        const QString configuredSettingsTranslations = qEnvironmentVariable("MEO_SETTINGS_TRANSLATIONS").trimmed();
        if (!configuredSettingsTranslations.isEmpty()) {
            settingsTranslationPaths.append(configuredSettingsTranslations);
        }
        settingsTranslationPaths.append(bundledSettingsTranslationDirectory());
#ifdef MEO_SETTINGS_TRANSLATIONS_BUILD_DIR
        settingsTranslationPaths.append(QString::fromUtf8(MEO_SETTINGS_TRANSLATIONS_BUILD_DIR));
#endif
#ifdef MEO_SETTINGS_TRANSLATIONS_INSTALL_DIR
        settingsTranslationPaths.append(QString::fromUtf8(MEO_SETTINGS_TRANSLATIONS_INSTALL_DIR));
#endif
        meoSettingsCatalogLoaded = installCatalog(app, meoSettingsTranslator,
                                                  QStringLiteral("meo_settings_zh_CN"),
                                                  settingsTranslationPaths);
    }
    qInfo().nospace() << "Meo Settings UI language=" << uiLocale.name()
                      << ", MeoUI zh_CN catalog=" << meoUiCatalogLoaded
                      << ", Meo Settings zh_CN catalog=" << meoSettingsCatalogLoaded;

    QSize requestedSize;
    if (parser.isSet(sizeOption)) {
        const auto match = QRegularExpression(QStringLiteral("^(\\d+)x(\\d+)$"))
                               .match(parser.value(sizeOption));
        if (!match.hasMatch()) {
            qCritical() << "--size must use WIDTHxHEIGHT";
            return EXIT_FAILURE;
        }
        requestedSize = QSize(match.captured(1).toInt(), match.captured(2).toInt());
        if (requestedSize.width() < 1 || requestedSize.height() < 1) {
            qCritical() << "--size must be positive";
            return EXIT_FAILURE;
        }
    }

    SettingsRegistry registry;
    NetworkBackend networkBackend;
    BluetoothBackend bluetoothBackend;
    AudioBackend audioBackend;
    ApplicationIconBackend applicationIconBackend;
    AppPermissionsBackend appPermissionsBackend;
    DisplayBackend displayBackend;
    HardwareCapabilityRegistry hardwareCapabilityRegistry;
    LoginAuthBackend loginAuthBackend;
    RecoveryBackend recoveryBackend;
    ConfigBackend configBackend;
    FingerprintBackend fingerprintBackend;
    DocumentationProvider documentationProvider;
    SystemTransactionBackend systemTransactionBackend;
    DynamicColorBackend dynamicColorBackend;
    WeatherBackend weatherBackend;
    MeoAccountBackend meoAccountBackend;
    PowerBackend powerBackend;
    RepairLauncher repairLauncher;
    SystemInfoBackend systemInfoBackend;
    StorageBackend storageBackend;
    OmniStoreAppsBackend omniStoreAppsBackend;
    PackageInventoryBackend packageInventoryBackend;
    UpdatesBackend updatesBackend;
    ControlCenterBackend controlCenterBackend;
    ShelfBackend shelfBackend;
    ShellSettingsBackend shellSettingsBackend;
    KcmBridge kcmBridge;
    WelcomeBackend welcomeBackend;
    CapabilityManager capabilities(&networkBackend, &bluetoothBackend, &audioBackend, &displayBackend, &powerBackend);

    QQmlApplicationEngine engine;
    engine.setUiLanguage(uiLocale.bcp47Name());
#ifdef MEOUI_IMPORT_ROOT_PATH
    MeoQmlImportPolicy::prioritizeMeoUi(engine, QStringLiteral(MEOUI_IMPORT_ROOT_PATH));
#else
    MeoQmlImportPolicy::prioritizeMeoUi(engine);
#endif
#ifdef MEO_KDE_QML_IMPORT_ROOT_PATH
    // MeoShellTheme is the KDE-side dynamic HCT bridge.  MeoUI consumes its
    // complete role table but intentionally remains platform-neutral.
    engine.addImportPath(QStringLiteral(MEO_KDE_QML_IMPORT_ROOT_PATH));
#endif
#ifdef MEO_SYSTEM_IMPORT_ROOT_PATH
    // Development builds load the same Material color provider that the
    // packaged MeoKDE module uses.  Installed systems use Qt's normal path.
    engine.addImportPath(QStringLiteral(MEO_SYSTEM_IMPORT_ROOT_PATH));
#endif
    auto *context = engine.rootContext();
    context->setContextProperty(QStringLiteral("SettingsRegistry"), &registry);
    context->setContextProperty(QStringLiteral("Capabilities"), &capabilities);
    context->setContextProperty(QStringLiteral("NetworkBackend"), &networkBackend);
    context->setContextProperty(QStringLiteral("BluetoothBackend"), &bluetoothBackend);
    context->setContextProperty(QStringLiteral("AudioBackend"), &audioBackend);
    context->setContextProperty(QStringLiteral("ApplicationIconBackend"), &applicationIconBackend);
    context->setContextProperty(QStringLiteral("AppPermissionsBackend"), &appPermissionsBackend);
    context->setContextProperty(QStringLiteral("DisplayBackend"), &displayBackend);
    context->setContextProperty(QStringLiteral("HardwareCapabilities"), &hardwareCapabilityRegistry);
    context->setContextProperty(QStringLiteral("LoginAuthBackend"), &loginAuthBackend);
    context->setContextProperty(QStringLiteral("RecoveryBackend"), &recoveryBackend);
    context->setContextProperty(QStringLiteral("ConfigBackend"), &configBackend);
    context->setContextProperty(QStringLiteral("FingerprintBackend"), &fingerprintBackend);
    context->setContextProperty(QStringLiteral("DocumentationProvider"), &documentationProvider);
    context->setContextProperty(QStringLiteral("SystemTransactionBackend"), &systemTransactionBackend);
    context->setContextProperty(QStringLiteral("DynamicColorBackend"), &dynamicColorBackend);
    context->setContextProperty(QStringLiteral("WeatherBackend"), &weatherBackend);
    context->setContextProperty(QStringLiteral("AccountBackend"), &meoAccountBackend);
    context->setContextProperty(QStringLiteral("PowerBackend"), &powerBackend);
    context->setContextProperty(QStringLiteral("RepairLauncher"), &repairLauncher);
    context->setContextProperty(QStringLiteral("SystemInfoBackend"), &systemInfoBackend);
    context->setContextProperty(QStringLiteral("StorageBackend"), &storageBackend);
    context->setContextProperty(QStringLiteral("OmniStoreAppsBackend"), &omniStoreAppsBackend);
    context->setContextProperty(QStringLiteral("PackageInventoryBackend"), &packageInventoryBackend);
    context->setContextProperty(QStringLiteral("UpdatesBackend"), &updatesBackend);
    context->setContextProperty(QStringLiteral("ControlCenterBackend"), &controlCenterBackend);
    context->setContextProperty(QStringLiteral("ShelfBackend"), &shelfBackend);
    context->setContextProperty(QStringLiteral("ShellSettingsBackend"), &shellSettingsBackend);
    context->setContextProperty(QStringLiteral("KcmBridge"), &kcmBridge);
    context->setContextProperty(QStringLiteral("WelcomeBackend"), &welcomeBackend);
    // This mode exists solely for isolated CTest and screenshot validation of
    // the data-only editor. It exposes no system writer or lock-screen state.
    context->setContextProperty(QStringLiteral("sessionEntryLayoutEditorMode"),
                                parser.isSet(sessionEntryLayoutEditorOption));
    QObject::connect(&configBackend, &ConfigBackend::transactionRequested,
                     &systemTransactionBackend, &SystemTransactionBackend::submitConfigurationRequest);

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const auto &warning : warnings) {
                             std::fprintf(stderr, "%s\n", qPrintable(warning.toString()));
                             qWarning().noquote() << warning.toString();
                         }
                     });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] {
                         std::fputs("Meo Settings could not create its QML root object.\n", stderr);
                         qCritical() << "Meo Settings could not create its QML root object.";
                         QCoreApplication::exit(EXIT_FAILURE);
                     }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("org.meo.settings"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    auto *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    if (!requestedSize.isEmpty() && window) {
        window->resize(requestedSize);
    }
    const QString startupRoute = parser.isSet(routeOption)
                                   ? parser.value(routeOption)
                                   : (parser.isSet(sessionEntryLayoutEditorOption)
                                      ? QStringLiteral("session-entry") : QString());
    if (!startupRoute.isEmpty()
        && !QMetaObject::invokeMethod(root, "navigate",
                                      Q_ARG(QVariant, QVariant(startupRoute)))) {
        qCritical() << "Meo Settings could not navigate to" << startupRoute;
        return EXIT_FAILURE;
    }

    if (parser.isSet(screenshotOption)) {
        if (!window) {
            qCritical() << "Meo Settings root is not a QQuickWindow.";
            return EXIT_FAILURE;
        }
        const auto screenshotPath = parser.value(screenshotOption);
        QTimer::singleShot(1200, window, [window, screenshotPath, &app] {
            const QImage image = window->grabWindow();
            if (!image.isNull() && image.save(screenshotPath)) {
                qInfo() << "Saved Meo Settings visual validation image to" << screenshotPath;
                app.quit();
                return;
            }
            qCritical() << "Could not save Meo Settings visual validation image to" << screenshotPath;
            app.exit(EXIT_FAILURE);
        });
        QTimer::singleShot(8000, &app, [&app] {
            qCritical() << "Timed out waiting for Meo Settings screenshot capture.";
            app.exit(EXIT_FAILURE);
        });
    }

    if (parser.isSet(smokeOption)) {
        // Instantiate every first-milestone page, including the non-launching
        // KCM fallback page.  This is deliberately navigation-only: it must
        // never connect a network, pair a device, alter audio, or open a KCM.
        const QStringList smokeRoutes{
            QStringLiteral("category:network"),
            QStringLiteral("wifi"),
            QStringLiteral("kcm:kcm_proxy"),
            QStringLiteral("category:devices"),
            QStringLiteral("bluetooth"),
            QStringLiteral("sound"),
            QStringLiteral("category:display-sound"),
            QStringLiteral("display"),
            QStringLiteral("power"),
            QStringLiteral("kcm:kcm_keyboard"),
            QStringLiteral("category:personalization"),
            QStringLiteral("appearance"),
            QStringLiteral("kcm:kcm_lookandfeel"),
            QStringLiteral("category:apps"),
            QStringLiteral("notifications"),
            QStringLiteral("control-center"),
            QStringLiteral("desktop-integration"),
            QStringLiteral("kcm:kcm_componentchooser"),
            QStringLiteral("category:accounts"),
            QStringLiteral("accounts"),
            QStringLiteral("category:storage"),
            QStringLiteral("storage"),
            QStringLiteral("category:system"),
            QStringLiteral("hardware"),
            QStringLiteral("recovery"),
            QStringLiteral("system-center"),
            QStringLiteral("kcm:kcm_kscreen"),
            QStringLiteral("category:privacy"),
            QStringLiteral("privacy"),
            QStringLiteral("session-entry"),
            QStringLiteral("category:accessibility"),
            QStringLiteral("category:updates"),
            QStringLiteral("updates"),
            QStringLiteral("about"),
            QStringLiteral("home"),
        };
        auto routeIndex = std::make_shared<int>(0);
        auto notReadyTicks = std::make_shared<int>(0);
        auto *smokeTimer = new QTimer(&app);
        smokeTimer->setInterval(420);
        QObject::connect(smokeTimer, &QTimer::timeout, &app,
                         [&engine, smokeRoutes, routeIndex, notReadyTicks, smokeTimer] {
                             if (engine.rootObjects().isEmpty()) {
                                 qCritical() << "Meo Settings lost its QML root during smoke navigation.";
                                 QCoreApplication::exit(EXIT_FAILURE);
                                 return;
                             }
                             auto *rootObject = engine.rootObjects().constFirst();
                             if (!rootObject->property("pageContentReady").toBool()) {
                                 // A cold QML page can legitimately need more
                                 // than one 420 ms tick while its module and
                                 // live read-only backend state initialize.
                                 // Wait for the explicit page-ready contract
                                 // instead of making the smoke result depend
                                 // on machine load; still fail if it never
                                 // settles.
                                 ++*notReadyTicks;
                                 if (*notReadyTicks > 12) {
                                     qCritical() << "Meo Settings page host did not retain the current page during smoke navigation."
                                                 << "route:" << rootObject->property("currentRoute").toString()
                                                 << "loaded:" << rootObject->property("lastLoadedRoute").toString();
                                     QCoreApplication::exit(EXIT_FAILURE);
                                 }
                                 return;
                             }
                             *notReadyTicks = 0;
                             if (!rootObject->property("dynamicThemeReady").toBool()) {
                                 qCritical() << "Meo Settings did not receive a complete KDE dynamic color scheme."
                                             << "mode:" << rootObject->property("dynamicThemeMode").toString();
                                 QCoreApplication::exit(EXIT_FAILURE);
                                 return;
                             }
                             if (*routeIndex >= smokeRoutes.size()) {
                                 smokeTimer->stop();
                                 QCoreApplication::quit();
                                 return;
                             }
                             if (!QMetaObject::invokeMethod(rootObject, "navigate",
                                                               Q_ARG(QVariant, QVariant(smokeRoutes.at(*routeIndex))))) {
                                 qCritical() << "Meo Settings smoke navigation failed for" << smokeRoutes.at(*routeIndex);
                                 QCoreApplication::exit(EXIT_FAILURE);
                                 return;
                             }
                             ++*routeIndex;
                         });
        smokeTimer->start();
    }

    return app.exec();
}
