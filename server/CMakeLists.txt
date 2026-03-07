add_executable(station_server
    src/main.cpp
    src/game_world.cpp
    src/network_server.cpp
    src/collision.cpp
    src/ship_manager.cpp
)

target_include_directories(station_server PRIVATE include)
target_link_libraries(station_server PRIVATE shared)

# Set working directory to project root so assets/maps/ is found
set_target_properties(station_server PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(station_server PRIVATE ws2_32)
endif()
