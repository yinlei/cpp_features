cmake_minimum_required(VERSION 2.8)

###################################################################################
include_directories("${PROJECT_SOURCE_DIR}/..")
include_directories("${PROJECT_SOURCE_DIR}/../linux")

add_library(coroutine_main coroutine_main.cpp)

set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS coroutine_main LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
