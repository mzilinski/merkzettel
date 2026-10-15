cmake_minimum_required(VERSION 3.20)

project(merkzettel VERSION 0.4.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.6.0)
set(KF_MIN_VERSION 6.8.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)
include(ECMAddTests)

option(MERKZETTEL_WERROR "Treat compiler warnings as errors (CI)" OFF)
if(MERKZETTEL_WERROR)
    add_compile_options(-Werror)
endif()

option(MERKZETTEL_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(MERKZETTEL_SANITIZERS)
    set(ECM_ENABLE_SANITIZERS "address;undefined")
    include(ECMEnableSanitizers)
endif()

include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Network
    NetworkAuth
    Sql
    Widgets
)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    DBusAddons
    GlobalAccel
    I18n
    Notifications
    StatusNotifierItem
    Config
    ConfigWidgets
    ColorScheme
    Kirigami
)

find_package(Qt6Keychain REQUIRED)

# Azure App Registration (Public Client / native).
# Override at configure time:
#   cmake -DMERKZETTEL_CLIENT_ID=xxxx -DMERKZETTEL_TENANT=common ..
set(MERKZETTEL_CLIENT_ID "" CACHE STRING "Azure App Registration client_id")
set(MERKZETTEL_TENANT "common" CACHE STRING "Azure tenant: common, organizations, consumers, or tenant-id")

if(NOT MERKZETTEL_CLIENT_ID)
    message(WARNING
        "MERKZETTEL_CLIENT_ID is empty. The app will build but cannot log in. "
        "See docs/azure-setup.md to register an app, then re-run cmake with "
        "-DMERKZETTEL_CLIENT_ID=<your-id>.")
endif()

add_subdirectory(src)
add_subdirectory(po)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(MERKZETTEL_APP_ID io.github.mzilinski.merkzettel)
install(FILES ${MERKZETTEL_APP_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${MERKZETTEL_APP_ID}.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES icons/${MERKZETTEL_APP_ID}.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES src/merkzettel.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
