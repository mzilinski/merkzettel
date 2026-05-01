add_executable(merkzettel
    main.cpp
    app.cpp
    auth/authmanager.cpp
    auth/tokenstore.cpp
    graph/graphclient.cpp
    graph/todoapi.cpp
    models/tasksmodel.cpp
    models/tasklistsmodel.cpp
    cache/database.cpp
    tray/trayicon.cpp
)

target_compile_definitions(merkzettel PRIVATE
    MERKZETTEL_CLIENT_ID="${MERKZETTEL_CLIENT_ID}"
    MERKZETTEL_TENANT="${MERKZETTEL_TENANT}"
    MERKZETTEL_VERSION="${PROJECT_VERSION}"
    MERKZETTEL_BUILD_LOCALE_DIR="${CMAKE_BINARY_DIR}/po"
)

target_include_directories(merkzettel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(merkzettel PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::NetworkAuth
    Qt6::Sql
    Qt6::Widgets
    KF6::CoreAddons
    KF6::I18n
    KF6::Notifications
    KF6::StatusNotifierItem
    KF6::ConfigCore
    KF6::ConfigGui
    qt6keychain
)

ecm_add_qml_module(merkzettel
    URI org.kde.merkzettel
    VERSION 1.0
    QML_FILES
        Main.qml
        LoginPage.qml
        TasksPage.qml
        TaskDelegate.qml
        TaskDetailSheet.qml
)

install(TARGETS merkzettel ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
