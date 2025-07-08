#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <boost/asio.hpp>
#include <iostream>
#include "Map.h"
#include <sstream>

using boost::asio::ip::tcp;

void drawQuad(int x, int y, float size) {
    glBegin(GL_QUADS);
    glVertex2f(x * size, y * size);
    glVertex2f((x+1) * size, y * size);
    glVertex2f((x+1) * size, (y+1) * size);
    glVertex2f(x * size, (y+1) * size);
    glEnd();
}

bool sendCommand(char cmd, tcp::socket& socket, int& px, int& py, GameMap& map) {
    boost::system::error_code ec;
    boost::asio::write(socket, boost::asio::buffer(&cmd,1), ec);
    if (ec) return false;

    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, '\n', ec);
    if (ec) return false;
    std::istream is(&buf);
    std::string token;
    while (is >> token) {
        if (token == "POS") {
            is >> px >> py;
        } else if (token == "DOOR") {
            int x,y; std::string state; is >> x >> y >> state;
            map.set(x,y, state=="open" ? Cell::DoorOpen : Cell::DoorClosed);
        }
    }
    return true;
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
    int playerX = 10, playerY = 10;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 20, 20, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        float size = 1.0f;
        // handle input
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) sendCommand('W', socket, playerX, playerY, localMap);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) sendCommand('S', socket, playerX, playerY, localMap);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) sendCommand('A', socket, playerX, playerY, localMap);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) sendCommand('D', socket, playerX, playerY, localMap);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) sendCommand('E', socket, playerX, playerY, localMap);

        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) {
                const Cell& c = localMap.get(x,y);
                if (c.type == Cell::Walkable) {
                    glColor3f(0.0f, 1.0f, 0.0f);
                    drawQuad(x, y, size);
                } else if (c.type == Cell::DoorClosed) {
                    glColor3f(1.0f, 0.0f, 0.0f);
                    drawQuad(x, y, size);
                } else if (c.type == Cell::DoorOpen) {
                    glColor3f(0.0f, 0.0f, 1.0f);
                    drawQuad(x, y, size);
                }
            }
        }

        glColor3f(1.0f, 1.0f, 1.0f);
        drawQuad(playerX, playerY, size);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
