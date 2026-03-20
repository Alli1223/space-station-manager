#include "client/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace ssm {

// Legacy uniform-color shader (used for circles only)
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

// Per-vertex-color shader (used for all batched rects)
static const char* batchVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uProjection;
out vec4 vColor;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* batchFragmentShaderSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
    }
    return prog;
}

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

    // Legacy VAO/VBO (circles only)
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // Batched VAO/VBO (per-vertex color: x, y, r, g, b, a = 6 floats per vertex)
    glGenVertexArrays(1, &batchVao);
    glGenBuffers(1, &batchVbo);
    glBindVertexArray(batchVao);
    glBindBuffer(GL_ARRAY_BUFFER, batchVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    // Position: location 0, 2 floats, stride 6 floats, offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color: location 1, 4 floats, stride 6 floats, offset 2 floats
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Reserve batch capacity
    batchVertices.reserve(6000 * 6); // ~1000 quads initial

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!font.init()) {
        std::cerr << "Failed to init bitmap font" << std::endl;
        return false;
    }

    return true;
}

bool Renderer::createShaders() {
    // Legacy uniform-color shader
    {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
        shaderProgram = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        uProjection = glGetUniformLocation(shaderProgram, "uProjection");
        uColor = glGetUniformLocation(shaderProgram, "uColor");
    }

    // Batched per-vertex-color shader
    {
        GLuint vs = compileShader(GL_VERTEX_SHADER, batchVertexShaderSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, batchFragmentShaderSrc);
        batchShaderProgram = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        uBatchProjection = glGetUniformLocation(batchShaderProgram, "uProjection");
    }

    return true;
}

void Renderer::shutdown() {
    font.shutdown();
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (batchShaderProgram) glDeleteProgram(batchShaderProgram);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (batchVao) glDeleteVertexArrays(1, &batchVao);
    if (batchVbo) glDeleteBuffers(1, &batchVbo);
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

    // Set projection on both shaders (zoom scales the view)
    float halfW = (windowWidth / 2.0f) / zoom;
    float halfH = (windowHeight / 2.0f) / zoom;
    glm::mat4 proj = glm::ortho(
        cameraX - halfW,
        cameraX + halfW,
        cameraY + halfH,  // flip Y so Y-down
        cameraY - halfH,
        -1.0f, 1.0f
    );

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proj));

    glUseProgram(batchShaderProgram);
    glUniformMatrix4fv(uBatchProjection, 1, GL_FALSE, glm::value_ptr(proj));

    batchVertices.clear();
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void Renderer::setCamera(float x, float y) {
    cameraX = x;
    cameraY = y;
}

// =========================================================================
// Batched rect drawing — accumulate quads, flush once
// =========================================================================

void Renderer::batchRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    // 6 vertices per quad, 6 floats per vertex (x, y, r, g, b, a)
    float v[] = {
        x,     y,     r, g, b, a,
        x + w, y,     r, g, b, a,
        x + w, y + h, r, g, b, a,
        x,     y,     r, g, b, a,
        x + w, y + h, r, g, b, a,
        x,     y + h, r, g, b, a,
    };
    batchVertices.insert(batchVertices.end(), v, v + 36);
}

void Renderer::batchRotatedRect(float cx, float cy, float w, float h, float angle, float r, float g, float b, float a) {
    float hw = w / 2.0f;
    float hh = h / 2.0f;
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);
    auto rx = [&](float lx, float ly) { return cx + lx * cosA - ly * sinA; };
    auto ry = [&](float lx, float ly) { return cy + lx * sinA + ly * cosA; };
    float x0 = rx(-hw, -hh), y0 = ry(-hw, -hh);
    float x1 = rx( hw, -hh), y1 = ry( hw, -hh);
    float x2 = rx( hw,  hh), y2 = ry( hw,  hh);
    float x3 = rx(-hw,  hh), y3 = ry(-hw,  hh);
    float v[] = {
        x0, y0, r, g, b, a,  x1, y1, r, g, b, a,  x2, y2, r, g, b, a,
        x0, y0, r, g, b, a,  x2, y2, r, g, b, a,  x3, y3, r, g, b, a,
    };
    batchVertices.insert(batchVertices.end(), v, v + 36);
}

void Renderer::flushBatch() {
    if (batchVertices.empty()) return;

    glUseProgram(batchShaderProgram);
    glBindVertexArray(batchVao);
    glBindBuffer(GL_ARRAY_BUFFER, batchVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 batchVertices.size() * sizeof(float),
                 batchVertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batchVertices.size() / 6));

    batchVertices.clear();
}

// Legacy single-rect draw (used only for drawWorldRect/drawUIRect/drawUIOutline)
void Renderer::drawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    batchRect(x, y, w, h, r, g, b, a);
    flushBatch();
}

