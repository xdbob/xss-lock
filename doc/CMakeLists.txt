configure_file(xss-lock.1.rst.in xss-lock.1.rst)

find_program(rst2man NAMES rst2man rst2man.py rst2man2 rst2man2.py)
if(rst2man)
    set(man_input "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.1.rst")
    set(man_output "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.1")
    add_custom_command(OUTPUT ${man_output}
                       COMMAND ${rst2man} ${man_input} ${man_output}
                       DEPENDS ${man_input})
    add_custom_target(man ALL DEPENDS ${man_output})
    install(FILES ${man_output} DESTINATION share/man/man1)
else()
    message(WARNING "rst2man (docutils) not found, disabling man page generation")
endif()

install(FILES dim-screen.sh xdg-screensaver.patch
              transfer-sleep-lock-i3lock.sh transfer-sleep-lock-generic-delay.sh
              xss-lock.service
        DESTINATION share/doc/${PROJECT_NAME})
