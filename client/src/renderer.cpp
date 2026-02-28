#include "client/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>

namespace ssm {

static const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

static const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)";

bool Renderer::init(int w, int h) {
    windowWidth = w;
    windowHeight = h;

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(windowWidth, windowHeight, "Space Station Manager", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD" << std::endl;
        return false;
    }

    if (!createShaders()) return false;

    // Setup VAO/VBO for quads
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

bool Renderer::createShaders() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vs);
    int success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        std::cerr << "Vertex shader error: " << log << std::endl;
        return false;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        std::cerr << "Fragment shader error: " << log << std::endl;
        return false;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    uProjection = glGetUniformLocation(shaderProgram, "uProjection");
    uColor = glGetUniformLocation(shaderProgram, "uColor");

    return true;
}

void Renderer::shutdown() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

void Renderer::beginFrame() {
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // dark space background
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

    // Orthographic projection centered on camera
    glm::mat4 proj = glm::ortho(
        cameraX - windowWidth / 2.0f,
        cameraX + windowWidth / 2.0f,
        cameraY + windowHeight / 2.0f,  // flip Y so Y-down
        cameraY - windowHeight / 2.0f,
        -1.0f, 1.0f
    );
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proj));
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void Renderer::setCamera(float x, float y) {
    cameraX = x;
    cameraY = y;
}

void Renderer::drawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float vertices[] = {
        x, y,
        x + w, y,
        x + w, y + h,
        x, y,
        x + w, y + h,
        x, y + h,
    };

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glUniform4f(uColor, r, g, b, a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawCircle(float cx, float cy, float radius, float r, float g, float b) {
    const int segments = 20;
    float vertices[(segments + 2) * 2];

    // Center vertex
    vertices[0] = cx;
    vertices[1] = cy;

    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * 3.14159f * i / segments;
        vertices[(i + 1) * 2] = cx + radius * std::cos(angle);
        vertices[(i + 1) * 2 + 1] = cy + radius * std::sin(angle);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glUniform4f(uColor, r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
}

void Renderer::renderMap(const StationMap& map) {
    // Batch tiles by color to minimize draw calls
    // Each visible tile = 6 vertices (2 triangles), 2 floats per vertex = 12 floats per tile
    struct ColorBatch {
        float r, g, b;
        std::vector<float> vertices;
        void addQuad(float x, float y, float w, float h) {
            float v[] = { x,y, x+w,y, x+w,y+h, x,y, x+w,y+h, x,y+h };
            vertices.insert(vertices.end(), v, v + 12);
        }
    };

    ColorBatch wallBatch    {0.3f, 0.3f, 0.35f, {}};
    ColorBatch floorBatch   {0.6f, 0.6f, 0.65f, {}};
    ColorBatch airlockBatch {0.5f, 0.55f, 0.6f, {}};
    ColorBatch storageBatch {0.55f, 0.6f, 0.55f, {}};
    ColorBatch collarBatch  {0.8f, 0.5f, 0.2f, {}};

    float halfW = windowWidth / 2.0f;
    float halfH = windowHeight / 2.0f;

    // Iterate using logical/world grid coordinates (origin-aware)
    for (int ly = map.originY; ly < map.originY + map.height; ly++) {
        for (int lx = map.originX; lx < map.originX + map.width; lx++) {
            float wx = lx * CELL_SIZE;
            float wy = ly * CELL_SIZE;

            // Frustum culling
            if (wx + CELL_SIZE < cameraX - halfW - CELL_SIZE ||
                wx > cameraX + halfW + CELL_SIZE ||
                wy + CELL_SIZE < cameraY - halfH - CELL_SIZE ||
                wy > cameraY + halfH + CELL_SIZE) {
                continue;
            }

            CellType cell = map.getCell(lx, ly);
            switch (cell) {
                case CellType::WALL:
                    wallBatch.addQuad(wx, wy, CELL_SIZE, CELL_SIZE); break;
                case CellType::FLOOR:
                case CellType::SPAWN_POINT:
                case CellType::DOOR:
                case CellType::TERMINAL:
                    floorBatch.addQuad(wx, wy, CELL_SIZE, CELL_SIZE); break;
                case CellType::AIRLOCK:
                    airlockBatch.addQuad(wx, wy, CELL_SIZE, CELL_SIZE); break;
                case CellType::STORAGE:
                    storageBatch.addQuad(wx, wy, CELL_SIZE, CELL_SIZE); break;
                case CellType::DOCKING_COLLAR:
                    collarBatch.addQuad(wx, wy, CELL_SIZE, CELL_SIZE); break;
                default: break;
            }
        }
    }

    // Draw each batch in one call
    auto drawBatch = [&](const ColorBatch& batch) {
        if (batch.vertices.empty()) return;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     batch.vertices.size() * sizeof(float),
                     batch.vertices.data(), GL_DYNAMIC_DRAW);
        glUniform4f(uColor, batch.r, batch.g, batch.b, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch.vertices.size() / 2));
    };

    drawBatch(floorBatch);
    drawBatch(wallBatch);
    drawBatch(airlockBatch);
    drawBatch(storageBatch);
    drawBatch(collarBatch);
}

