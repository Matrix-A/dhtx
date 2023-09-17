cmake_minimum_required(VERSION 3.20)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(PROJECT_BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
include("${PROJECT_BASE_FOLDER}/cmake/build_helpers.cmake")

loadVersion(DHTX_VERSION)
setVariableInParent(DHTX_VERSION ${DHTX_VERSION})

set(DHTX_PROJECT_NAME "dhtx")

project(${DHTX_PROJECT_NAME}
        LANGUAGES C CXX VERSION ${DHTX_VERSION}
        DESCRIPTION "The dhtx")

add_compile_definitions(DHTX_PROJECT_NAME="${DHTX_PROJECT_NAME}")
add_compile_definitions(DHTX_VERSION="${DHTX_VERSION}")

add_subdirectory(src)
