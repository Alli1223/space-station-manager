include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLAD (bundled - pre-generated OpenGL 3.3 Core loader)
add_library(glad STATIC
    ${CMAKE_SOURCE_DIR}/third_party/glad/src/glad.c
)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/third_party/glad/include)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

add_executable(station_client
    src/main.cpp
    src/renderer.cpp
    src/bitmap_font.cpp
    src/network_client.cpp
    src/input_handler.cpp
    src/game_client.cpp
    src/ui/ui_element.cpp
    src/ui/ui_manager.cpp
    src/ui/ui_button.cpp
    src/ui/ui_hotbar.cpp
)

target_include_directories(station_client PRIVATE include)
target_link_libraries(station_client PRIVATE shared glfw glad glm::glm)

# Set working directory to project root
set_target_properties(station_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(station_client PRIVATE ws2_32 opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(station_client PRIVATE OpenGL::GL)
endif()