glm::vec4 Renderer::getColorForObject(const GameObject* obj) const {
    switch (obj->type) {
        case GameObjectType::DOOR: {
            auto* door = static_cast<const Door*>(obj);
            if (door->state == DoorState::OPEN)
                return {0.2f, 0.7f, 0.2f, 1.0f}; // green when open
            else
                return {0.6f, 0.3f, 0.1f, 1.0f}; // brown when closed
        }
        case GameObjectType::TERMINAL:
            return {0.2f, 0.4f, 0.9f, 1.0f}; // blue
        case GameObjectType::DOCKING_COLLAR:
            return {0.9f, 0.6f, 0.1f, 1.0f}; // orange
        case GameObjectType::SHIP: {
            auto* ship = static_cast<const Ship*>(obj);
            switch (ship->shipClass) {
                case ShipClass::SMALL:  return {0.85f, 0.9f, 0.95f, 1.0f};  // light blue-white
                case ShipClass::MEDIUM: return {0.9f, 0.9f, 0.95f, 1.0f};   // white
                case ShipClass::LARGE:  return {0.95f, 0.88f, 0.85f, 1.0f}; // warm cream
                default:                return {0.9f, 0.9f, 0.95f, 1.0f};
            }
        }
        case GameObjectType::CARGO: {
            auto* cargo = static_cast<const Cargo*>(obj);
            switch (cargo->cargoType) {
                case CargoType::METAL: return {0.75f, 0.75f, 0.8f, 1.0f};
                case CargoType::WOOD:  return {0.6f, 0.4f, 0.2f, 1.0f};
                case CargoType::FUEL:  return {0.9f, 0.9f, 0.2f, 1.0f};
                case CargoType::FOOD:  return {0.3f, 0.8f, 0.3f, 1.0f};
                default:               return {0.5f, 0.5f, 0.5f, 1.0f};
            }
        }
        default:
            return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

void Renderer::renderObject(const GameObject* obj) {
    if (!obj->active) return;

    // Skip floor and wall - rendered by map
    if (obj->type == GameObjectType::WALL || obj->type == GameObjectType::FLOOR) return;

    if (obj->type == GameObjectType::PLAYER) {
        auto* player = static_cast<const Player*>(obj);
        float cx = player->x + player->width / 2.0f;
        float cy = player->y + player->height / 2.0f;
        // Use player's assigned color
        uint8_t ci = player->colorIndex % 8;
        drawCircle(cx, cy, player->width / 2.0f,
                   PLAYER_COLORS[ci][0], PLAYER_COLORS[ci][1], PLAYER_COLORS[ci][2]);
        // Draw a small indicator if carrying cargo
        if (player->isCarrying()) {
            drawCircle(cx, cy - player->height / 2.0f - 4.0f, 4.0f, 1.0f, 1.0f, 0.0f);
        }
        return;
    }

    if (obj->type == GameObjectType::SHIP) {
        auto* ship = static_cast<const Ship*>(obj);
        glm::vec4 color = getColorForObject(obj);
        drawRect(obj->x, obj->y, obj->width, obj->height, color.r, color.g, color.b, color.a);
        // Draw cargo hold contents if docked
        if (ship->state == ShipState::UNLOADING || ship->state == ShipState::WAITING_RESUPPLY) {
            renderShipCargo(ship);
        }
        return;
    }

    glm::vec4 color = getColorForObject(obj);
    drawRect(obj->x, obj->y, obj->width, obj->height, color.r, color.g, color.b, color.a);
}

void Renderer::renderObjects(const std::vector<GameObject*>& objects) {
    // Render in order: doors, terminals, cargo, ships, players (so players draw on top)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::DOOR) renderObject(obj);
    }
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::TERMINAL || obj->type == GameObjectType::DOCKING_COLLAR)
            renderObject(obj);
    }
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::SHIP) renderObject(obj);
    }
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::CARGO) renderObject(obj);
    }
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::PLAYER) renderObject(obj);
    }
}

