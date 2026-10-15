# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: BSD-3-Clause

ecm_add_tests(
    tst_dateparser.cpp
    tst_graphclient.cpp
    tst_database.cpp
    tst_tasksmodel.cpp
    LINK_LIBRARIES merkzettel_core Qt6::Test
)

# QML smoke test: loads the QML from the source tree with the real App.
ecm_add_test(tst_appdemo.cpp
    TEST_NAME tst_appdemo
    LINK_LIBRARIES merkzettel_app Qt6::Test Qt6::Quick Qt6::QuickControls2 KF6::I18nQml
)
target_compile_definitions(tst_appdemo PRIVATE MERKZETTEL_QML_DIR="${CMAKE_SOURCE_DIR}/src")
# Needs the full QML runtime (Kirigami, kirigami-addons, qqc2-desktop-style);
# label it so environments without it can skip: ctest -LE qml
set_tests_properties(tst_appdemo PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS qml)
