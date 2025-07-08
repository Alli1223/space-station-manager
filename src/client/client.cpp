#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <boost/asio.hpp>
#include <iostream>
#include "Map.h"

using boost::asio::ip::tcp;

void drawCell(int x, int y, float size) {
    glBegin(GL_QUADS);
    glVertex2f(x * size, y * size);
    glVertex2f((x+1) * size, y * size);
    glVertex2f((x+1) * size, (y+1) * size);
    glVertex2f(x * size, (y+1) * size);
    glEnd();
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return -1;
    }
    GLFWwindow* window = glfwCreateWindow(800, 600, "Client", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW init error" << std::endl;
        return -1;
    }

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve("127.0.0.1", "12345");
    tcp::socket socket(io_context);
    boost::asio::connect(socket, endpoints);

    GameMap localMap; // start with same map

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(0.0f, 1.0f, 0.0f);
        float size = 0.05f;
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                if (localMap.get(x,y).type == Cell::Walkable)
                    drawCell(x, y, size);
            }
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