void Renderer::renderHUD(const Player* localPlayer, const std::vector<GameObject*>& objects, int32_t stationMoney) {
    // Switch to screen-space projection for HUD
    beginScreenSpace();

    if (!localPlayer) {
        endScreenSpace();
        return;
    }

    // Show carrying status — small swatch in top-left
    if (localPlayer->isCarrying()) {
        drawRect(2.0f, 2.0f, 26.0f, 26.0f, 0.1f, 0.1f, 0.15f, 0.8f);
        for (auto* obj : objects) {
            if (obj->id == localPlayer->carryingCargoId && obj->type == GameObjectType::CARGO) {
                auto* cargo = static_cast<const Cargo*>(obj);
                glm::vec4 cc = getColorForObject(cargo);
                drawRect(5.0f, 5.0f, 20.0f, 20.0f, cc.r, cc.g, cc.b);
                break;
            }
        }
    }

    // === Money display (top-left, below carrying indicator) ===
    {
        float moneyY = 34.0f;
        // Coin icon (yellow circle approximated as square)
        drawRect(4.0f, moneyY, 12.0f, 12.0f, 1.0f, 0.85f, 0.2f);

        // Money bar: 1px per 10 money, capped at 200px visual width
        float barX = 20.0f;
        float barW = std::min(static_cast<float>(std::abs(stationMoney)) / 10.0f, 200.0f);
        float barH = 8.0f;
        float barY = moneyY + 2.0f;

        // Background
        drawRect(barX, barY, 200.0f, barH, 0.15f, 0.15f, 0.2f, 0.6f);

        // Bar color: green if positive, red if negative
        if (stationMoney >= 0) {
            drawRect(barX, barY, barW, barH, 0.2f, 0.8f, 0.3f);
        } else {
            drawRect(barX, barY, barW, barH, 0.9f, 0.2f, 0.2f);
        }

        // Coin pips: one small yellow square per 100 money, up to 10
        int pips = std::min(std::abs(stationMoney) / 100, 10);
        for (int i = 0; i < pips; i++) {
            float pipX = barX + i * 14.0f + 2.0f;
            float pipY = moneyY + barH + 6.0f;
            drawRect(pipX, pipY, 10.0f, 6.0f, 1.0f, 0.85f, 0.2f);
        }
    }

    // Find the nearest docked ship within 300px of the local player
    float playerCX = localPlayer->x + localPlayer->width / 2.0f;
    float playerCY = localPlayer->y + localPlayer->height / 2.0f;
    const Ship* nearestShip = nullptr;
    float nearestDist = 300.0f;

    for (auto* obj : objects) {
        if (obj->type == GameObjectType::SHIP && obj->active) {
            auto* ship = static_cast<const Ship*>(obj);
            if (ship->state == ShipState::UNLOADING || ship->state == ShipState::WAITING_RESUPPLY ||
                ship->state == ShipState::DOCKING) {
                float shipCX = ship->x + ship->width / 2.0f;
                float shipCY = ship->y + ship->height / 2.0f;
                float dx = playerCX - shipCX;
                float dy = playerCY - shipCY;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearestShip = ship;
                }
            }
        }
    }

    if (nearestShip) {
        renderObjectivesPanel(nearestShip);
    }

    // Restore world projection
    endScreenSpace();
}

void Renderer::beginScreenSpace() {
    glm::mat4 proj = glm::ortho(0.0f, (float)windowWidth, (float)windowHeight, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proj));
}

void Renderer::endScreenSpace() {
    glm::mat4 proj = glm::ortho(
        cameraX - windowWidth / 2.0f,
        cameraX + windowWidth / 2.0f,
        cameraY + windowHeight / 2.0f,
        cameraY - windowHeight / 2.0f,
        -1.0f, 1.0f
    );
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proj));
}

void Renderer::drawUIRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    drawRect(x, y, w, h, r, g, b, a);
}

void Renderer::drawUIOutline(float x, float y, float w, float h, float thickness, float r, float g, float b, float a) {
    // Top
    drawRect(x, y, w, thickness, r, g, b, a);
    // Bottom
    drawRect(x, y + h - thickness, w, thickness, r, g, b, a);
    // Left
    drawRect(x, y, thickness, h, r, g, b, a);
    // Right
    drawRect(x + w - thickness, y, thickness, h, r, g, b, a);
}

void Renderer::drawWorldRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    drawRect(x, y, w, h, r, g, b, a);
}

