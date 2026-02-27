add_library(shared STATIC
    src/game_object.cpp
    src/game_objects.cpp
    src/protocol.cpp
    src/map.cpp
)

target_include_directories(shared PUBLIC include)

if(WIN32)
    target_link_libraries(shared PUBLIC ws2_32)
endif()
