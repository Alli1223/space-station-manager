#include "client/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>

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
            return getCargoTypeColor(cargo->cargoType);
        }
        default:
            return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

void Renderer::renderObject(const GameObject* obj) {
    if (!obj->active) return;

    // Skip floor and wall - rendered by map
    if (obj->type == GameObjectType::WALL || obj->type == GameObjectType::FLOOR) return;

    // Animated door rendering — split from center
    if (obj->type == GameObjectType::DOOR) {
        auto* door = static_cast<const Door*>(obj);
        float doorR = 0.6f, doorG = 0.3f, doorB = 0.1f; // brown base
        // Lerp toward green as it opens
        float t = door->openAmount;
        doorR = doorR * (1.0f - t) + 0.2f * t;
        doorG = doorG * (1.0f - t) + 0.7f * t;
        doorB = doorB * (1.0f - t) + 0.2f * t;

        float slideOffset = t * (CELL_SIZE / 2.0f); // max slide = half cell

        if (door->orientation == 0) {
            // Walls on left+right → door splits horizontally (left half + right half)
            float halfW = obj->width / 2.0f;
            // Left half slides left
            drawRect(obj->x - slideOffset, obj->y, halfW, obj->height, doorR, doorG, doorB);
            // Right half slides right
            drawRect(obj->x + halfW + slideOffset, obj->y, halfW, obj->height, doorR, doorG, doorB);
        } else {
            // Walls on up+down → door splits vertically (top half + bottom half)
            float halfH = obj->height / 2.0f;
            // Top half slides up
            drawRect(obj->x, obj->y - slideOffset, obj->width, halfH, doorR, doorG, doorB);
            // Bottom half slides down
            drawRect(obj->x, obj->y + halfH + slideOffset, obj->width, halfH, doorR, doorG, doorB);
        }
        return;
    }

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
        // Draw dark interior for docked ships (accessible through open bottom)
        if (ship->state == ShipState::DOCKING || ship->state == ShipState::UNLOADING ||
            ship->state == ShipState::WAITING_RESUPPLY) {
            float wallT = 4.0f;
            drawRect(ship->x + wallT, ship->y + wallT,
                     ship->width - 2.0f * wallT, ship->height - wallT,
                     0.12f, 0.12f, 0.18f, 0.9f);
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
        case CargoType::METAL:    return {0.75f, 0.75f, 0.8f, 1.0f};   // silver
        case CargoType::ORE:      return {0.7f, 0.45f, 0.25f, 1.0f};   // rusty orange-brown
        case CargoType::FUEL:     return {0.9f, 0.9f, 0.2f, 1.0f};     // bright yellow
        case CargoType::FOOD:     return {0.3f, 0.8f, 0.3f, 1.0f};     // green
        case CargoType::CRYSTALS: return {0.6f, 0.3f, 0.9f, 1.0f};     // purple
        case CargoType::PLASMA:   return {0.2f, 0.9f, 0.9f, 1.0f};     // bright cyan
        default:                  return {0.5f, 0.5f, 0.5f, 1.0f};
    }
}

void Renderer::renderObjectivesPanel(const Ship* ship) {
    // Panel on the right side of screen
    float panelW = 120.0f;
    float panelH = 290.0f;
    float panelX = windowWidth - panelW - 10.0f;
    float panelY = 40.0f;

    // Panel background
    drawRect(panelX, panelY, panelW, panelH, 0.1f, 0.1f, 0.15f, 0.85f);

    float y = panelY + 8.0f;
    float iconSize = 16.0f;
    float dotSize = 6.0f;
    float sectionPad = 12.0f;

    // Helper lambda for export dot-indicator rows
    auto drawExportRow = [&](CargoType ctype, uint8_t remaining, uint8_t total, bool lastInSection) {
        if (total == 0) return; // skip rows with zero total
        glm::vec4 c = getCargoTypeColor(ctype);
        bool allTaken = (remaining == 0);
        drawRect(panelX + 8.0f, y, iconSize, iconSize, c.r, c.g, c.b);
        if (allTaken) {
            drawRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        }
        float dotX = panelX + 8.0f + iconSize + 6.0f;
        for (int i = 0; i < total; i++) {
            bool withdrawn = (i >= remaining);
            if (withdrawn) {
                drawRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.2f, 0.9f, 0.2f);
            } else {
                drawRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.5f, 0.5f, 0.55f);
            }
            dotX += dotSize + 2.0f;
        }
        y += iconSize + (lastInSection ? sectionPad : 6.0f);
    };

    // === EXPORT SECTION (items to take off ship) ===
    // Section header bar
    drawRect(panelX, y, panelW, 3.0f, 0.8f, 0.4f, 0.2f); // orange bar = export
    y += 8.0f;

    // Metal row
    drawExportRow(CargoType::METAL, ship->metalToUnload, ship->totalMetal, false);
    // Ore row
    drawExportRow(CargoType::ORE, ship->oreToUnload, ship->totalOre, false);
    // Crystals row
    drawExportRow(CargoType::CRYSTALS, ship->crystalsToUnload, ship->totalCrystals, false);
    // Plasma row
    drawExportRow(CargoType::PLASMA, ship->plasmaToUnload, ship->totalPlasma, true);

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

