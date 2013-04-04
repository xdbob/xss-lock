cmake_minimum_required(VERSION 2.8)
project(xlocklaunch C)
set(VERSION 0.1.0)

set(logind_dbus_interfaces manager session)
set(logind_dbus_sources)
foreach(interface ${logind_dbus_interfaces})
    set(input logind_${interface}.xml)
    set(output logind_${interface}.c logind_${interface}.h)
    list(APPEND logind_dbus_sources ${output})
    add_custom_command(OUTPUT ${output}
        COMMAND gdbus-codegen --generate-c-code logind_${interface}
                --c-namespace logind --interface-prefix org.freedesktop.login1
                ${CMAKE_CURRENT_SOURCE_DIR}/${input}
        DEPENDS ${input} VERBATIM)
endforeach()

include(FindPkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.30 gio-unix-2.0)
pkg_check_modules(XCB REQUIRED xcb xcb-event xcb-screensaver)
include_directories(${GLIB2_INCLUDE_DIRS} ${XCB_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS} ${XCB_LIBRARY_DIRS})

if(XCB_xcb_VERSION VERSION_LESS 1.8)
    set(XCB_POLL_FOR_QUEUED_EVENT 0)
else()
    set(XCB_POLL_FOR_QUEUED_EVENT 1)
endif()

configure_file("${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

add_executable(xlocklaunch
    xlocklaunch.c
    x_event_source.c
    x_event_source.h
    config.h
    ${logind_dbus_sources}
)

target_link_libraries(xlocklaunch ${GLIB2_LIBRARIES} ${XCB_LIBRARIES})

install(TARGETS xlocklaunch DESTINATION bin)

find_program(rst2man NAMES rst2man rst2man.py rst2man2 rst2man2.py)
if(rst2man)
    set(man_input xlocklaunch.1.rst)
    set(man_output xlocklaunch.1)
    add_custom_command(OUTPUT ${man_output}
        COMMAND ${rst2man} ${CMAKE_CURRENT_SOURCE_DIR}/${man_input} ${man_output}
        DEPENDS ${man_input})
    add_custom_target(man ALL DEPENDS ${man_output})
    install(FILES ${man_output} DESTINATION share/man/man1)
else()
    message(WARNING "rst2man (docutils) not found, disabling man page generation")
endif()