void Renderer::drawCircle(float cx, float cy, float radius, float r, float g, float b) {
    // Must flush any pending batch first, then switch to legacy shader
    flushBatch();

    const int segments = 20;
    float vertices[(segments + 2) * 2];

    vertices[0] = cx;
    vertices[1] = cy;
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * 3.14159f * i / segments;
        vertices[(i + 1) * 2] = cx + radius * std::cos(angle);
        vertices[(i + 1) * 2 + 1] = cy + radius * std::sin(angle);
    }

    glUseProgram(shaderProgram);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glUniform4f(uColor, r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
}

// =========================================================================
// Frustum culling helper
// =========================================================================

bool Renderer::isOnScreen(float x, float y, float w, float h) const {
    float halfW = (windowWidth / 2.0f) / zoom;
    float halfH = (windowHeight / 2.0f) / zoom;
    return !(x + w < cameraX - halfW || x > cameraX + halfW ||
             y + h < cameraY - halfH || y > cameraY + halfH);
}

// =========================================================================
// Map rendering — fully batched
// =========================================================================

void Renderer::renderMap(const StationMap& map) {
    float halfW = (windowWidth / 2.0f) / zoom;
    float halfH = (windowHeight / 2.0f) / zoom;

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
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.3f, 0.3f, 0.35f); break;
                case CellType::FLOOR:
                case CellType::SPAWN_POINT:
                case CellType::DOOR:
                case CellType::TERMINAL:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.6f, 0.6f, 0.65f); break;
                case CellType::AIRLOCK:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.5f, 0.55f, 0.6f); break;
                case CellType::STORAGE:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.55f, 0.6f, 0.55f); break;
                case CellType::DOCKING_COLLAR:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.8f, 0.5f, 0.2f); break;
                case CellType::LANDING_PAD:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.5f, 0.55f, 0.65f); break;
                case CellType::HANGAR_DOOR:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.4f, 0.4f, 0.5f); break;
                case CellType::REFINERY:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.7f, 0.45f, 0.2f); break;
                case CellType::TURRET_BASE:
                    batchRect(wx, wy, CELL_SIZE, CELL_SIZE, 0.25f, 0.25f, 0.3f); break;
                default: break;
            }
        }
    }

    flushBatch();
}

