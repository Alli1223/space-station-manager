#include "client/game_client.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <unordered_map>

namespace ssm {

// --- Client-side interpolation state ---
struct InterpState {
    float prevX, prevY;   // position at previous server tick
    float targetX, targetY; // position at latest server tick
};
static std::unordered_map<uint32_t, InterpState> interpMap;
static float interpAlpha = 0.0f; // 0..1 between prev and target tick

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
        // Before replacing, snapshot current positions as "previous" for interpolation
        // and new positions as "target"
        for (auto* newObj : objects) {
            auto it = interpMap.find(newObj->id);
            if (it != interpMap.end()) {
                // Move the old target to prev, set new target
                it->second.prevX = it->second.targetX;
                it->second.prevY = it->second.targetY;
                it->second.targetX = newObj->x;
                it->second.targetY = newObj->y;
            } else {
                // First time seeing this object — snap (no interpolation)
                interpMap[newObj->id] = { newObj->x, newObj->y, newObj->x, newObj->y };
            }
        }
        // Reset interpolation alpha — we just received a new tick
        interpAlpha = 0.0f;

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

    network.onStationState = [this](int32_t money) {
        stationMoney = money;
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
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (running && !glfwWindowShouldClose(renderer.getWindow())) {
        // Compute frame delta time
        auto frameNow = std::chrono::steady_clock::now();
        float frameDt = std::chrono::duration<float>(frameNow - lastFrameTime).count();
        if (frameDt > 0.1f) frameDt = 0.1f; // clamp to avoid huge jumps
        lastFrameTime = frameNow;
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

        // Tether toggle: when NOT carrying, NOT in edit mode, NOT in turret, left-click on on-ground cargo
        if (!editMode && !uiConsumed && carryCheckPlayer && !carryCheckPlayer->isCarrying() && !carryCheckPlayer->isInTurret() && mapReceived) {
            if (input.wasLeftClickPressed()) {
                float worldX, worldY;
                screenToWorld(input.getMouseX(), input.getMouseY(), worldX, worldY);
                // Hit-test all on-ground cargo (same AABB as tooltip hover)
                for (auto* obj : gameObjects) {
                    if (obj->type == GameObjectType::CARGO && obj->active) {
                        auto* cargo = static_cast<const Cargo*>(obj);
                        if (!cargo->isOnGround()) continue;
                        if (worldX >= cargo->x && worldX <= cargo->x + cargo->width &&
                            worldY >= cargo->y && worldY <= cargo->y + cargo->height) {
                            network.sendTetherToggle(cargo->id);
                            break;
                        }
                    }
                }
            }
        }

        // Accumulate interact presses between network ticks so they aren't lost
        if (input.wasInteractPressed()) {
            pendingInteract = true;
            std::cout << "[CLIENT] E pressed — queuing interact" << std::endl;
        }

        // Handle turret exit immediately when E is pressed
        Player* turretCheckPlayer = findLocalPlayer();
        if (pendingInteract && turretCheckPlayer && turretCheckPlayer->isInTurret()) {
            network.sendTurretExit();
            pendingInteract = false;
            std::cout << "[CLIENT] Exiting turret" << std::endl;
        }

        // Send input to server every tick
        auto now = std::chrono::steady_clock::now();
        float timeSinceInput = std::chrono::duration<float>(now - lastInputSend).count();
        if (timeSinceInput >= SERVER_TICK_INTERVAL) {
            if (turretCheckPlayer && turretCheckPlayer->isInTurret()) {
                // Turret mode: send aim angle and firing state
                float worldMX, worldMY;
                screenToWorld(input.getMouseX(), input.getMouseY(), worldMX, worldMY);
                float pcx = turretCheckPlayer->x + turretCheckPlayer->width / 2.0f;
                float pcy = turretCheckPlayer->y + turretCheckPlayer->height / 2.0f;
                float aimAngle = std::atan2(worldMY - pcy, worldMX - pcx);
                network.sendTurretAim(aimAngle, input.isLeftMouseDown());
            } else {
                if (pendingInteract) {
                    std::cout << "[CLIENT] Sending interact to server" << std::endl;
                }
                network.sendInput(input.getMoveX(), input.getMoveY(), pendingInteract, input.isSprinting());
                pendingInteract = false;
            }
            lastInputSend = now;
        }

        // Advance interpolation alpha (0→1 over one server tick interval)
        interpAlpha += frameDt / SERVER_TICK_INTERVAL;
        if (interpAlpha > 1.0f) interpAlpha = 1.0f;

        // Apply interpolated positions to all dynamic game objects
        for (auto* obj : gameObjects) {
            if (obj->type == GameObjectType::WALL || obj->type == GameObjectType::FLOOR) continue;
            auto it = interpMap.find(obj->id);
            if (it != interpMap.end()) {
                const auto& s = it->second;
                obj->x = s.prevX + (s.targetX - s.prevX) * interpAlpha;
                obj->y = s.prevY + (s.targetY - s.prevY) * interpAlpha;
            }
        }

        // Clean up stale entries from interpMap (objects that no longer exist)
        if (interpMap.size() > gameObjects.size() * 2) {
            std::unordered_map<uint32_t, InterpState> cleaned;
            for (auto* obj : gameObjects) {
                auto it = interpMap.find(obj->id);
                if (it != interpMap.end()) cleaned[obj->id] = it->second;
            }
            interpMap = std::move(cleaned);
        }

        // Update camera to smoothly follow local player
        Player* localPlayer = findLocalPlayer();
        if (localPlayer) {
            float targetX = localPlayer->x + localPlayer->width / 2.0f;
            float targetY = localPlayer->y + localPlayer->height / 2.0f;

            if (!cameraInitialized) {
                // Snap on first frame
                cameraPosX = targetX;
                cameraPosY = targetY;
                cameraInitialized = true;
            } else {
                // Smooth exponential lerp — higher = snappier, lower = smoother
                constexpr float CAMERA_SMOOTHING = 6.0f;
                float lerpFactor = 1.0f - std::exp(-CAMERA_SMOOTHING * frameDt);
                cameraPosX += (targetX - cameraPosX) * lerpFactor;
                cameraPosY += (targetY - cameraPosY) * lerpFactor;
            }

            renderer.setCamera(cameraPosX, cameraPosY);

            // Smooth zoom transition for turret mode
            float targetZoom = localPlayer->isInTurret() ? 0.4f : 1.0f;
            float currentZoom = renderer.getZoom();
            constexpr float ZOOM_SMOOTHING = 4.0f;
            float zoomLerp = 1.0f - std::exp(-ZOOM_SMOOTHING * frameDt);
            float newZoom = currentZoom + (targetZoom - currentZoom) * zoomLerp;
            renderer.setZoom(newZoom);
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
        renderer.renderTetherRopes(gameObjects);

        // Hover tooltip: check what the mouse is over
        std::string tooltipTitle;
        std::string tooltipDesc;
        CargoType hoveredCargoType = CargoType::NONE;
        if (!editMode && mapReceived) {
            float hoverWorldX, hoverWorldY;
            screenToWorld(input.getMouseX(), input.getMouseY(), hoverWorldX, hoverWorldY);
            for (auto* obj : gameObjects) {
                if (!obj->active) continue;
                // AABB hit test
                if (hoverWorldX < obj->x || hoverWorldX > obj->x + obj->width ||
                    hoverWorldY < obj->y || hoverWorldY > obj->y + obj->height) continue;

                if (obj->type == GameObjectType::CARGO) {
                    auto* cargo = static_cast<const Cargo*>(obj);
                    if (!cargo->isOnGround()) continue;
                    hoveredCargoType = cargo->cargoType;
                    switch (cargo->cargoType) {
                        case CargoType::FUEL:     tooltipTitle = "Fuel"; tooltipDesc = "Powers ship engines"; break;
                        case CargoType::FOOD:     tooltipTitle = "Food"; tooltipDesc = "Feeds ship crew"; break;
                        case CargoType::METAL:    tooltipTitle = "Metal"; tooltipDesc = "Used for turret ammo and building"; break;
                        case CargoType::ORE:      tooltipTitle = "Ore"; tooltipDesc = "Raw material, refine for credits"; break;
                        case CargoType::CRYSTALS: tooltipTitle = "Crystals"; tooltipDesc = "Valuable trade commodity"; break;
                        case CargoType::PLASMA:   tooltipTitle = "Plasma"; tooltipDesc = "High-energy resource"; break;
                        default: break;
                    }
                    break;
                } else if (obj->type == GameObjectType::SHIP) {
                    auto* ship = static_cast<const Ship*>(obj);
                    switch (ship->shipClass) {
                        case ShipClass::SMALL:  tooltipTitle = "Small Trader"; break;
                        case ShipClass::MEDIUM: tooltipTitle = "Medium Freighter"; break;
                        case ShipClass::LARGE:  tooltipTitle = "Large Hauler"; break;
                    }
                    switch (ship->state) {
                        case ShipState::APPROACHING:       tooltipDesc = "Approaching station"; break;
                        case ShipState::DOCKING:           tooltipDesc = "Landing..."; break;
                        case ShipState::UNLOADING:         tooltipDesc = "Unloading cargo"; break;
                        case ShipState::WAITING_RESUPPLY:  tooltipDesc = "Needs fuel and food"; break;
                        case ShipState::DEPARTING:         tooltipDesc = "Departing"; break;
                        default: break;
                    }
                    break;
                } else if (obj->type == GameObjectType::TURRET) {
                    auto* turret = static_cast<const Turret*>(obj);
                    if (turret->turretType == TurretType::ENERGY) {
                        tooltipTitle = "Energy Turret";
                        tooltipDesc = "High damage, short range, unlimited ammo";
                    } else {
                        tooltipTitle = "Kinetic Turret";
                        tooltipDesc = "Fast fire, long range, needs Metal ammo";
                    }
                    if (turret->operatorId != 0) tooltipDesc += " [Manned]";
                    break;
                } else if (obj->type == GameObjectType::ENEMY_SHIP) {
                    auto* enemy = static_cast<const EnemyShip*>(obj);
                    tooltipTitle = "Enemy Ship";
                    int hpPct = static_cast<int>(enemy->health / enemy->maxHealth * 100.0f);
                    tooltipDesc = "Hostile - HP: " + std::to_string(hpPct) + "%";
                    break;
                }
            }
        }

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
            renderer.renderHUD(localPlayer, gameObjects, stationMoney);
        }

        // Render tooltip in screen space if hovering over something
        if (!tooltipTitle.empty()) {
            renderer.beginScreenSpace();
            // Keep the small colored cargo indicator near the mouse
            if (hoveredCargoType != CargoType::NONE) {
                renderer.renderCargoTooltip(input.getMouseX(), input.getMouseY(), hoveredCargoType);
            }
            // Text tooltip in bottom-right corner
            renderer.renderTooltip(tooltipTitle, tooltipDesc);
            renderer.endScreenSpace();
        }

        // Render UI on top of everything
        uiManager.render(renderer);

        // Draw "EDIT" label on the edit button in screen space
        if (editButton) {
            renderer.beginScreenSpace();
            float textScale = 1.5f;
            float textW = renderer.measureText("EDIT", textScale);
            float textH = renderer.getTextHeight(textScale);
            float tx = editButton->x + (editButton->width - textW) / 2.0f;
            float ty = editButton->y + (editButton->height - textH) / 2.0f;
            if (editMode) {
                renderer.drawText(tx, ty, "EDIT", 0.2f, 0.9f, 0.3f, 1.0f, textScale);
            } else {
                renderer.drawText(tx, ty, "EDIT", 0.8f, 0.8f, 0.8f, 1.0f, textScale);
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
    float z = renderer.getZoom();
    float worldX = renderer.getCameraX() + (screenX - renderer.getWindowWidth() / 2.0f) / z;
    float worldY = renderer.getCameraY() + (screenY - renderer.getWindowHeight() / 2.0f) / z;
    gridX = static_cast<int>(std::floor(worldX / CELL_SIZE));
    gridY = static_cast<int>(std::floor(worldY / CELL_SIZE));
}

void GameClient::screenToWorld(float screenX, float screenY, float& worldX, float& worldY) {
    float z = renderer.getZoom();
    worldX = renderer.getCameraX() + (screenX - renderer.getWindowWidth() / 2.0f) / z;
    worldY = renderer.getCameraY() + (screenY - renderer.getWindowHeight() / 2.0f) / z;
}

} // namespace ssm
