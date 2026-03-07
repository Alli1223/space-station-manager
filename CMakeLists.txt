cmake_minimum_required(VERSION 3.20)
project(SpaceStationManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared library (types, protocol, game objects)
add_subdirectory(shared)

# Server executable (no graphics deps)
add_subdirectory(server)

# Client executable (OpenGL + GLFW)
add_subdirectory(client)
