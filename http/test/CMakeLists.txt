cmake_minimum_required(VERSION 2.8)
project(http_test)

set(CMAKE_CXX_FLAGS "-g -std=c++11 -Wall -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -std=c++11 -Wall -Werror")

##################################################################################
include_directories("${PROJECT_SOURCE_DIR}/../../coroutine/unit_test/gtest_unit/gtest/include")
link_directories("${PROJECT_SOURCE_DIR}/../../coroutine/unit_test/gtest_unit/gtest/lib")

add_executable(url_test.t url_test.cpp)
target_link_libraries(url_test.t -lhttp -lboost_regex -lboost_system -lgtest -lgtest_main -lpthread)