glm::vec4 Renderer::getColorForObject(const GameObject* obj) const {
    switch (obj->type) {
        case GameObjectType::DOOR: {
            auto* door = static_cast<const Door*>(obj);
            if (door->state == DoorState::OPEN)
                return {0.2f, 0.7f, 0.2f, 1.0f};
            else
                return {0.6f, 0.3f, 0.1f, 1.0f};
        }
        case GameObjectType::TERMINAL:
            return {0.2f, 0.4f, 0.9f, 1.0f};
        case GameObjectType::DOCKING_COLLAR:
            return {0.9f, 0.6f, 0.1f, 1.0f};
        case GameObjectType::SHIP: {
            auto* ship = static_cast<const Ship*>(obj);
            switch (ship->shipClass) {
                case ShipClass::SMALL:  return {0.85f, 0.9f, 0.95f, 1.0f};
                case ShipClass::MEDIUM: return {0.9f, 0.9f, 0.95f, 1.0f};
                case ShipClass::LARGE:  return {0.95f, 0.88f, 0.85f, 1.0f};
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

// =========================================================================
// Object rendering — batched with frustum culling
// =========================================================================

void Renderer::renderObject(const GameObject* obj) {
    if (!obj->active) return;
    if (obj->type == GameObjectType::WALL || obj->type == GameObjectType::FLOOR) return;

    // Animated door rendering — split from center
    if (obj->type == GameObjectType::DOOR) {
        auto* door = static_cast<const Door*>(obj);
        if (!isOnScreen(obj->x - CELL_SIZE, obj->y - CELL_SIZE, obj->width + 2 * CELL_SIZE, obj->height + 2 * CELL_SIZE)) return;

        float t = door->openAmount;
        float doorR, doorG, doorB;
        if (door->isHangarDoor) {
            // Hangar doors: dark steel when closed, dim when open
            doorR = 0.45f * (1.0f - t) + 0.25f * t;
            doorG = 0.45f * (1.0f - t) + 0.25f * t;
            doorB = 0.55f * (1.0f - t) + 0.3f * t;
        } else if (door->isAirlock) {
            // Airlock doors: red-orange when closed, green when open
            doorR = 0.8f * (1.0f - t) + 0.2f * t;
            doorG = 0.25f * (1.0f - t) + 0.7f * t;
            doorB = 0.15f * (1.0f - t) + 0.2f * t;
        } else {
            // Regular doors: brown when closed, green when open
            doorR = 0.6f * (1.0f - t) + 0.2f * t;
            doorG = 0.3f * (1.0f - t) + 0.7f * t;
            doorB = 0.1f * (1.0f - t) + 0.2f * t;
        }

        float slideOffset = t * (CELL_SIZE / 2.0f);

        if (door->orientation == 0) {
            float halfW = obj->width / 2.0f;
            batchRect(obj->x - slideOffset, obj->y, halfW, obj->height, doorR, doorG, doorB);
            batchRect(obj->x + halfW + slideOffset, obj->y, halfW, obj->height, doorR, doorG, doorB);
        } else {
            float halfH = obj->height / 2.0f;
            batchRect(obj->x, obj->y - slideOffset, obj->width, halfH, doorR, doorG, doorB);
            batchRect(obj->x, obj->y + halfH + slideOffset, obj->width, halfH, doorR, doorG, doorB);
        }
        return;
    }

    if (obj->type == GameObjectType::PLAYER) {
        auto* player = static_cast<const Player*>(obj);
        if (!isOnScreen(obj->x, obj->y, obj->width, obj->height)) return;

        // Flush batch before switching to circle shader
        flushBatch();

        float cx = player->x + player->width / 2.0f;
        float cy = player->y + player->height / 2.0f;
        uint8_t ci = player->colorIndex % 8;
        drawCircle(cx, cy, player->width / 2.0f,
                   PLAYER_COLORS[ci][0], PLAYER_COLORS[ci][1], PLAYER_COLORS[ci][2]);
        if (player->isCarrying()) {
            drawCircle(cx, cy - player->height / 2.0f - 4.0f, 4.0f, 1.0f, 1.0f, 0.0f);
        }
        return;
    }

    if (obj->type == GameObjectType::SHIP) {
        auto* ship = static_cast<const Ship*>(obj);
        if (!isOnScreen(obj->x, obj->y, obj->width, obj->height)) return;

        glm::vec4 color = getColorForObject(obj);
        batchRect(obj->x, obj->y, obj->width, obj->height, color.r, color.g, color.b, color.a);
        if (ship->state == ShipState::DOCKING || ship->state == ShipState::UNLOADING ||
            ship->state == ShipState::WAITING_RESUPPLY) {
            float wallT = 4.0f;
            batchRect(ship->x + wallT, ship->y + wallT,
                     ship->width - 2.0f * wallT, ship->height - wallT,
                     0.12f, 0.12f, 0.18f, 0.9f);
        }
        return;
    }

    if (obj->type == GameObjectType::TURRET) {
        auto* turret = static_cast<const Turret*>(obj);
        if (!isOnScreen(obj->x - 16, obj->y - 16, obj->width + 32, obj->height + 32)) return;

        float cx = turret->x + turret->width / 2.0f;
        float cy = turret->y + turret->height / 2.0f;

        // Base square
        float baseR = 0.25f, baseG = 0.25f, baseB = 0.3f;
        if (turret->operatorId != 0) {
            baseR = 0.3f; baseG = 0.35f; baseB = 0.4f;
        }
        batchRect(turret->x, turret->y, turret->width, turret->height, baseR, baseG, baseB);

        // Barrel — rotated rect extending in aimAngle direction
        float barrelW, barrelH, barrelR, barrelG, barrelB;
        if (turret->turretType == TurretType::ENERGY) {
            barrelW = 18.0f; barrelH = 8.0f;
            barrelR = 0.2f; barrelG = 0.4f; barrelB = 1.0f;
        } else {
            barrelW = 24.0f; barrelH = 5.0f;
            barrelR = 0.55f; barrelG = 0.55f; barrelB = 0.55f;
        }
        float offsetDist = barrelW / 2.0f;
        float barrelCX = cx + std::cos(turret->aimAngle) * offsetDist;
        float barrelCY = cy + std::sin(turret->aimAngle) * offsetDist;
        batchRotatedRect(barrelCX, barrelCY, barrelW, barrelH, turret->aimAngle, barrelR, barrelG, barrelB);

        // Ammo bar for kinetic turrets
        if (turret->turretType == TurretType::KINETIC && turret->maxAmmo > 0) {
            float ammoPct = static_cast<float>(turret->ammo) / static_cast<float>(turret->maxAmmo);
            float barW = turret->width;
            float barH = 3.0f;
            batchRect(turret->x, turret->y + turret->height + 2.0f, barW, barH, 0.2f, 0.2f, 0.2f);
            batchRect(turret->x, turret->y + turret->height + 2.0f, barW * ammoPct, barH, 0.6f, 0.6f, 0.7f);
        }
        return;
    }

    if (obj->type == GameObjectType::PROJECTILE) {
        auto* proj = static_cast<const Projectile*>(obj);
        if (!isOnScreen(obj->x, obj->y, obj->width, obj->height)) return;
        float pr, pg, pb;
        if (proj->owner == ProjectileOwner::STATION) {
            pr = 1.0f; pg = 0.9f; pb = 0.3f;
        } else {
            pr = 1.0f; pg = 0.2f; pb = 0.1f;
        }
        batchRect(obj->x, obj->y, obj->width, obj->height, pr, pg, pb);
        return;
    }

    if (obj->type == GameObjectType::ENEMY_SHIP) {
        auto* enemy = static_cast<const EnemyShip*>(obj);
        if (!isOnScreen(obj->x, obj->y, obj->width, obj->height)) return;
        batchRect(obj->x, obj->y, obj->width, obj->height, 0.8f, 0.15f, 0.1f);
        float wallT = 3.0f;
        batchRect(obj->x + wallT, obj->y + wallT,
                  obj->width - 2.0f * wallT, obj->height - 2.0f * wallT,
                  0.5f, 0.1f, 0.08f);
        if (enemy->health < enemy->maxHealth) {
            float hpPct = enemy->health / enemy->maxHealth;
            float barW = obj->width;
            float barH = 4.0f;
            batchRect(obj->x, obj->y - barH - 2.0f, barW, barH, 0.2f, 0.2f, 0.2f);
            batchRect(obj->x, obj->y - barH - 2.0f, barW * hpPct, barH,
                      1.0f - hpPct, hpPct, 0.1f);
        }
        return;
    }

    // Generic objects (terminals, collars, cargo)
    if (!isOnScreen(obj->x, obj->y, obj->width, obj->height)) return;
    glm::vec4 color = getColorForObject(obj);
    batchRect(obj->x, obj->y, obj->width, obj->height, color.r, color.g, color.b, color.a);
}

void Renderer::renderObjects(const std::vector<GameObject*>& objects) {
    // Render in order: doors, terminals/collars, ships, cargo, players
    // Batch all rects together and flush between layers that need different draw modes

    // Layer 1: Doors (all batched rects)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::DOOR) renderObject(obj);
    }

    // Layer 2: Terminals + collars (batched rects)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::TERMINAL || obj->type == GameObjectType::DOCKING_COLLAR)
            renderObject(obj);
    }

    // Layer 3: Turrets (batched rects)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::TURRET) renderObject(obj);
    }

    // Layer 4: Ships + enemy ships (batched rects)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::SHIP || obj->type == GameObjectType::ENEMY_SHIP)
            renderObject(obj);
    }

    // Layer 5: Cargo (batched rects)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::CARGO) renderObject(obj);
    }

    // Flush all accumulated rects before drawing circles
    flushBatch();

    // Layer 6: Players (circles — each is an individual draw call, but there are few)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::PLAYER) renderObject(obj);
    }

    // Layer 7: Projectiles (on top of everything)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::PROJECTILE) renderObject(obj);
    }
    flushBatch();
}

