cmake_minimum_required(VERSION 2.8)
project(http)

set(CMAKE_CXX_FLAGS "-g -std=c++11 -Wall -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -std=c++11 -Wall -Werror")

##################################################################################
aux_source_directory(${PROJECT_SOURCE_DIR}/src HTTP_SRC_LIST)
set(TARGET "http")
add_library("${TARGET}" STATIC ${HTTP_SRC_LIST})

set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS ${TARGET} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION "include/http" FILES_MATCHING PATTERN "*.h")