glm::vec4 Renderer::getCargoTypeColor(CargoType type) {
    switch (type) {
        case CargoType::METAL: return {0.75f, 0.75f, 0.8f, 1.0f};
        case CargoType::WOOD:  return {0.6f, 0.4f, 0.2f, 1.0f};
        case CargoType::FUEL:  return {0.9f, 0.9f, 0.2f, 1.0f};
        case CargoType::FOOD:  return {0.3f, 0.8f, 0.3f, 1.0f};
        default:               return {0.5f, 0.5f, 0.5f, 1.0f};
    }
}

void Renderer::renderShipCargo(const Ship* ship) {
    // Draw a dark cargo hold area inset on the ship body
    float holdX = ship->x + 4.0f;
    float holdY = ship->y + 4.0f;
    float holdW = ship->width - 8.0f;
    float holdH = ship->height * 0.5f; // upper half of the ship is the cargo hold

    drawRect(holdX, holdY, holdW, holdH, 0.15f, 0.15f, 0.2f, 0.8f);

    // Draw cargo items as small colored squares in a grid layout
    float itemSize = 10.0f;
    float padding = 2.0f;
    float startX = holdX + padding;
    float startY = holdY + padding;
    float curX = startX;
    float curY = startY;
    int cols = static_cast<int>((holdW - padding * 2) / (itemSize + padding));
    if (cols < 1) cols = 1;
    int col = 0;

    // Draw metal items
    glm::vec4 metalColor = getCargoTypeColor(CargoType::METAL);
    for (int i = 0; i < ship->metalToUnload; i++) {
        drawRect(curX, curY, itemSize, itemSize, metalColor.r, metalColor.g, metalColor.b);
        col++;
        if (col >= cols) {
            col = 0;
            curX = startX;
            curY += itemSize + padding;
        } else {
            curX += itemSize + padding;
        }
    }

    // Draw wood items
    glm::vec4 woodColor = getCargoTypeColor(CargoType::WOOD);
    for (int i = 0; i < ship->woodToUnload; i++) {
        drawRect(curX, curY, itemSize, itemSize, woodColor.r, woodColor.g, woodColor.b);
        col++;
        if (col >= cols) {
            col = 0;
            curX = startX;
            curY += itemSize + padding;
        } else {
            curX += itemSize + padding;
        }
    }
}

