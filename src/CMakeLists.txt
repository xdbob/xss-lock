cmake_minimum_required(VERSION 2.8)
project(xss-lock C)
set(VERSION 0.1.0)

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

add_executable(xss-lock
    xss-lock.c
    xcb_util.c
    xcb_util.h
    config.h
)

target_link_libraries(xss-lock ${GLIB2_LIBRARIES} ${XCB_LIBRARIES})

install(TARGETS xss-lock DESTINATION bin)

find_program(rst2man NAMES rst2man rst2man.py rst2man2 rst2man2.py)
if(rst2man)
    set(man_input xss-lock.1.rst)
    set(man_output xss-lock.1)
    add_custom_command(OUTPUT ${man_output}
        COMMAND ${rst2man} ${CMAKE_CURRENT_SOURCE_DIR}/${man_input} ${man_output}
        DEPENDS ${man_input})
    add_custom_target(man ALL DEPENDS ${man_output})
    install(FILES ${man_output} DESTINATION share/man/man1)
else()
    message(WARNING "rst2man (docutils) not found, disabling man page generation")
endif()