void Renderer::renderCargoTooltip(float screenX, float screenY, CargoType type) {
    // Tooltip: small dark panel with colored square + distinctive icon shape
    float tooltipW = 44.0f;
    float tooltipH = 24.0f;
    // Offset so tooltip doesn't overlap cursor
    float tx = screenX + 12.0f;
    float ty = screenY - tooltipH - 4.0f;
    // Clamp to window
    if (tx + tooltipW > windowWidth) tx = screenX - tooltipW - 4.0f;
    if (ty < 0) ty = screenY + 16.0f;

    // Background panel
    drawRect(tx, ty, tooltipW, tooltipH, 0.08f, 0.08f, 0.12f, 0.9f);
    // Border
    drawRect(tx, ty, tooltipW, 1.0f, 0.4f, 0.4f, 0.45f, 0.8f);
    drawRect(tx, ty + tooltipH - 1.0f, tooltipW, 1.0f, 0.4f, 0.4f, 0.45f, 0.8f);
    drawRect(tx, ty, 1.0f, tooltipH, 0.4f, 0.4f, 0.45f, 0.8f);
    drawRect(tx + tooltipW - 1.0f, ty, 1.0f, tooltipH, 0.4f, 0.4f, 0.45f, 0.8f);

    // Colored cargo square
    glm::vec4 c = getCargoTypeColor(type);
    float sqX = tx + 4.0f;
    float sqY = ty + 4.0f;
    float sqSize = 16.0f;
    drawRect(sqX, sqY, sqSize, sqSize, c.r, c.g, c.b);

    // Distinctive icon next to the square (approx 12x12, using simple shapes)
    float iconX = sqX + sqSize + 4.0f;
    float iconY = sqY + 2.0f;
    float iconS = 12.0f;

    switch (type) {
        case CargoType::METAL:
            // Three horizontal bars (ingot-like)
            drawRect(iconX, iconY, iconS, 3.0f, 0.9f, 0.9f, 0.95f);
            drawRect(iconX, iconY + 4.5f, iconS, 3.0f, 0.9f, 0.9f, 0.95f);
            drawRect(iconX, iconY + 9.0f, iconS, 3.0f, 0.9f, 0.9f, 0.95f);
            break;
        case CargoType::ORE:
            // Rough diamond shape (rotated square via 4 rects)
            drawRect(iconX + 3.0f, iconY, 6.0f, 4.0f, c.r, c.g, c.b);
            drawRect(iconX + 1.0f, iconY + 3.0f, 10.0f, 6.0f, c.r, c.g, c.b);
            drawRect(iconX + 3.0f, iconY + 8.0f, 6.0f, 4.0f, c.r, c.g, c.b);
            break;
        case CargoType::CRYSTALS:
            // Pointy triangle shape (stacked rects narrowing upward)
            drawRect(iconX + 4.0f, iconY, 4.0f, 3.0f, c.r, c.g, c.b);
            drawRect(iconX + 2.0f, iconY + 3.0f, 8.0f, 3.0f, c.r, c.g, c.b);
            drawRect(iconX, iconY + 6.0f, 12.0f, 6.0f, c.r, c.g, c.b);
            break;
        case CargoType::PLASMA:
            // Circle approximation (small filled square with rounded corners via overlapping rects)
            drawRect(iconX + 2.0f, iconY, 8.0f, 12.0f, c.r, c.g, c.b);
            drawRect(iconX, iconY + 2.0f, 12.0f, 8.0f, c.r, c.g, c.b);
            break;
        case CargoType::FUEL:
            // Flame shape (narrow at top, wide at bottom)
            drawRect(iconX + 4.0f, iconY, 4.0f, 4.0f, c.r, c.g, c.b);
            drawRect(iconX + 2.0f, iconY + 4.0f, 8.0f, 4.0f, c.r, c.g, c.b);
            drawRect(iconX + 1.0f, iconY + 8.0f, 10.0f, 4.0f, c.r, c.g, c.b);
            break;
        case CargoType::FOOD:
            // Leaf shape (small at edges, big in center)
            drawRect(iconX + 3.0f, iconY, 6.0f, 2.0f, c.r, c.g, c.b);
            drawRect(iconX + 1.0f, iconY + 2.0f, 10.0f, 8.0f, c.r, c.g, c.b);
            drawRect(iconX + 3.0f, iconY + 10.0f, 6.0f, 2.0f, c.r, c.g, c.b);
            break;
        default:
            drawRect(iconX, iconY, iconS, iconS, 0.5f, 0.5f, 0.5f);
            break;
    }
}