void Renderer::renderHUD(const Player* localPlayer, const std::vector<GameObject*>& objects, int32_t stationMoney) {
    beginScreenSpace();

    if (!localPlayer) {
        endScreenSpace();
        return;
    }

    // Show carrying status
    if (localPlayer->isCarrying()) {
        batchRect(2.0f, 2.0f, 26.0f, 26.0f, 0.1f, 0.1f, 0.15f, 0.8f);
        for (auto* obj : objects) {
            if (obj->id == localPlayer->carryingCargoId && obj->type == GameObjectType::CARGO) {
                auto* cargo = static_cast<const Cargo*>(obj);
                glm::vec4 cc = getColorForObject(cargo);
                batchRect(5.0f, 5.0f, 20.0f, 20.0f, cc.r, cc.g, cc.b);
                break;
            }
        }
    }

    // Money display
    {
        float moneyY = 34.0f;
        flushBatch();
        std::string moneyStr = "$" + std::to_string(stationMoney);
        float textScale = 1.5f;
        if (stationMoney >= 0) {
            drawText(4.0f, moneyY, moneyStr, 0.2f, 0.9f, 0.3f, 1.0f, textScale);
        } else {
            drawText(4.0f, moneyY, moneyStr, 0.9f, 0.2f, 0.2f, 1.0f, textScale);
        }
    }

    // Stamina bar (bottom-left, above hotbar area)
    {
        float staminaBarX = 4.0f;
        float staminaBarY = static_cast<float>(windowHeight) - 24.0f;
        float staminaBarW = 100.0f;
        float staminaBarH = 6.0f;
        float staminaPct = (localPlayer->maxStamina > 0.0f) ?
            localPlayer->stamina / localPlayer->maxStamina : 1.0f;

        // Only show when stamina is not full (avoid clutter)
        if (staminaPct < 0.99f) {
            // Background
            batchRect(staminaBarX, staminaBarY, staminaBarW, staminaBarH, 0.15f, 0.15f, 0.2f, 0.7f);
            // Fill — color shifts from cyan to red as stamina depletes
            float fillW = staminaBarW * staminaPct;
            float sr = 0.2f + (1.0f - staminaPct) * 0.7f; // 0.2 → 0.9
            float sg = 0.7f * staminaPct + 0.2f;            // 0.9 → 0.2
            float sb = 0.9f * staminaPct;                    // 0.9 → 0.0
            batchRect(staminaBarX, staminaBarY, fillW, staminaBarH, sr, sg, sb, 0.9f);
            // Small sprint icon (lightning bolt shape — just a small yellow rect as indicator)
            batchRect(staminaBarX + staminaBarW + 4.0f, staminaBarY - 1.0f, 4.0f, 8.0f, 1.0f, 0.9f, 0.2f, 0.8f);
        }
    }

    // Turret mode HUD
    if (localPlayer->isInTurret()) {
        const Turret* activeTurret = nullptr;
        for (auto* obj : objects) {
            if (obj->type == GameObjectType::TURRET && obj->id == localPlayer->operatingTurretId) {
                activeTurret = static_cast<const Turret*>(obj);
                break;
            }
        }
        if (activeTurret) {
            flushBatch();
            float textScale = 2.0f;
            std::string turretLabel;
            if (activeTurret->turretType == TurretType::ENERGY) {
                turretLabel = "ENERGY TURRET";
            } else {
                std::string ammoStr = std::to_string(activeTurret->ammo) + "/" + std::to_string(activeTurret->maxAmmo);
                turretLabel = "KINETIC [" + ammoStr + "]";
            }
            float labelW = measureText(turretLabel, textScale);
            float labelX = windowWidth / 2.0f - labelW / 2.0f;
            // Background bar
            batchRect(labelX - 8.0f, 6.0f, labelW + 16.0f, 22.0f, 0.1f, 0.1f, 0.15f, 0.8f);
            flushBatch();
            if (activeTurret->turretType == TurretType::ENERGY) {
                drawText(labelX, 8.0f, turretLabel, 0.3f, 0.5f, 1.0f, 1.0f, textScale);
            } else {
                drawText(labelX, 8.0f, turretLabel, 0.7f, 0.7f, 0.75f, 1.0f, textScale);
            }

            // "Press E to exit" hint
            std::string exitHint = "Press E to exit";
            float hintW = measureText(exitHint, 1.5f);
            drawText(windowWidth / 2.0f - hintW / 2.0f, 32.0f, exitHint, 0.6f, 0.6f, 0.6f, 0.7f, 1.5f);
        }

        // Crosshair
        float chX = windowWidth / 2.0f;
        float chY = windowHeight / 2.0f;
        float chSize = 12.0f;
        float chThick = 2.0f;
        batchRect(chX - chSize, chY - chThick / 2, chSize * 2, chThick, 1.0f, 1.0f, 1.0f, 0.5f);
        batchRect(chX - chThick / 2, chY - chSize, chThick, chSize * 2, 1.0f, 1.0f, 1.0f, 0.5f);
    }

    // Find nearest docked ship
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

    // Flush all HUD rects in one draw call
    flushBatch();

    endScreenSpace();
}

