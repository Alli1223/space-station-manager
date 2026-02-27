#include "client/game_client.h"
#include <iostream>
#include <chrono>
#include <cmath>

namespace ssm {

GameClient::~GameClient() {
    clearGameObjects();
}

bool GameClient::init(const std::string& host, uint16_t port, const std::string& name) {
    playerName = name;

    if (!renderer.init(1024, 768)) {
        std::cerr << "Failed to init renderer" << std::endl;
        return false;
    }

    // --- Setup UI ---

    // Edit button (top-right corner)
    auto editBtn = std::make_unique<UIButton>(
        static_cast<float>(renderer.getWindowWidth()) - 80.0f, 5.0f,
        70.0f, 25.0f,
        [this]() { toggleEditMode(); }
    );
    editBtn->colorR = 0.2f; editBtn->colorG = 0.25f; editBtn->colorB = 0.35f;
    editBtn->hoverR = 0.3f; editBtn->hoverG = 0.35f; editBtn->hoverB = 0.5f;
    editButton = editBtn.get();
    uiManager.addElement(std::move(editBtn));

    // Hotbar (centered at bottom, initially hidden)
    auto hbar = std::make_unique<UIHotbar>();
    hbar->init(static_cast<float>(renderer.getWindowWidth()),
               static_cast<float>(renderer.getWindowHeight()));
    hbar->visible = false;
    hotbar = hbar.get();
    uiManager.addElement(std::move(hbar));

    // --- Setup network callbacks ---
    network.onWelcome = [this](uint32_t playerId, const StationMap& serverMap) {
        localPlayerId = playerId;
        map = serverMap;
        mapReceived = true;
        std::cout << "Received welcome! Player ID: " << playerId
                  << ", Map: " << map.width << "x" << map.height << std::endl;
    };

    network.onState = [this](const std::vector<GameObject*>& objects) {
        clearGameObjects();
        gameObjects = objects; // take ownership
    };

    network.onEvent = [](EventType event, uint32_t objectId) {
        switch (event) {
            case EventType::SHIP_ARRIVED:
                std::cout << "Ship arrived! (id=" << objectId << ")" << std::endl;
                break;
            case EventType::SHIP_DEPARTED:
                std::cout << "Ship departed! (id=" << objectId << ")" << std::endl;
                break;
            default:
                break;
        }
    };

    network.onMapUpdate = [this](int16_t gx, int16_t gy, CellType ct) {
        map.setCell(gx, gy, ct);
    };

    if (!network.connect(host, port)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return false;
    }

    network.sendJoin(playerName);
    running = true;
    return true;
}

void GameClient::run() {
    auto lastInputSend = std::chrono::steady_clock::now();

    while (running && !glfwWindowShouldClose(renderer.getWindow())) {
        // Poll network
        network.poll();
        if (!network.isConnected()) {
            std::cout << "Lost connection to server" << std::endl;
            running = false;
            break;
        }

        // Handle input
        input.update(renderer.getWindow());

        if (glfwGetKey(renderer.getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            running = false;
            break;
        }

        // Edit mode toggle via B key
        if (input.wasEditTogglePressed()) {
            toggleEditMode();
        }

        // Number keys for hotbar selection
        if (editMode && hotbar) {
            int numKey = input.getNumberKeyPressed();
            if (numKey >= 0) {
                hotbar->selectSlot(numKey);
            }
        }

        // Process UI input first (consumes clicks on UI elements)
        bool uiConsumed = uiManager.processInput(renderer.getWindow());

        // Update hotbar position on window resize
        if (hotbar) {
            hotbar->updatePosition(
                static_cast<float>(renderer.getWindowWidth()),
                static_cast<float>(renderer.getWindowHeight())
            );
        }
        // Update edit button position on resize
        if (editButton) {
            editButton->x = static_cast<float>(renderer.getWindowWidth()) - 80.0f;
        }

        // Edit mode: handle grid placement if UI didn't consume the click
        if (editMode && !uiConsumed && mapReceived) {
            if (input.wasLeftClickPressed()) {
                int gx, gy;
                screenToGrid(input.getMouseX(), input.getMouseY(), gx, gy);
                CellType selectedType = hotbar ? hotbar->getSelectedType() : CellType::WALL;
                network.sendCellEdit(static_cast<int16_t>(gx), static_cast<int16_t>(gy), selectedType);
            }
            if (input.wasRightClickPressed()) {
                int gx, gy;
                screenToGrid(input.getMouseX(), input.getMouseY(), gx, gy);
                network.sendCellEdit(static_cast<int16_t>(gx), static_cast<int16_t>(gy), CellType::EMPTY);
            }
        }

        // Cargo placement: when carrying, not in edit mode, left-click to place
        Player* carryCheckPlayer = findLocalPlayer();
        if (!editMode && !uiConsumed && carryCheckPlayer && carryCheckPlayer->isCarrying() && mapReceived) {
            if (input.wasLeftClickPressed()) {
                float worldX, worldY;
                screenToWorld(input.getMouseX(), input.getMouseY(), worldX, worldY);
                network.sendCargoPlace(worldX, worldY);
            }
        }

        // Accumulate interact presses between network ticks so they aren't lost
        if (input.wasInteractPressed()) {
            pendingInteract = true;
            std::cout << "[CLIENT] E pressed — queuing interact" << std::endl;
        }

        // Send input to server every tick (always send, even 0,0, so server clears movement)
        auto now = std::chrono::steady_clock::now();
        float timeSinceInput = std::chrono::duration<float>(now - lastInputSend).count();
        if (timeSinceInput >= SERVER_TICK_INTERVAL) {
            if (pendingInteract) {
                std::cout << "[CLIENT] Sending interact to server" << std::endl;
            }
            network.sendInput(input.getMoveX(), input.getMoveY(), pendingInteract);
            pendingInteract = false;
            lastInputSend = now;
        }

        // Update camera to follow local player
        Player* localPlayer = findLocalPlayer();
        if (localPlayer) {
            renderer.setCamera(
                localPlayer->x + localPlayer->width / 2.0f,
                localPlayer->y + localPlayer->height / 2.0f
            );
        }

        // --- Render ---
        renderer.beginFrame();

        if (mapReceived) {
            renderer.renderMap(map);
        }

        // Edit mode: draw cursor overlay
        if (editMode && mapReceived) {
            int gx, gy;
            screenToGrid(input.getMouseX(), input.getMouseY(), gx, gy);
            float wx = gx * CELL_SIZE;
            float wy = gy * CELL_SIZE;
            CellType selectedType = hotbar ? hotbar->getSelectedType() : CellType::WALL;
            glm::vec4 color = Renderer::getCellTypeColor(selectedType);
            renderer.drawWorldRect(wx, wy, CELL_SIZE, CELL_SIZE, color.r, color.g, color.b, 0.4f);
        }

        renderer.renderObjects(gameObjects);

        // Cargo ghost cursor: show translucent cargo at mouse position when carrying
        if (!editMode && localPlayer && localPlayer->isCarrying() && mapReceived) {
            // Find what cargo type we're carrying
            CargoType carryingType = CargoType::METAL;
            for (auto* obj : gameObjects) {
                if (obj->id == localPlayer->carryingCargoId && obj->type == GameObjectType::CARGO) {
                    carryingType = static_cast<const Cargo*>(obj)->cargoType;
                    break;
                }
            }

            float worldX, worldY;
            screenToWorld(input.getMouseX(), input.getMouseY(), worldX, worldY);

            // Check if target is valid
            int gx = static_cast<int>(std::floor(worldX / CELL_SIZE));
            int gy = static_cast<int>(std::floor(worldY / CELL_SIZE));
            CellType cell = map.getCell(gx, gy);

            float playerCX = localPlayer->x + localPlayer->width / 2.0f;
            float playerCY = localPlayer->y + localPlayer->height / 2.0f;
            float dx = worldX - playerCX;
            float dy = worldY - playerCY;
            float dist = std::sqrt(dx * dx + dy * dy);

            bool walkable = (cell == CellType::FLOOR || cell == CellType::STORAGE ||
                             cell == CellType::SPAWN_POINT || cell == CellType::AIRLOCK ||
                             cell == CellType::DOOR || cell == CellType::DOCKING_COLLAR);
            bool inRange = (dist <= INTERACTION_RANGE * 3.0f);
            bool valid = walkable && inRange;

            glm::vec4 cargoColor = Renderer::getCargoTypeColor(carryingType);
            float ghostSize = 14.0f;
            float ghostX = worldX - ghostSize / 2.0f;
            float ghostY = worldY - ghostSize / 2.0f;

            if (valid) {
                renderer.drawWorldRect(ghostX, ghostY, ghostSize, ghostSize,
                                       cargoColor.r, cargoColor.g, cargoColor.b, 0.5f);
                // Green outline
                float t = 2.0f;
                renderer.drawWorldRect(ghostX - t, ghostY - t, ghostSize + t * 2, t, 0.2f, 0.9f, 0.2f, 0.7f);
                renderer.drawWorldRect(ghostX - t, ghostY + ghostSize, ghostSize + t * 2, t, 0.2f, 0.9f, 0.2f, 0.7f);
                renderer.drawWorldRect(ghostX - t, ghostY, t, ghostSize, 0.2f, 0.9f, 0.2f, 0.7f);
                renderer.drawWorldRect(ghostX + ghostSize, ghostY, t, ghostSize, 0.2f, 0.9f, 0.2f, 0.7f);
            } else {
                renderer.drawWorldRect(ghostX, ghostY, ghostSize, ghostSize,
                                       0.9f, 0.2f, 0.2f, 0.4f);
                // Red outline
                float t = 2.0f;
                renderer.drawWorldRect(ghostX - t, ghostY - t, ghostSize + t * 2, t, 0.9f, 0.2f, 0.2f, 0.7f);
                renderer.drawWorldRect(ghostX - t, ghostY + ghostSize, ghostSize + t * 2, t, 0.9f, 0.2f, 0.2f, 0.7f);
                renderer.drawWorldRect(ghostX - t, ghostY, t, ghostSize, 0.9f, 0.2f, 0.2f, 0.7f);
                renderer.drawWorldRect(ghostX + ghostSize, ghostY, t, ghostSize, 0.9f, 0.2f, 0.2f, 0.7f);
            }
        }

        if (localPlayer) {
            renderer.renderHUD(localPlayer, gameObjects);
        }

        // Render UI on top of everything
        uiManager.render(renderer);

        // Draw "EDIT" label on the edit button in screen space
        if (editButton) {
            renderer.beginScreenSpace();
            // Small colored indicator inside button
            float indicatorSize = 8.0f;
            float ix = editButton->x + editButton->width / 2.0f - indicatorSize / 2.0f;
            float iy = editButton->y + editButton->height / 2.0f - indicatorSize / 2.0f;
            if (editMode) {
                renderer.drawUIRect(ix, iy, indicatorSize, indicatorSize, 0.2f, 0.9f, 0.3f);
            } else {
                renderer.drawUIRect(ix, iy, indicatorSize, indicatorSize, 0.7f, 0.7f, 0.7f);
            }
            renderer.endScreenSpace();
        }

        renderer.endFrame();
    }
}

void GameClient::shutdown() {
    network.disconnect();
    renderer.shutdown();
    clearGameObjects();
}

void GameClient::clearGameObjects() {
    for (auto* obj : gameObjects) delete obj;
    gameObjects.clear();
}

Player* GameClient::findLocalPlayer() {
    for (auto* obj : gameObjects) {
        if (obj->type == GameObjectType::PLAYER && obj->id == localPlayerId) {
            return static_cast<Player*>(obj);
        }
    }
    return nullptr;
}

void GameClient::toggleEditMode() {
    editMode = !editMode;
    if (editButton) editButton->toggled = editMode;
    if (hotbar) hotbar->visible = editMode;
    std::cout << "Edit mode: " << (editMode ? "ON" : "OFF") << std::endl;
}

void GameClient::screenToGrid(float screenX, float screenY, int& gridX, int& gridY) {
    float worldX = renderer.getCameraX() - renderer.getWindowWidth() / 2.0f + screenX;
    float worldY = renderer.getCameraY() - renderer.getWindowHeight() / 2.0f + screenY;
    gridX = static_cast<int>(std::floor(worldX / CELL_SIZE));
    gridY = static_cast<int>(std::floor(worldY / CELL_SIZE));
}

void GameClient::screenToWorld(float screenX, float screenY, float& worldX, float& worldY) {
    worldX = renderer.getCameraX() - renderer.getWindowWidth() / 2.0f + screenX;
    worldY = renderer.getCameraY() - renderer.getWindowHeight() / 2.0f + screenY;
}

} // namespace ssm
