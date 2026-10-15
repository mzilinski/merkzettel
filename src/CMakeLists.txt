# Everything without UI dependencies lives in a static library so the unit
# tests can link it without the QML front end.
add_library(merkzettel_core STATIC
    auth/authmanager.cpp
    auth/tokenstore.cpp
    graph/graphclient.cpp
    graph/todoapi.cpp
    models/tasksmodel.cpp
    models/tasklistsmodel.cpp
    cache/database.cpp
    util/dateparser.cpp
)

target_compile_definitions(merkzettel_core PUBLIC
    MERKZETTEL_CLIENT_ID="${MERKZETTEL_CLIENT_ID}"
    MERKZETTEL_TENANT="${MERKZETTEL_TENANT}"
    MERKZETTEL_VERSION="${PROJECT_VERSION}"
)

target_include_directories(merkzettel_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(merkzettel_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::NetworkAuth
    Qt6::Sql
    KF6::I18n
    KF6::ConfigCore
    qt6keychain
)

add_executable(merkzettel
    main.cpp
    app.cpp
    tray/trayicon.cpp
)

target_compile_definitions(merkzettel PRIVATE
    MERKZETTEL_BUILD_LOCALE_DIR="${CMAKE_BINARY_DIR}/po"
)

target_link_libraries(merkzettel PRIVATE
    merkzettel_core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::GlobalAccel
    KF6::Notifications
    KF6::StatusNotifierItem
    KF6::ConfigGui
    KF6::ColorScheme
)

ecm_add_qml_module(merkzettel
    URI io.github.mzilinski.merkzettel
    VERSION 1.0
    QML_FILES
        Main.qml
        LoginPage.qml
        TasksPage.qml
        TaskDelegate.qml
        TaskDetailSheet.qml
        MoveToListMenu.qml
        CustomRecurrenceDialog.qml
)

install(TARGETS merkzettel ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