void Renderer::beginScreenSpace() {
    // Flush any pending world-space batch
    flushBatch();

    glm::mat4 proj = glm::ortho(0.0f, (float)windowWidth, (float)windowHeight, 0.0f, -1.0f, 1.0f);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proj));
    glUseProgram(batchShaderProgram);
    glUniformMatrix4fv(uBatchProjection, 1, GL_FALSE, glm::value_ptr(proj));
}

void Renderer::endScreenSpace() {
    // Flush any pending screen-space batch
    flushBatch();

    float halfW = (windowWidth / 2.0f) / zoom;
    float halfH = (windowHeight / 2.0f) / zoom;
    glm::mat4 proj = glm::ortho(
        cameraX - halfW,
        cameraX + halfW,
        cameraY + halfH,
        cameraY - halfH,
        -1.0f, 1.0f
    );
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(proj));
    glUseProgram(batchShaderProgram);
    glUniformMatrix4fv(uBatchProjection, 1, GL_FALSE, glm::value_ptr(proj));
}

void Renderer::drawUIRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    batchRect(x, y, w, h, r, g, b, a);
    flushBatch();
}

void Renderer::drawUIOutline(float x, float y, float w, float h, float thickness, float r, float g, float b, float a) {
    batchRect(x, y, w, thickness, r, g, b, a);
    batchRect(x, y + h - thickness, w, thickness, r, g, b, a);
    batchRect(x, y, thickness, h, r, g, b, a);
    batchRect(x + w - thickness, y, thickness, h, r, g, b, a);
    flushBatch();
}

void Renderer::drawWorldRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    batchRect(x, y, w, h, r, g, b, a);
    flushBatch();
}

