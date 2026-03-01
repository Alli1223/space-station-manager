#pragma once

#include "shared/game_object.h"
#include "shared/game_objects.h"
#include "shared/map.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

namespace ssm {

class Renderer {
public:
    bool init(int windowWidth, int windowHeight);
    void shutdown();

    void beginFrame();
    void endFrame();

    void setCamera(float x, float y);
    float getCameraX() const { return cameraX; }
    float getCameraY() const { return cameraY; }

    void renderMap(const StationMap& map);
    void renderObject(const GameObject* obj);
    void renderObjects(const std::vector<GameObject*>& objects);
    void renderHUD(const Player* localPlayer, const std::vector<GameObject*>& objects, int32_t stationMoney = 0);

    // Screen-space rendering for UI
    void beginScreenSpace();
    void endScreenSpace();

    // Public drawing API (used by UI system)
    void drawUIRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    void drawUIOutline(float x, float y, float w, float h, float thickness, float r, float g, float b, float a = 1.0f);
    void drawWorldRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);

    // Cargo tooltip (call in screen space)
    void renderCargoTooltip(float screenX, float screenY, CargoType type);

    // Tether ropes (call in world space, after renderObjects)
    void renderTetherRopes(const std::vector<GameObject*>& objects);

    GLFWwindow* getWindow() const { return window; }
    int getWindowWidth() const { return windowWidth; }
    int getWindowHeight() const { return windowHeight; }

    // Get color for a cell type (used by hotbar)
    static glm::vec4 getCellTypeColor(CellType type);

    // Get color for a cargo type (used by ghost cursor and objectives panel)
    static glm::vec4 getCargoTypeColor(CargoType type);

private:
    GLFWwindow* window = nullptr;
    int windowWidth = 800;
    int windowHeight = 600;

    // Camera
    float cameraX = 0.0f;
    float cameraY = 0.0f;

    // Shader program
    GLuint shaderProgram = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint uProjection = -1;
    GLint uColor = -1;

    void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    void drawCircle(float cx, float cy, float radius, float r, float g, float b);
    glm::vec4 getColorForObject(const GameObject* obj) const;

    // Objectives panel rendering
    void renderObjectivesPanel(const Ship* ship);

    bool createShaders();
};

} // namespace ssm
