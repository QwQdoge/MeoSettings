cmake_minimum_required(VERSION 3.21)

project(MeoSettings VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core DBus Gui Qml Quick QuickControls2 Test LinguistTools)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6NetworkManagerQt REQUIRED)
find_package(KF6BluezQt REQUIRED)
find_package(KF6Solid REQUIRED)
find_package(KF6PulseAudioQt REQUIRED)
find_package(KF6Screen REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

# MeoUI is deliberately an external design-system dependency.  This application
# does not vendor its QML components.  A packaged MeoArch system resolves it from
# Qt's normal import path; the cache variable supports an out-of-tree developer
# build of the sibling MeoUI repository.
set(MEOUI_IMPORT_ROOT "" CACHE PATH "Directory containing the built MeoUI QML module")
if(NOT MEOUI_IMPORT_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../meo-ui/out/build/release/MeoUI/qmldir")
    set(MEOUI_IMPORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../meo-ui/out/build/release")
endif()
if(MEOUI_IMPORT_ROOT AND NOT EXISTS "${MEOUI_IMPORT_ROOT}/MeoUI/qmldir")
    message(FATAL_ERROR
        "MEOUI_IMPORT_ROOT must contain MeoUI/qmldir; got: ${MEOUI_IMPORT_ROOT}")
endif()

# Settings is a MeoArch system application, not a standalone palette owner.
# The platform bridge turns the active KDE accent into a complete HCT/MD3 role
# table and installs it in MeoUI before the first visible page.  These roots
# make that explicit for sibling-repository builds; packaged builds resolve
# the same modules through Qt's normal import path.
set(MEO_KDE_QML_IMPORT_ROOT "" CACHE PATH "Directory containing the MeoKDE QML module")
if(NOT MEO_KDE_QML_IMPORT_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../meo-kde/qml/MeoKDE/qmldir")
    set(MEO_KDE_QML_IMPORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../meo-kde/qml")
endif()
if(MEO_KDE_QML_IMPORT_ROOT AND NOT EXISTS "${MEO_KDE_QML_IMPORT_ROOT}/MeoKDE/qmldir")
    message(FATAL_ERROR
        "MEO_KDE_QML_IMPORT_ROOT must contain MeoKDE/qmldir; got: ${MEO_KDE_QML_IMPORT_ROOT}")
endif()

set(MEO_SYSTEM_IMPORT_ROOT "" CACHE PATH "Directory containing the Meo.System QML plugin")
if(NOT MEO_SYSTEM_IMPORT_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../meo-kde/out/build/system/qml/Meo/System/qmldir")
    set(MEO_SYSTEM_IMPORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../meo-kde/out/build/system/qml")
endif()
if(MEO_SYSTEM_IMPORT_ROOT AND NOT EXISTS "${MEO_SYSTEM_IMPORT_ROOT}/Meo/System/qmldir")
    message(FATAL_ERROR
        "MEO_SYSTEM_IMPORT_ROOT must contain Meo/System/qmldir; got: ${MEO_SYSTEM_IMPORT_ROOT}")
endif()

qt_add_executable(meo-settings
    src/main.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/core/capabilitymanager.cpp
    src/core/capabilitymanager.h
    src/core/qmlimportpolicy.cpp
    src/core/qmlimportpolicy.h
    src/core/settingsregistry.cpp
    src/core/settingsregistry.h
    src/backends/networkbackend.cpp
    src/backends/networkbackend.h
    src/backends/bluetoothbackend.cpp
    src/backends/bluetoothbackend.h
    src/backends/bluetoothpairingagent.cpp
    src/backends/bluetoothpairingagent.h
    src/backends/audiobackend.cpp
    src/backends/audiobackend.h
    src/backends/applicationiconbackend.cpp
    src/backends/applicationiconbackend.h
    src/backends/apppermissionsbackend.cpp
    src/backends/apppermissionsbackend.h
    src/backends/displaybackend.cpp
    src/backends/displaybackend.h
    src/backends/hardwarecapabilityregistry.cpp
    src/backends/hardwarecapabilityregistry.h
    src/backends/loginauthbackend.cpp
    src/backends/loginauthbackend.h
    src/backends/recoverybackend.cpp
    src/backends/recoverybackend.h
    src/backends/configbackend.cpp
    src/backends/configbackend.h
    src/backends/fingerprintbackend.cpp
    src/backends/fingerprintbackend.h
    src/backends/documentationprovider.cpp
    src/backends/documentationprovider.h
    src/backends/systemtransactionbackend.cpp
    src/backends/systemtransactionbackend.h
    src/backends/dynamiccolorbackend.cpp
    src/backends/dynamiccolorbackend.h
    src/backends/weatherbackend.cpp
    src/backends/weatherbackend.h
    src/backends/meoaccountbackend.cpp
    src/backends/meoaccountbackend.h
    src/backends/powerbackend.cpp
    src/backends/powerbackend.h
    src/backends/repairlauncher.cpp
    src/backends/repairlauncher.h
    src/backends/systeminfobackend.cpp
    src/backends/systeminfobackend.h
    src/backends/storagebackend.cpp
    src/backends/storagebackend.h
    src/backends/omnistoreappsbackend.cpp
    src/backends/omnistoreappsbackend.h
    src/backends/packageinventorybackend.cpp
    src/backends/packageinventorybackend.h
    src/backends/updatesbackend.cpp
    src/backends/updatesbackend.h
    src/backends/controlcenterbackend.cpp
    src/backends/controlcenterbackend.h
    src/backends/shelfbackend.cpp
    src/backends/shelfbackend.h
    src/backends/shellsettingsbackend.cpp
    src/backends/shellsettingsbackend.h
    src/backends/kcmbridge.cpp
    src/backends/kcmbridge.h
    src/backends/welcomebackend.cpp
    src/backends/welcomebackend.h
)

qt_add_qml_module(meo-settings
    URI org.meo.settings
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/HomePage.qml
        qml/pages/CategoryPage.qml
        qml/pages/WifiPage.qml
        qml/pages/BluetoothPage.qml
        qml/pages/SoundPage.qml
        qml/pages/DisplayPage.qml
        qml/pages/PowerPage.qml
        qml/pages/LanguageRegionPage.qml
        qml/pages/AppearancePage.qml
        qml/pages/ControlCenterPage.qml
        qml/pages/ShelfLauncherPage.qml
        qml/pages/ShellPage.qml
        qml/pages/DesktopIntegrationPage.qml
        qml/pages/NotificationsPage.qml
        qml/pages/AccountsPage.qml
        qml/pages/PrivacyPage.qml
        qml/pages/SessionEntryPage.qml
        qml/pages/MeoLockScreenLayoutEditor.qml
        qml/pages/StoragePage.qml
        qml/pages/UpdatesPage.qml
        qml/pages/HardwarePage.qml
        qml/pages/RecoveryPage.qml
        qml/pages/SystemCenterPage.qml
        qml/pages/AboutPage.qml
        qml/pages/KcmPage.qml
        qml/components/RepairEntry.qml
)

qt_add_executable(meo-welcome
    src/welcome-main.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/networkbackend.cpp
    src/backends/networkbackend.h
    src/backends/meoaccountbackend.cpp
    src/backends/meoaccountbackend.h
    src/backends/fingerprintbackend.cpp
    src/backends/fingerprintbackend.h
    src/core/qmlimportpolicy.cpp
    src/core/qmlimportpolicy.h
)

qt_add_qml_module(meo-welcome
    URI org.meo.welcome
    VERSION 1.0
    QML_FILES qml/Welcome.qml
)

# Meo Welcome owns its product copy, while MeoUI remains the owner of shared
# component copy. Catalog loading stays in the Welcome process so an
# account-specific app preference never changes the Plasma session locale.
set(MEO_WELCOME_TRANSLATIONS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
set(MEO_SETTINGS_TRANSLATIONS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
set(MEOUI_TRANSLATIONS_DEVELOPMENT_DIR "")
if(MEOUI_IMPORT_ROOT)
    set(MEOUI_TRANSLATIONS_DEVELOPMENT_DIR "${MEOUI_IMPORT_ROOT}/translations")
endif()

qt_add_lupdate(
    SOURCE_TARGETS meo-welcome
    TS_FILES translations/meo_welcome_zh_CN.ts
    LUPDATE_TARGET meo_welcome_update_translations
)
qt_add_lrelease(
    LRELEASE_TARGET meo_welcome_lrelease
    TS_FILES translations/meo_welcome_zh_CN.ts
    QM_OUTPUT_DIRECTORY "${MEO_WELCOME_TRANSLATIONS_BUILD_DIR}"
    QM_FILES_OUTPUT_VARIABLE MEO_WELCOME_TRANSLATION_QM_FILES
)
add_dependencies(meo-welcome meo_welcome_lrelease)
target_compile_definitions(meo-welcome PRIVATE
    MEO_WELCOME_TRANSLATIONS_BUILD_DIR="${MEO_WELCOME_TRANSLATIONS_BUILD_DIR}"
    MEO_WELCOME_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/meo-settings/translations"
    MEOUI_TRANSLATIONS_DEVELOPMENT_DIR="${MEOUI_TRANSLATIONS_DEVELOPMENT_DIR}"
    MEOUI_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/meoui-qml/translations"
)

# The Settings catalog owns every shipped Settings page. Keeping this list
# complete means a later lupdate run cannot silently omit a translated route
# just because it was added after the initial shell catalog work. Welcome is a
# separate executable and deliberately keeps its own catalog above.
qt_add_lupdate(
    SOURCES
        qml/Main.qml
        qml/components/RepairEntry.qml
        qml/pages/AboutPage.qml
        qml/pages/AccountsPage.qml
        qml/pages/AppearancePage.qml
        qml/pages/BluetoothPage.qml
        qml/pages/CategoryPage.qml
        qml/pages/ControlCenterPage.qml
        qml/pages/ShelfLauncherPage.qml
        qml/pages/ShellPage.qml
        qml/pages/DesktopIntegrationPage.qml
        qml/pages/DisplayPage.qml
        qml/pages/HardwarePage.qml
        qml/pages/HomePage.qml
        qml/pages/KcmPage.qml
        qml/pages/MeoLockScreenLayoutEditor.qml
        qml/pages/WifiPage.qml
        qml/pages/SoundPage.qml
        qml/pages/NotificationsPage.qml
        qml/pages/PowerPage.qml
        qml/pages/LanguageRegionPage.qml
        qml/pages/PrivacyPage.qml
        qml/pages/RecoveryPage.qml
        qml/pages/SessionEntryPage.qml
        qml/pages/StoragePage.qml
        qml/pages/SystemCenterPage.qml
        qml/pages/UpdatesPage.qml
        src/core/settingsregistry.cpp
    TS_FILES translations/meo_settings_zh_CN.ts
    LUPDATE_TARGET meo_settings_update_shell_translations
)
qt_add_lrelease(
    LRELEASE_TARGET meo_settings_lrelease
    TS_FILES translations/meo_settings_zh_CN.ts
    QM_OUTPUT_DIRECTORY "${MEO_SETTINGS_TRANSLATIONS_BUILD_DIR}"
    QM_FILES_OUTPUT_VARIABLE MEO_SETTINGS_TRANSLATION_QM_FILES
)
add_dependencies(meo-settings meo_settings_lrelease)

target_link_libraries(meo-settings PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::CoreAddons
    KF6::NetworkManagerQt
    KF6::BluezQt
    KF6::Solid
    KF6::PulseAudioQt
    KF6::Screen
)

target_link_libraries(meo-welcome PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::NetworkManagerQt
)

target_compile_definitions(meo-settings PRIVATE
    MEO_SETTINGS_VERSION="${PROJECT_VERSION}"
    MEO_SETTINGS_TRANSLATIONS_BUILD_DIR="${MEO_SETTINGS_TRANSLATIONS_BUILD_DIR}"
    MEO_SETTINGS_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/meo-settings/translations"
    MEOUI_TRANSLATIONS_DEVELOPMENT_DIR="${MEOUI_TRANSLATIONS_DEVELOPMENT_DIR}"
    MEOUI_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/meoui-qml/translations"
)

if(MEOUI_IMPORT_ROOT)
    target_compile_definitions(meo-settings PRIVATE
        MEOUI_IMPORT_ROOT_PATH="${MEOUI_IMPORT_ROOT}"
    )
    set_property(TARGET meo-settings APPEND PROPERTY QT_QML_IMPORT_PATH "${MEOUI_IMPORT_ROOT}")
    target_compile_definitions(meo-welcome PRIVATE MEOUI_IMPORT_ROOT_PATH="${MEOUI_IMPORT_ROOT}")
    set_property(TARGET meo-welcome APPEND PROPERTY QT_QML_IMPORT_PATH "${MEOUI_IMPORT_ROOT}")
endif()

if(MEO_KDE_QML_IMPORT_ROOT)
    target_compile_definitions(meo-settings PRIVATE
        MEO_KDE_QML_IMPORT_ROOT_PATH="${MEO_KDE_QML_IMPORT_ROOT}"
    )
    set_property(TARGET meo-settings APPEND PROPERTY QT_QML_IMPORT_PATH "${MEO_KDE_QML_IMPORT_ROOT}")
endif()

if(MEO_SYSTEM_IMPORT_ROOT)
    target_compile_definitions(meo-settings PRIVATE
        MEO_SYSTEM_IMPORT_ROOT_PATH="${MEO_SYSTEM_IMPORT_ROOT}"
    )
    set_property(TARGET meo-settings APPEND PROPERTY QT_QML_IMPORT_PATH "${MEO_SYSTEM_IMPORT_ROOT}")
endif()

add_executable(meo-settings-registry-test
    tests/tst_registry.cpp
    src/core/settingsregistry.cpp
    src/core/settingsregistry.h
)
target_link_libraries(meo-settings-registry-test PRIVATE Qt6::Core Qt6::Test)
add_dependencies(meo-settings-registry-test meo_settings_lrelease)
target_compile_definitions(meo-settings-registry-test PRIVATE
    MEO_SETTINGS_TRANSLATIONS_BUILD_DIR="${MEO_SETTINGS_TRANSLATIONS_BUILD_DIR}"
)
add_test(NAME meo-settings-registry COMMAND meo-settings-registry-test)

add_executable(meo-settings-powerbackend-test
    tests/tst_powerbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/powerbackend.cpp
    src/backends/powerbackend.h
)
target_link_libraries(meo-settings-powerbackend-test PRIVATE Qt6::Core Qt6::Test KF6::Solid)
add_test(NAME meo-settings-powerbackend COMMAND meo-settings-powerbackend-test)

add_executable(meo-settings-storagebackend-test
    tests/tst_storagebackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/storagebackend.cpp
    src/backends/storagebackend.h
)
target_link_libraries(meo-settings-storagebackend-test PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Test
    KF6::Solid
)
add_test(NAME meo-settings-storagebackend COMMAND meo-settings-storagebackend-test)

add_executable(meo-settings-omnistoreappsbackend-test
    tests/tst_omnistoreappsbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/omnistoreappsbackend.cpp
    src/backends/omnistoreappsbackend.h
)
target_link_libraries(meo-settings-omnistoreappsbackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-omnistoreappsbackend COMMAND meo-settings-omnistoreappsbackend-test)

add_executable(meo-settings-packageinventorybackend-test
    tests/tst_packageinventorybackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/packageinventorybackend.cpp
    src/backends/packageinventorybackend.h
)
target_link_libraries(meo-settings-packageinventorybackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-packageinventorybackend COMMAND meo-settings-packageinventorybackend-test)

add_executable(meo-settings-controlcenterbackend-test
    tests/tst_controlcenterbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/controlcenterbackend.cpp
    src/backends/controlcenterbackend.h
)
target_link_libraries(meo-settings-controlcenterbackend-test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME meo-settings-controlcenterbackend COMMAND meo-settings-controlcenterbackend-test)

add_executable(meo-settings-shellsettingsbackend-test
    tests/tst_shellsettingsbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/shellsettingsbackend.cpp
    src/backends/shellsettingsbackend.h
)
target_link_libraries(meo-settings-shellsettingsbackend-test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME meo-settings-shellsettingsbackend COMMAND meo-settings-shellsettingsbackend-test)

add_executable(meo-settings-bluetoothbackend-test
    tests/tst_bluetoothbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/bluetoothbackend.cpp
    src/backends/bluetoothbackend.h
    src/backends/bluetoothpairingagent.cpp
    src/backends/bluetoothpairingagent.h
)
target_link_libraries(meo-settings-bluetoothbackend-test PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Test
    KF6::BluezQt
)
add_test(NAME meo-settings-bluetoothbackend COMMAND meo-settings-bluetoothbackend-test)

add_executable(meo-settings-dynamiccolorbackend-test
    tests/tst_dynamiccolorbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/dynamiccolorbackend.cpp
    src/backends/dynamiccolorbackend.h
)
target_link_libraries(meo-settings-dynamiccolorbackend-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME meo-settings-dynamiccolorbackend COMMAND meo-settings-dynamiccolorbackend-test)

add_executable(meo-settings-weatherbackend-test
    tests/tst_weatherbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/weatherbackend.cpp
    src/backends/weatherbackend.h
)
target_link_libraries(meo-settings-weatherbackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-weatherbackend COMMAND meo-settings-weatherbackend-test)

add_executable(meo-settings-applicationiconbackend-test
    tests/tst_applicationiconbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/applicationiconbackend.cpp
    src/backends/applicationiconbackend.h
)
target_link_libraries(meo-settings-applicationiconbackend-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME meo-settings-applicationiconbackend COMMAND meo-settings-applicationiconbackend-test)

add_executable(meo-settings-apppermissionsbackend-test
    tests/tst_apppermissionsbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/apppermissionsbackend.cpp
    src/backends/apppermissionsbackend.h
)
target_link_libraries(meo-settings-apppermissionsbackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-apppermissionsbackend COMMAND meo-settings-apppermissionsbackend-test)

add_executable(meo-settings-updatesbackend-test
    tests/tst_updatesbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/updatesbackend.cpp
    src/backends/updatesbackend.h
)
target_link_libraries(meo-settings-updatesbackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-updatesbackend COMMAND meo-settings-updatesbackend-test)

add_executable(meo-settings-configbackend-test
    tests/tst_configbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/configbackend.cpp
    src/backends/configbackend.h
)
target_link_libraries(meo-settings-configbackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-configbackend COMMAND meo-settings-configbackend-test)

add_executable(meo-settings-meoaccountbackend-test
    tests/tst_meoaccountbackend.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/meoaccountbackend.cpp
    src/backends/meoaccountbackend.h
)
target_link_libraries(meo-settings-meoaccountbackend-test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME meo-settings-meoaccountbackend COMMAND meo-settings-meoaccountbackend-test)

add_executable(meo-settings-fingerprintbackend-test
    tests/tst_fingerprintbackend.cpp
    src/backends/fingerprintbackend.cpp
    src/backends/fingerprintbackend.h
)
target_link_libraries(meo-settings-fingerprintbackend-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-fingerprintbackend COMMAND meo-settings-fingerprintbackend-test)

add_executable(meo-settings-qmlimportpolicy-test
    tests/tst_qmlimportpolicy.cpp
    src/core/qmlimportpolicy.cpp
    src/core/qmlimportpolicy.h
)
target_include_directories(meo-settings-qmlimportpolicy-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(meo-settings-qmlimportpolicy-test PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
add_test(NAME meo-settings-qmlimportpolicy COMMAND meo-settings-qmlimportpolicy-test)

add_executable(meo-settings-welcomereadiness-test
    tests/tst_welcomereadiness.cpp
)
target_compile_definitions(meo-settings-welcomereadiness-test PRIVATE
    MEO_SETTINGS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(meo-settings-welcomereadiness-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-welcomereadiness COMMAND meo-settings-welcomereadiness-test)

add_executable(meo-settings-repairlauncher-test
    tests/tst_repairlauncher.cpp
    src/core/backendbase.cpp
    src/core/backendbase.h
    src/backends/repairlauncher.cpp
    src/backends/repairlauncher.h
)
target_include_directories(meo-settings-repairlauncher-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(meo-settings-repairlauncher-test PRIVATE
    MEO_SETTINGS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(meo-settings-repairlauncher-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME meo-settings-repairlauncher COMMAND meo-settings-repairlauncher-test)

if(MEOUI_IMPORT_ROOT AND MEO_KDE_QML_IMPORT_ROOT AND MEO_SYSTEM_IMPORT_ROOT)
    # --smoke walks every first-milestone route without performing a hardware
    # action or launching a KCM. Keep this in CTest so type collisions and
    # lazy page creation failures cannot hide behind the initial home page.
    add_test(NAME meo-settings-qml-smoke COMMAND $<TARGET_FILE:meo-settings> --smoke)
    set_property(TEST meo-settings-qml-smoke APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QML_IMPORT_PATH=${MEOUI_IMPORT_ROOT}:${MEO_KDE_QML_IMPORT_ROOT}:${MEO_SYSTEM_IMPORT_ROOT}"
        "QT_FORCE_STDERR_LOGGING=1"
    )

    # Keep the editor reachable in CTest independently of a pointer-driven
    # manual check. It remains in-session and never invokes a config writer.
    add_test(NAME meo-settings-lockscreen-layout-editor-smoke
             COMMAND $<TARGET_FILE:meo-settings> --session-entry-layout-editor --smoke)
    set_property(TEST meo-settings-lockscreen-layout-editor-smoke APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QML_IMPORT_PATH=${MEOUI_IMPORT_ROOT}:${MEO_KDE_QML_IMPORT_ROOT}:${MEO_SYSTEM_IMPORT_ROOT}"
        "QT_FORCE_STDERR_LOGGING=1"
    )

    add_test(NAME meo-welcome-qml-smoke COMMAND $<TARGET_FILE:meo-welcome> --show --smoke)
    set_property(TEST meo-welcome-qml-smoke APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QML_IMPORT_PATH=${MEOUI_IMPORT_ROOT}:${MEO_KDE_QML_IMPORT_ROOT}:${MEO_SYSTEM_IMPORT_ROOT}"
        "QT_FORCE_STDERR_LOGGING=1"
    )

    # The normal route smoke instantiates SettingsTaskSheet but does not open
    # it. Exercise its adaptive compact presentation separately so a desktop
    # side sheet cannot silently regress into a clipped mobile surface.
    find_program(QML6_EXECUTABLE qml6)
    if(QML6_EXECUTABLE)
        add_test(NAME meoui-settings-task-sheet-compact
                 COMMAND ${QML6_EXECUTABLE}
                         -I ${MEOUI_IMPORT_ROOT}
                         -f
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/SettingsTaskSheetCompactSmoke.qml)
        set_property(TEST meoui-settings-task-sheet-compact APPEND PROPERTY ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen"
            "QT_QUICK_BACKEND=software"
            "QML_IMPORT_PATH=${MEOUI_IMPORT_ROOT}"
            "QT_FORCE_STDERR_LOGGING=1"
        )

        # Exercise Plasma's real Settings QML API in an isolated KConfig
        # location. This writes only a build-directory plasmanotifyrc, never
        # the active user's notification preferences.
        add_test(NAME meo-settings-notification-settings-api
                 COMMAND ${QML6_EXECUTABLE}
                         -f
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/NotificationSettingsApiSmoke.qml)
        set_property(TEST meo-settings-notification-settings-api APPEND PROPERTY ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen"
            "QT_QUICK_BACKEND=software"
            "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/notification-settings-test-config"
            "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/notification-settings-test-data"
            "QT_FORCE_STDERR_LOGGING=1"
        )
    endif()
elseif(MEOUI_IMPORT_ROOT)
    message(WARNING "Skipping dynamic Settings QML smoke test because the MeoKDE bridge or Meo.System plugin import root is unavailable.")
endif()

install(TARGETS meo-settings
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(TARGETS meo-welcome
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(FILES
    ${MEO_WELCOME_TRANSLATION_QM_FILES}
    ${MEO_SETTINGS_TRANSLATION_QM_FILES}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/meo-settings/translations"
)
install(FILES
    data/org.meo.settings.desktop
    data/org.meo.settings.bluetooth.desktop
    data/org.meo.settings.wifi.desktop
    data/org.meo.settings.sound.desktop
    data/org.meo.settings.display.desktop
    data/org.meo.settings.notifications.desktop
    data/org.meo.settings.power.desktop
    data/org.meo.welcome.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
# Original application identities remain package-owned artwork. The Studio
# package owns only its reviewed source sheet and generated user overlay, so a
# normal FreeDesktop lookup keeps resolving the real Meo app after Undo.
install(FILES
    data/icons/hicolor/scalable/apps/org.meo.settings.svg
    data/icons/hicolor/scalable/apps/org.meo.welcome.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
)
install(FILES
    data/icons/hicolor/16x16/apps/org.meo.settings.svg
    data/icons/hicolor/16x16/apps/org.meo.welcome.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps"
)
install(FILES
    data/icons/hicolor/22x22/apps/org.meo.settings.svg
    data/icons/hicolor/22x22/apps/org.meo.welcome.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/22x22/apps"
)
install(FILES
    data/icons/hicolor/32x32/apps/org.meo.settings.svg
    data/icons/hicolor/32x32/apps/org.meo.welcome.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps"
)
install(FILES data/meo-account/clients/org.meo.Settings.json
    DESTINATION "${CMAKE_INSTALL_DATADIR}/meo-account/clients"
)
install(FILES LICENSE
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/meo-settings"
)