void Renderer::renderTetherRopes(const std::vector<GameObject*>& objects) {
    // Draw lines (as thin rects) from each tethered cargo to its anchor
    // Anchor: player (tetherOrder==0) or previous cargo in the chain

    // First, find all players (for color lookup)
    std::unordered_map<uint32_t, const Player*> playersById;
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::PLAYER && obj->active) {
            playersById[obj->id] = static_cast<const Player*>(obj);
        }
    }

    // Gather tethered cargo grouped by player, sorted by tetherOrder
    std::unordered_map<uint32_t, std::vector<const Cargo*>> tetheredByPlayer;
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::CARGO && obj->active) {
            auto* cargo = static_cast<const Cargo*>(obj);
            if (cargo->isTethered()) {
                tetheredByPlayer[cargo->tetheredToPlayerId].push_back(cargo);
            }
        }
    }

    for (auto& [playerId, cargoList] : tetheredByPlayer) {
        // Sort by tetherOrder
        std::sort(cargoList.begin(), cargoList.end(), [](const Cargo* a, const Cargo* b) {
            return a->tetherOrder < b->tetherOrder;
        });

        auto playerIt = playersById.find(playerId);
        if (playerIt == playersById.end()) continue;
        const Player* player = playerIt->second;

        // Get player color for the rope
        uint8_t ci = player->colorIndex % 8;
        float pr = PLAYER_COLORS[ci][0];
        float pg = PLAYER_COLORS[ci][1];
        float pb = PLAYER_COLORS[ci][2];

        for (size_t i = 0; i < cargoList.size(); i++) {
            const Cargo* cargo = cargoList[i];

            float toX, toY; // anchor point
            if (i == 0) {
                toX = player->x + player->width / 2.0f;
                toY = player->y + player->height / 2.0f;
            } else {
                toX = cargoList[i - 1]->x + cargoList[i - 1]->width / 2.0f;
                toY = cargoList[i - 1]->y + cargoList[i - 1]->height / 2.0f;
            }

            float fromX = cargo->x + cargo->width / 2.0f;
            float fromY = cargo->y + cargo->height / 2.0f;

            // Draw a series of small rects along the line
            float dx = toX - fromX;
            float dy = toY - fromY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 1.0f) continue;

            int segments = static_cast<int>(dist / 4.0f);
            if (segments < 2) segments = 2;
            if (segments > 30) segments = 30;

            float segW = 3.0f; // rope thickness
            float segH = 3.0f;

            for (int s = 0; s <= segments; s++) {
                float t = static_cast<float>(s) / static_cast<float>(segments);
                float sx = fromX + dx * t - segW / 2.0f;
                float sy = fromY + dy * t - segH / 2.0f;

                // Add a slight sag (catenary approximation)
                float sag = 4.0f * t * (1.0f - t) * (std::min)(dist * 0.1f, 8.0f);
                sy += sag;

                drawRect(sx, sy, segW, segH, pr, pg, pb, 0.8f);
            }
        }
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
