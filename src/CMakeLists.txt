add_library(core Map.cpp)
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(server server/server.cpp)
target_link_libraries(server PRIVATE core boost_system)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(client client/client.cpp)
target_link_libraries(client PRIVATE core glfw GLEW::GLEW OpenGL::GL boost_system)