glm::vec4 Renderer::getCargoTypeColor(CargoType type) {
    switch (type) {
        case CargoType::METAL:    return {0.75f, 0.75f, 0.8f, 1.0f};
        case CargoType::ORE:      return {0.7f, 0.45f, 0.25f, 1.0f};
        case CargoType::FUEL:     return {0.9f, 0.9f, 0.2f, 1.0f};
        case CargoType::FOOD:     return {0.3f, 0.8f, 0.3f, 1.0f};
        case CargoType::CRYSTALS: return {0.6f, 0.3f, 0.9f, 1.0f};
        case CargoType::PLASMA:   return {0.2f, 0.9f, 0.9f, 1.0f};
        default:                  return {0.5f, 0.5f, 0.5f, 1.0f};
    }
}

void Renderer::renderObjectivesPanel(const Ship* ship) {
    float panelW = 120.0f;
    float panelH = 290.0f;
    float panelX = windowWidth - panelW - 10.0f;
    float panelY = 40.0f;

    batchRect(panelX, panelY, panelW, panelH, 0.1f, 0.1f, 0.15f, 0.85f);

    float y = panelY + 8.0f;
    float iconSize = 16.0f;
    float dotSize = 6.0f;
    float sectionPad = 12.0f;

    auto drawExportRow = [&](CargoType ctype, uint8_t remaining, uint8_t total, bool lastInSection) {
        if (total == 0) return;
        glm::vec4 c = getCargoTypeColor(ctype);
        bool allTaken = (remaining == 0);
        batchRect(panelX + 8.0f, y, iconSize, iconSize, c.r, c.g, c.b);
        if (allTaken) {
            batchRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        }
        float dotX = panelX + 8.0f + iconSize + 6.0f;
        for (int i = 0; i < total; i++) {
            bool withdrawn = (i >= remaining);
            if (withdrawn) {
                batchRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.2f, 0.9f, 0.2f);
            } else {
                batchRect(dotX, y + (iconSize - dotSize) / 2.0f, dotSize, dotSize, 0.5f, 0.5f, 0.55f);
            }
            dotX += dotSize + 2.0f;
        }
        y += iconSize + (lastInSection ? sectionPad : 6.0f);
    };

    // EXPORT SECTION
    batchRect(panelX, y, panelW, 3.0f, 0.8f, 0.4f, 0.2f);
    y += 8.0f;
    drawExportRow(CargoType::METAL, ship->metalToUnload, ship->totalMetal, false);
    drawExportRow(CargoType::ORE, ship->oreToUnload, ship->totalOre, false);
    drawExportRow(CargoType::CRYSTALS, ship->crystalsToUnload, ship->totalCrystals, false);
    drawExportRow(CargoType::PLASMA, ship->plasmaToUnload, ship->totalPlasma, true);

    // IMPORT SECTION
    batchRect(panelX, y, panelW, 3.0f, 0.2f, 0.6f, 0.9f);
    y += 8.0f;

    // Fuel row
    {
        glm::vec4 fc = getCargoTypeColor(CargoType::FUEL);
        float pct = ship->fuel / ship->maxFuel;
        bool full = (pct >= 1.0f);
        batchRect(panelX + 8.0f, y, iconSize, iconSize, fc.r, fc.g, fc.b);
        if (full) batchRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        float barX = panelX + 8.0f + iconSize + 6.0f;
        float barW = panelW - iconSize - 22.0f;
        float barH = 8.0f;
        float barY = y + (iconSize - barH) / 2.0f;
        batchRect(barX, barY, barW, barH, 0.2f, 0.2f, 0.2f);
        batchRect(barX, barY, barW * pct, barH, fc.r, fc.g, fc.b);
        if (full) batchRect(barX, barY, barW, barH, 0.2f, 0.9f, 0.2f, 0.3f);
        y += iconSize + 6.0f;
    }

    // Food row
    {
        glm::vec4 fc = getCargoTypeColor(CargoType::FOOD);
        float pct = ship->food / ship->maxFood;
        bool full = (pct >= 1.0f);
        batchRect(panelX + 8.0f, y, iconSize, iconSize, fc.r, fc.g, fc.b);
        if (full) batchRect(panelX + 8.0f, y, iconSize, iconSize, 0.2f, 0.9f, 0.2f, 0.5f);
        float barX = panelX + 8.0f + iconSize + 6.0f;
        float barW = panelW - iconSize - 22.0f;
        float barH = 8.0f;
        float barY = y + (iconSize - barH) / 2.0f;
        batchRect(barX, barY, barW, barH, 0.2f, 0.2f, 0.2f);
        batchRect(barX, barY, barW * pct, barH, fc.r, fc.g, fc.b);
        if (full) batchRect(barX, barY, barW, barH, 0.2f, 0.9f, 0.2f, 0.3f);
        y += iconSize + sectionPad;
    }

    // PATIENCE SECTION
    if (ship->state == ShipState::WAITING_RESUPPLY && ship->maxPatience > 0.0f) {
        batchRect(panelX, y, panelW, 3.0f, 0.6f, 0.6f, 0.65f);
        y += 8.0f;

        float pct = ship->patienceTimer / ship->maxPatience;
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 1.0f) pct = 1.0f;

        float pBarX = panelX + 8.0f;
        float pBarW = panelW - 16.0f;
        float pBarH = 10.0f;

        float pr, pg, pb;
        if (pct > 0.5f) { pr = 0.3f; pg = 0.9f; pb = 0.3f; }
        else if (pct > 0.25f) { pr = 0.9f; pg = 0.9f; pb = 0.2f; }
        else { pr = 0.9f; pg = 0.2f; pb = 0.2f; }

        float alpha = 1.0f;
        if (pct < 0.25f) {
            float t = static_cast<float>(glfwGetTime());
            alpha = 0.5f + 0.5f * std::sin(t * 6.0f);
            if (alpha < 0.5f) alpha = 0.5f;
        }

        batchRect(pBarX, y, pBarW, pBarH, 0.2f, 0.2f, 0.2f);
        batchRect(pBarX, y, pBarW * pct, pBarH, pr, pg, pb, alpha);
    }
}

