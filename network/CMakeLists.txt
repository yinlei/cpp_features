##################################################################################
aux_source_directory(${PROJECT_SOURCE_DIR}/network NET_SRC_LIST)
include_directories("${PROJECT_SOURCE_DIR}/network")
include_directories("${PROJECT_SOURCE_DIR}")
set(TARGET "network")
add_library("${TARGET}" STATIC ${NET_SRC_LIST})

set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS ${TARGET} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(DIRECTORY ${PROJECT_SOURCE_DIR}/network/ DESTINATION "include/network" FILES_MATCHING PATTERN "*.h")

