set(CMAKE_CXX_STANDARD 23)

set(DHTX_SOURCES "main.cpp")

link_directories(${NPCAP_LIB_DIR} ${PCAPPP_LIB_DIR})
add_executable(${DHTX_PROJECT_NAME} ${DHTX_SOURCES})

set_target_properties(${DHTX_PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "dhtx"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${DHTX_PROJECT_NAME} PUBLIC .)

find_package(LibtorrentRasterbar CONFIG REQUIRED)
target_link_libraries(${DHTX_PROJECT_NAME} PRIVATE LibtorrentRasterbar::torrent-rasterbar)

install(TARGETS ${DHTX_PROJECT_NAME})
