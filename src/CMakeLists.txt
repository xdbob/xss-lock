include(FindPkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.34 gio-unix-2.0)
pkg_check_modules(XCB REQUIRED xcb xcb-event xcb-screensaver)
include_directories(${GLIB2_INCLUDE_DIRS} ${XCB_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS} ${XCB_LIBRARY_DIRS})

if(XCB_xcb_VERSION VERSION_LESS 1.8)
    set(XCB_POLL_FOR_QUEUED_EVENT 0)
else()
    set(XCB_POLL_FOR_QUEUED_EVENT 1)
endif()

configure_file(config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(xss-lock
    xss-lock.c
    xcb_utils.c
    xcb_utils.h
    config.h
)

target_link_libraries(xss-lock ${GLIB2_LIBRARIES} ${XCB_LIBRARIES})

install(TARGETS xss-lock DESTINATION bin)