void Renderer::renderCargoTooltip(float screenX, float screenY, CargoType type) {
    float tooltipW = 44.0f;
    float tooltipH = 24.0f;
    float tx = screenX + 12.0f;
    float ty = screenY - tooltipH - 4.0f;
    if (tx + tooltipW > windowWidth) tx = screenX - tooltipW - 4.0f;
    if (ty < 0) ty = screenY + 16.0f;

    batchRect(tx, ty, tooltipW, tooltipH, 0.08f, 0.08f, 0.12f, 0.9f);
    batchRect(tx, ty, tooltipW, 1.0f, 0.4f, 0.4f, 0.45f, 0.8f);
    batchRect(tx, ty + tooltipH - 1.0f, tooltipW, 1.0f, 0.4f, 0.4f, 0.45f, 0.8f);
    batchRect(tx, ty, 1.0f, tooltipH, 0.4f, 0.4f, 0.45f, 0.8f);
    batchRect(tx + tooltipW - 1.0f, ty, 1.0f, tooltipH, 0.4f, 0.4f, 0.45f, 0.8f);

    glm::vec4 c = getCargoTypeColor(type);
    float sqX = tx + 4.0f;
    float sqY = ty + 4.0f;
    float sqSize = 16.0f;
    batchRect(sqX, sqY, sqSize, sqSize, c.r, c.g, c.b);

    float iconX = sqX + sqSize + 4.0f;
    float iconY = sqY + 2.0f;
    float iconS = 12.0f;

    switch (type) {
        case CargoType::METAL:
            batchRect(iconX, iconY, iconS, 3.0f, 0.9f, 0.9f, 0.95f);
            batchRect(iconX, iconY + 4.5f, iconS, 3.0f, 0.9f, 0.9f, 0.95f);
            batchRect(iconX, iconY + 9.0f, iconS, 3.0f, 0.9f, 0.9f, 0.95f);
            break;
        case CargoType::ORE:
            batchRect(iconX + 3.0f, iconY, 6.0f, 4.0f, c.r, c.g, c.b);
            batchRect(iconX + 1.0f, iconY + 3.0f, 10.0f, 6.0f, c.r, c.g, c.b);
            batchRect(iconX + 3.0f, iconY + 8.0f, 6.0f, 4.0f, c.r, c.g, c.b);
            break;
        case CargoType::CRYSTALS:
            batchRect(iconX + 4.0f, iconY, 4.0f, 3.0f, c.r, c.g, c.b);
            batchRect(iconX + 2.0f, iconY + 3.0f, 8.0f, 3.0f, c.r, c.g, c.b);
            batchRect(iconX, iconY + 6.0f, 12.0f, 6.0f, c.r, c.g, c.b);
            break;
        case CargoType::PLASMA:
            batchRect(iconX + 2.0f, iconY, 8.0f, 12.0f, c.r, c.g, c.b);
            batchRect(iconX, iconY + 2.0f, 12.0f, 8.0f, c.r, c.g, c.b);
            break;
        case CargoType::FUEL:
            batchRect(iconX + 4.0f, iconY, 4.0f, 4.0f, c.r, c.g, c.b);
            batchRect(iconX + 2.0f, iconY + 4.0f, 8.0f, 4.0f, c.r, c.g, c.b);
            batchRect(iconX + 1.0f, iconY + 8.0f, 10.0f, 4.0f, c.r, c.g, c.b);
            break;
        case CargoType::FOOD:
            batchRect(iconX + 3.0f, iconY, 6.0f, 2.0f, c.r, c.g, c.b);
            batchRect(iconX + 1.0f, iconY + 2.0f, 10.0f, 8.0f, c.r, c.g, c.b);
            batchRect(iconX + 3.0f, iconY + 10.0f, 6.0f, 2.0f, c.r, c.g, c.b);
            break;
        default:
            batchRect(iconX, iconY, iconS, iconS, 0.5f, 0.5f, 0.5f);
            break;
    }

    flushBatch();
}