void Renderer::renderObjectivesPanel(const Ship* ship) {
    // Panel on the right side of screen
    float panelW = 120.0f;
    float panelH = 230.0f;
    float panelX = windowWidth - panelW - 10.0f;
    float panelY = 40.0f;

    // Panel background
    drawRect(panelX, panelY, panelW, panelH, 0.1f, 0.1f, 0.15f, 0.85f);

    float y = panelY + 8.0f;
    float iconSize = 16.0f;
    float dotSize = 6.0f;
    float sectionPad = 12.0f;

    // === EXPORT SECTION (items to take off ship) ===
    // Section header bar
    drawRect(panelX, y, panelW, 3.0f, 0.8f, 0.4f, 0.2f); // orange bar = export
    y += 8.0f;

    // Metal row
    {
        glm::vec4 mc = getCargoTypeColor(CargoType::METAL);
        bool allTaken = (ship->metalToUnload == 0);
        drawRect(panelX + 8.0f, y, iconSize, iconSize, mc.r, mc.g, mc.b);
        if (allTaken) {
            // Green overlay on icon when all taken
            drawRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        }
        // Dot indicators
        int totalMetal = ship->totalMetal;
        float dotX = panelX + 8.0f + iconSize + 6.0f;
        for (int i = 0; i < totalMetal; i++) {
            bool withdrawn = (i >= ship->metalToUnload);
            if (withdrawn) {
                drawRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.2f, 0.9f, 0.2f); // green = done
            } else {
                drawRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.5f, 0.5f, 0.55f); // gray = remaining
            }
            dotX += dotSize + 2.0f;
        }
        y += iconSize + 6.0f;
    }

    // Wood row
    {
        glm::vec4 wc = getCargoTypeColor(CargoType::WOOD);
        bool allTaken = (ship->woodToUnload == 0);
        drawRect(panelX + 8.0f, y, iconSize, iconSize, wc.r, wc.g, wc.b);
        if (allTaken) {
            drawRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        }
        int totalWood = ship->totalWood;
        float dotX = panelX + 8.0f + iconSize + 6.0f;
        for (int i = 0; i < totalWood; i++) {
            bool withdrawn = (i >= ship->woodToUnload);
            if (withdrawn) {
                drawRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.2f, 0.9f, 0.2f);
            } else {
                drawRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.5f, 0.5f, 0.55f);
            }
            dotX += dotSize + 2.0f;
        }
        y += iconSize + sectionPad;
    }

    // === IMPORT SECTION (items to load onto ship) ===
    // Section header bar
    drawRect(panelX, y, panelW, 3.0f, 0.2f, 0.6f, 0.9f); // blue bar = import
    y += 8.0f;

    // Fuel row
    {
        glm::vec4 fc = getCargoTypeColor(CargoType::FUEL);
        float pct = ship->fuel / ship->maxFuel;
        bool full = (pct >= 1.0f);
        drawRect(panelX + 8.0f, y, iconSize, iconSize, fc.r, fc.g, fc.b);
        if (full) {
            drawRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        }
        // Progress bar
        float barX = panelX + 8.0f + iconSize + 6.0f;
        float barW = panelW - iconSize - 22.0f;
        float barH = 8.0f;
        float barY = y + (iconSize - barH) / 2.0f;
        drawRect(barX, barY, barW, barH, 0.2f, 0.2f, 0.2f);
        drawRect(barX, barY, barW * pct, barH, fc.r, fc.g, fc.b);
        if (full) {
            drawRect(barX, barY, barW, barH, 0.2f, 0.9f, 0.2f, 0.3f);
        }
        y += iconSize + 6.0f;
    }

    // Food row
    {
        glm::vec4 fc = getCargoTypeColor(CargoType::FOOD);
        float pct = ship->food / ship->maxFood;
        bool full = (pct >= 1.0f);
        drawRect(panelX + 8.0f, y, iconSize, iconSize, fc.r, fc.g, fc.b);
        if (full) {
            drawRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        }
        float barX = panelX + 8.0f + iconSize + 6.0f;
        float barW = panelW - iconSize - 22.0f;
        float barH = 8.0f;
        float barY = y + (iconSize - barH) / 2.0f;
        drawRect(barX, barY, barW, barH, 0.2f, 0.2f, 0.2f);
        drawRect(barX, barY, barW * pct, barH, fc.r, fc.g, fc.b);
        if (full) {
            drawRect(barX, barY, barW, barH, 0.2f, 0.9f, 0.2f, 0.3f);
        }
        y += iconSize + sectionPad;
    }

    // === PATIENCE SECTION ===
    if (ship->state == ShipState::WAITING_RESUPPLY && ship->maxPatience > 0.0f) {
        // Separator bar
        drawRect(panelX, y, panelW, 3.0f, 0.6f, 0.6f, 0.65f);
        y += 8.0f;

        // Patience bar
        float pct = ship->patienceTimer / ship->maxPatience;
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 1.0f) pct = 1.0f;

        float pBarX = panelX + 8.0f;
        float pBarW = panelW - 16.0f;
        float pBarH = 10.0f;

        // Color: green -> yellow -> red as patience decreases
        float pr, pg, pb;
        if (pct > 0.5f) {
            pr = 0.3f; pg = 0.9f; pb = 0.3f; // green
        } else if (pct > 0.25f) {
            pr = 0.9f; pg = 0.9f; pb = 0.2f; // yellow
        } else {
            pr = 0.9f; pg = 0.2f; pb = 0.2f; // red
        }

        // Flash/pulse when below 25%
        float alpha = 1.0f;
        if (pct < 0.25f) {
            float t = static_cast<float>(glfwGetTime());
            alpha = 0.5f + 0.5f * std::sin(t * 6.0f); // pulse between 0.5 and 1.0
            if (alpha < 0.5f) alpha = 0.5f;
        }

        // Background
        drawRect(pBarX, y, pBarW, pBarH, 0.2f, 0.2f, 0.2f);
        // Fill
        drawRect(pBarX, y, pBarW * pct, pBarH, pr, pg, pb, alpha);
    }
}

glm::vec4 Renderer::getCellTypeColor(CellType type) {
    switch (type) {
        case CellType::WALL:           return {0.3f, 0.3f, 0.35f, 1.0f};
        case CellType::FLOOR:          return {0.6f, 0.6f, 0.65f, 1.0f};
        case CellType::DOOR:           return {0.6f, 0.3f, 0.1f, 1.0f};
        case CellType::TERMINAL:       return {0.2f, 0.4f, 0.9f, 1.0f};
        case CellType::DOCKING_COLLAR: return {0.8f, 0.5f, 0.2f, 1.0f};
        case CellType::STORAGE:        return {0.55f, 0.6f, 0.55f, 1.0f};
        case CellType::AIRLOCK:        return {0.5f, 0.55f, 0.6f, 1.0f};
        case CellType::SPAWN_POINT:    return {0.6f, 0.6f, 0.65f, 1.0f};
        default:                        return {0.2f, 0.2f, 0.25f, 1.0f};
    }
}

} // namespace ssm
