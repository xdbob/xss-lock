cmake_minimum_required(VERSION 2.6)
project(xlocklaunch C)
set(VERSION 0.1.0)

configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

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
endforeach(interface)

include(FindPkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.30)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(XCB_UTIL REQUIRED xcb-util)
pkg_check_modules(XCB_SCREENSAVER REQUIRED xcb-screensaver)
include_directories(${GLIB2_INCLUDE_DIRS} ${XCB_INCLUDE_DIRS}
    ${XCB_UTIL_INCLUDE_DIRS} ${XCB_SCREENSAVER_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS} ${XCB_LIBRARY_DIRS}
    ${XCB_UTIL_LIBRARY_DIRS} ${XCB_SCREENSAVER_LIBRARY_DIRS})

add_executable(xlocklaunch
    xlocklaunch.c
    x_event_source.c
    x_event_source.h
    config.h
    ${logind_dbus_sources}
)

target_link_libraries(xlocklaunch ${GLIB2_LIBRARIES} ${XCB_LIBRARIES}
    ${XCB_UTIL_LIBRARIES} ${XCB_SCREENSAVER_LIBRARIES})

install(TARGETS xlocklaunch DESTINATION bin)
