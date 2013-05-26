cmake_minimum_required(VERSION 2.8)
project(xss-lock C)
set(PROJECT_VERSION 0.2.1)

install(FILES NEWS DESTINATION share/doc/${PROJECT_NAME})

add_subdirectory(src)
add_subdirectory(doc)