void Renderer::renderTetherRopes(const std::vector<GameObject*>& objects) {
    std::unordered_map<uint32_t, const Player*> playersById;
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::PLAYER && obj->active) {
            playersById[obj->id] = static_cast<const Player*>(obj);
        }
    }

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
        std::sort(cargoList.begin(), cargoList.end(), [](const Cargo* a, const Cargo* b) {
            return a->tetherOrder < b->tetherOrder;
        });

        auto playerIt = playersById.find(playerId);
        if (playerIt == playersById.end()) continue;
        const Player* player = playerIt->second;

        uint8_t ci = player->colorIndex % 8;
        float pr = PLAYER_COLORS[ci][0];
        float pg = PLAYER_COLORS[ci][1];
        float pb = PLAYER_COLORS[ci][2];

        for (size_t i = 0; i < cargoList.size(); i++) {
            const Cargo* cargo = cargoList[i];

            float toX, toY;
            if (i == 0) {
                toX = player->x + player->width / 2.0f;
                toY = player->y + player->height / 2.0f;
            } else {
                toX = cargoList[i - 1]->x + cargoList[i - 1]->width / 2.0f;
                toY = cargoList[i - 1]->y + cargoList[i - 1]->height / 2.0f;
            }

            float fromX = cargo->x + cargo->width / 2.0f;
            float fromY = cargo->y + cargo->height / 2.0f;

            float dx = toX - fromX;
            float dy = toY - fromY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 1.0f) continue;

            int segments = static_cast<int>(dist / 4.0f);
            if (segments < 2) segments = 2;
            if (segments > 30) segments = 30;

            float segW = 3.0f;
            float segH = 3.0f;

            for (int s = 0; s <= segments; s++) {
                float t = static_cast<float>(s) / static_cast<float>(segments);
                float sx = fromX + dx * t - segW / 2.0f;
                float sy = fromY + dy * t - segH / 2.0f;

                float sag = 4.0f * t * (1.0f - t) * (std::min)(dist * 0.1f, 8.0f);
                sy += sag;

                batchRect(sx, sy, segW, segH, pr, pg, pb, 0.8f);
            }
        }
    }

    // Flush all tether rope segments in one draw call
    flushBatch();
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
        case CellType::LANDING_PAD:    return {0.5f, 0.55f, 0.65f, 1.0f};
        case CellType::HANGAR_DOOR:    return {0.45f, 0.45f, 0.55f, 1.0f};
        case CellType::REFINERY:       return {0.7f, 0.45f, 0.2f, 1.0f};
        case CellType::TURRET_BASE:    return {0.25f, 0.25f, 0.3f, 1.0f};
        default:                        return {0.2f, 0.2f, 0.25f, 1.0f};
    }
}

// --- Text rendering wrappers ---

void Renderer::drawText(float x, float y, const std::string& text,
                         float r, float g, float b, float a, float scale) {
    font.beginText(windowWidth, windowHeight);
    font.drawText(x, y, text, r, g, b, a, scale);
    font.endText();
    // Restore batch shader state
    glUseProgram(batchShaderProgram);
}

float Renderer::measureText(const std::string& text, float scale) const {
    return font.measureText(text, scale);
}

float Renderer::getTextHeight(float scale) const {
    return font.getCharHeight(scale);
}

void Renderer::renderTooltip(const std::string& title, const std::string& description) {
    if (title.empty() && description.empty()) return;

    float scale = 2.0f;
    float padding = 10.0f;
    float lineHeight = getTextHeight(scale) + 4.0f;

    float titleW = measureText(title, scale);
    float descW = measureText(description, scale);
    float boxW = (std::max)(titleW, descW) + padding * 2.0f;
    float boxH = padding * 2.0f + lineHeight;
    if (!description.empty()) boxH += lineHeight;

    // Position in bottom-right corner
    float boxX = windowWidth - boxW - 20.0f;
    float boxY = windowHeight - boxH - 20.0f;

    // Background
    drawUIRect(boxX, boxY, boxW, boxH, 0.1f, 0.1f, 0.15f, 0.85f);
    drawUIOutline(boxX, boxY, boxW, boxH, 2.0f, 0.4f, 0.4f, 0.5f, 0.9f);

    // Title text
    font.beginText(windowWidth, windowHeight);
    font.drawText(boxX + padding, boxY + padding, title, 1.0f, 1.0f, 0.7f, 1.0f, scale);

    // Description text
    if (!description.empty()) {
        font.drawText(boxX + padding, boxY + padding + lineHeight,
                      description, 0.7f, 0.7f, 0.75f, 1.0f, scale);
    }
    font.endText();
    glUseProgram(batchShaderProgram);
}

} // namespace ssm
