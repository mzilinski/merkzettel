find_package(Gettext)
find_package(KF6I18n)

if(GETTEXT_FOUND AND KF6I18n_FOUND)
    file(GLOB_RECURSE PO_FILES "*.po")
    foreach(po ${PO_FILES})
        get_filename_component(lang "${po}" DIRECTORY)
        get_filename_component(lang "${lang}" NAME)
        set(mo "${CMAKE_CURRENT_BINARY_DIR}/${lang}/LC_MESSAGES/merkzettel.mo")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${lang}/LC_MESSAGES")
        add_custom_command(
            OUTPUT "${mo}"
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${mo}" "${po}"
            DEPENDS "${po}"
            COMMENT "Compiling ${lang}/merkzettel.mo"
        )
        list(APPEND MO_FILES "${mo}")
        install(FILES "${mo}"
                DESTINATION "${KDE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES")
    endforeach()
    add_custom_target(translations ALL DEPENDS ${MO_FILES})
endif()
