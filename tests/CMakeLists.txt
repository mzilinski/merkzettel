# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: BSD-3-Clause

ecm_add_tests(
    tst_dateparser.cpp
    tst_graphclient.cpp
    tst_database.cpp
    tst_tasksmodel.cpp
    LINK_LIBRARIES merkzettel_core Qt6::Test
)
