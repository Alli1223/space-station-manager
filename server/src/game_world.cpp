#include "server/game_world.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace ssm {

GameWorld::~GameWorld() {
    for (auto* obj : objects) delete obj;
    objects.clear();
}

bool GameWorld::init(const std::string& mapFile) {
    if (!map.loadFromFile(mapFile)) {
        std::cerr << "Failed to load map: " << mapFile << std::endl;
        return false;
    }

    buildMapObjects();

    // Initialize ship manager with docking collars
    auto collars = findObjectsOfType<DockingCollar>();
    shipManager.init(collars, [this]() { return generateId(); });

    // Wire economy callback
    shipManager.onMoneyChange = [this](int32_t delta, bool happy) {
        stationMoney += delta;
        std::cout << "[SERVER] Ship departed (" << (happy ? "happy" : "angry")
                  << "), money delta: " << (delta >= 0 ? "+" : "") << delta
                  << ", total: " << stationMoney << std::endl;
    };

    std::cout << "World initialized with " << objects.size() << " objects, starting money: "
              << stationMoney << std::endl;
    return true;
}

void GameWorld::buildMapObjects() {
    // First pass: create doors and remember positions for terminal linking
    struct DoorInfo {
        Door* door;
        int gridX, gridY;
    };
    std::vector<DoorInfo> doorInfos;

    for (int ly = map.originY; ly < map.originY + map.height; ly++) {
        for (int lx = map.originX; lx < map.originX + map.width; lx++) {
            CellType cell = map.getCell(lx, ly);
            float wx = lx * CELL_SIZE;
            float wy = ly * CELL_SIZE;

            switch (cell) {
                case CellType::WALL: {
                    auto* wall = new Wall(generateId(), wx, wy);
                    objects.push_back(wall);
                    break;
                }
                case CellType::FLOOR:
                case CellType::SPAWN_POINT:
                case CellType::AIRLOCK:
                case CellType::STORAGE: {
                    auto* floor = new Floor(generateId(), wx, wy);
                    objects.push_back(floor);
                    break;
                }
                case CellType::DOOR: {
                    // Create a floor underneath and a door on top
                    auto* floor = new Floor(generateId(), wx, wy);
                    objects.push_back(floor);
                    auto* door = new Door(generateId(), wx, wy);
                    objects.push_back(door);
                    doorInfos.push_back({door, lx, ly});
                    break;
                }
                case CellType::DOCKING_COLLAR: {
                    auto* floor = new Floor(generateId(), wx, wy);
                    objects.push_back(floor);
                    auto* collar = new DockingCollar(generateId(), wx, wy);
                    objects.push_back(collar);
                    break;
                }
                case CellType::TERMINAL: {
                    auto* floor = new Floor(generateId(), wx, wy);
                    objects.push_back(floor);
                    // Terminal created in second pass after all doors exist
                    break;
                }
                default:
                    break;
            }
        }
    }

    // Second pass: create terminals and link them to nearest door
    for (int ly = map.originY; ly < map.originY + map.height; ly++) {
        for (int lx = map.originX; lx < map.originX + map.width; lx++) {
            if (map.getCell(lx, ly) == CellType::TERMINAL) {
                // Find the nearest door
                uint32_t nearestDoorId = 0;
                float nearestDist = 999999.0f;
                for (auto& di : doorInfos) {
                    float dx = static_cast<float>(lx - di.gridX);
                    float dy = static_cast<float>(ly - di.gridY);
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearestDoorId = di.door->id;
                    }
                }

                float wx = lx * CELL_SIZE;
                float wy = ly * CELL_SIZE;
                auto* terminal = new Terminal(generateId(), wx, wy, nearestDoorId);
                objects.push_back(terminal);
            }
        }
    }

    // Third pass: link docking collars to nearest door (airlock)
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::DOCKING_COLLAR && obj->active) {
            auto* collar = static_cast<DockingCollar*>(obj);
            int collarGX = static_cast<int>(std::floor(collar->x / CELL_SIZE));
            int collarGY = static_cast<int>(std::floor(collar->y / CELL_SIZE));
            uint32_t nearestDoorId = 0;
            float nearestDist = 999999.0f;
            for (auto& di : doorInfos) {
                float dx = static_cast<float>(collarGX - di.gridX);
                float dy = static_cast<float>(collarGY - di.gridY);
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearestDoorId = di.door->id;
                }
            }
            collar->linkedDoorId = nearestDoorId;
        }
    }

    // Place initial fuel and food cargo in storage areas
    auto storagePositions = map.findCells(CellType::STORAGE);
    int fuelCount = 0, foodCount = 0;
    for (auto& [sx, sy] : storagePositions) {
        float wx = sx * CELL_SIZE + 4.0f;
        float wy = sy * CELL_SIZE + 4.0f;
        if (fuelCount < 4) {
            auto* cargo = new Cargo(generateId(), wx, wy, CargoType::FUEL, 1);
            objects.push_back(cargo);
            fuelCount++;
        } else if (foodCount < 4) {
            auto* cargo = new Cargo(generateId(), wx, wy, CargoType::FOOD, 1);
            objects.push_back(cargo);
            foodCount++;
        }
    }
}

void GameWorld::update(float dt) {
    // Update player movement with collision
    auto doors = getAllDoors();
    auto dockedShips = getDockedShips();
    for (auto& [clientIdx, player] : playersByClientIndex) {
        if (!player->active) continue;

        // Apply pending input
        if (player->dx != 0.0f || player->dy != 0.0f) {
            collision.moveWithCollision(map, doors, dockedShips, *player, player->dx, player->dy, dt);
        }

        // Handle interaction request
        if (player->interacting) {
            handleInteraction(player);
            player->interacting = false;
        }

        // If carrying cargo, update cargo position to follow player
        if (player->isCarrying()) {
            for (auto* obj : objects) {
                if (obj->id == player->carryingCargoId && obj->active) {
                    obj->x = player->x;
                    obj->y = player->y - 10.0f; // float above player
                    break;
                }
            }
        }
    }

    // Update ships
    shipManager.update(dt, objects);

    // Clean up inactive objects (but keep them for a bit to let clients see them deactivate)
}

void GameWorld::onPlayerJoin(uint32_t clientIndex, const std::string& name) {
    // Find spawn point
    auto spawns = map.findCells(CellType::SPAWN_POINT);
    float spawnX = 5 * CELL_SIZE, spawnY = 5 * CELL_SIZE; // fallback
    if (!spawns.empty()) {
        // Offset each player slightly so they don't stack
        int spawnIdx = static_cast<int>(playersByClientIndex.size()) % static_cast<int>(spawns.size());
        spawnX = spawns[spawnIdx].first * CELL_SIZE + (CELL_SIZE - PLAYER_SIZE) / 2.0f;
        spawnY = spawns[spawnIdx].second * CELL_SIZE + (CELL_SIZE - PLAYER_SIZE) / 2.0f;
    }

    auto* player = new Player(generateId(), spawnX, spawnY, name);
    player->colorIndex = static_cast<uint8_t>(playersByClientIndex.size() % 8);
    objects.push_back(player);
    playersByClientIndex[clientIndex] = player;

    std::cout << "Player '" << name << "' (id=" << player->id << ", color=" << static_cast<int>(player->colorIndex)
              << ") joined at (" << spawnX << ", " << spawnY << ")" << std::endl;
}

void GameWorld::onPlayerInput(uint32_t clientIndex, float dx, float dy, bool interact) {
    Player* player = findPlayer(clientIndex);
    if (!player) return;

    // Normalize input
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
        dx /= len;
        dy /= len;
    }

    player->dx = dx;
    player->dy = dy;
    if (interact) {
        player->interacting = true;
        std::cout << "[SERVER] Received interact from client " << clientIndex
                  << " ('" << player->name << "')" << std::endl;
    }
}

void GameWorld::onPlayerDisconnect(uint32_t clientIndex) {
    Player* player = findPlayer(clientIndex);
    if (player) {
        // Drop any carried cargo
        if (player->isCarrying()) {
            for (auto* obj : objects) {
                if (obj->id == player->carryingCargoId) {
                    auto* cargo = static_cast<Cargo*>(obj);
                    cargo->carriedByPlayerId = 0;
                    cargo->x = player->x;
                    cargo->y = player->y;
                    break;
                }
            }
        }
        player->active = false;
        playersByClientIndex.erase(clientIndex);
        std::cout << "Player '" << player->name << "' disconnected" << std::endl;
    }
}

ByteBuffer GameWorld::buildStateSnapshot() {
    return buildStateMessage(objects);
}

ByteBuffer GameWorld::buildWelcome(uint32_t playerId) {
    auto mapData = map.getRawData();
    return buildWelcomeMessage(playerId, mapData, map.width, map.height, map.originX, map.originY);
}

Player* GameWorld::findPlayer(uint32_t clientIndex) {
    auto it = playersByClientIndex.find(clientIndex);
    if (it != playersByClientIndex.end()) return it->second;
    return nullptr;
}

void GameWorld::handleInteraction(Player* player) {
    float centerX = player->x + player->width / 2.0f;
    float centerY = player->y + player->height / 2.0f;

    std::cout << "[SERVER] Interact received from '" << player->name
              << "' at (" << centerX << ", " << centerY << ")"
              << (player->isCarrying() ? " [carrying cargo]" : "") << std::endl;

    // If carrying cargo, try to drop it or load it onto a ship
    if (player->isCarrying()) {
        handleCargoDropOrLoad(player);
        return;
    }

    // Find distances to nearest terminal and cargo to pick the closer one
    Terminal* terminal = findNearestObject<Terminal>(centerX, centerY, INTERACTION_RANGE);
    Cargo* cargo = findNearestObject<Cargo>(centerX, centerY, INTERACTION_RANGE);

    // Filter out cargo that's already being carried by someone
    if (cargo && !cargo->isOnGround()) cargo = nullptr;

    float termDist = 999999.0f;
    float cargoDist = 999999.0f;

    if (terminal) {
        float dx = centerX - (terminal->x + terminal->width / 2.0f);
        float dy = centerY - (terminal->y + terminal->height / 2.0f);
        termDist = std::sqrt(dx * dx + dy * dy);
    }
    if (cargo) {
        float dx = centerX - (cargo->x + cargo->width / 2.0f);
        float dy = centerY - (cargo->y + cargo->height / 2.0f);
        cargoDist = std::sqrt(dx * dx + dy * dy);
    }

    // Cargo pickup takes priority when closer (it's the more common action)
    if (cargo && cargoDist <= termDist) {
        cargo->carriedByPlayerId = player->id;
        player->carryingCargoId = cargo->id;
        std::cout << "[SERVER] Player '" << player->name << "' picked up cargo " << cargo->id
                  << " (dist=" << cargoDist << ")" << std::endl;
        return;
    }

    if (terminal) {
        handleTerminalUse(player, terminal);
        return;
    }

    // Try to withdraw cargo from a docked ship's virtual cargo hold
    if (handleShipWithdraw(player)) {
        return;
    }

    std::cout << "[SERVER] No interactable found near '" << player->name << "'" << std::endl;
}

void GameWorld::handleTerminalUse(Player* player, Terminal* terminal) {
    // Find the linked door and toggle it
    for (auto* obj : objects) {
        if (obj->id == terminal->linkedDoorId && obj->type == GameObjectType::DOOR) {
            auto* door = static_cast<Door*>(obj);
            door->toggle();
            std::cout << "[SERVER] Player '" << player->name << "' toggled door " << door->id
                      << " via terminal " << terminal->id
                      << " to " << (door->isSolid() ? "CLOSED" : "OPEN") << std::endl;
            break;
        }
    }
}

void GameWorld::handleCargoPickup(Player* player) {
    float centerX = player->x + player->width / 2.0f;
    float centerY = player->y + player->height / 2.0f;

    // Find closest on-ground cargo
    Cargo* nearest = nullptr;
    float nearestDist = INTERACTION_RANGE;
    for (auto* obj : objects) {
        if (!obj->active || obj->type != GameObjectType::CARGO) continue;
        auto* cargo = static_cast<Cargo*>(obj);
        if (!cargo->isOnGround()) continue;
        float cx = cargo->x + cargo->width / 2.0f;
        float cy = cargo->y + cargo->height / 2.0f;
        float dx = centerX - cx;
        float dy = centerY - cy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = cargo;
        }
    }

    if (nearest) {
        nearest->carriedByPlayerId = player->id;
        player->carryingCargoId = nearest->id;
        std::cout << "[SERVER] Player '" << player->name << "' picked up cargo " << nearest->id
                  << " (dist=" << nearestDist << ")" << std::endl;
    }
}

bool GameWorld::handleShipWithdraw(Player* player) {
    float centerX = player->x + player->width / 2.0f;
    float centerY = player->y + player->height / 2.0f;

    // Find the nearest docked ship with cargo to unload
    for (auto* obj : objects) {
        if (obj->type != GameObjectType::SHIP || !obj->active) continue;
        auto* ship = static_cast<Ship*>(obj);

        // Ship must be in a state where cargo can be withdrawn
        if (ship->state != ShipState::UNLOADING && ship->state != ShipState::WAITING_RESUPPLY) continue;

        // Check distance to ship
        float shipCenterX = ship->x + ship->width / 2.0f;
        float shipCenterY = ship->y + ship->height / 2.0f;
        float dx = centerX - shipCenterX;
        float dy = centerY - shipCenterY;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist >= INTERACTION_RANGE + ship->width / 2.0f) continue;

        // Withdraw one item: metal first, then wood
        CargoType withdrawType;
        if (ship->metalToUnload > 0) {
            ship->metalToUnload--;
            withdrawType = CargoType::METAL;
        } else if (ship->woodToUnload > 0) {
            ship->woodToUnload--;
            withdrawType = CargoType::WOOD;
        } else {
            continue; // this ship has nothing to give
        }

        // Create a cargo object already carried by the player
        auto* cargo = new Cargo(generateId(), player->x, player->y - 10.0f, withdrawType, 1);
        cargo->carriedByPlayerId = player->id;
        player->carryingCargoId = cargo->id;
        objects.push_back(cargo);

        std::cout << "[SERVER] Player '" << player->name << "' withdrew "
                  << (withdrawType == CargoType::METAL ? "metal" : "wood")
                  << " from ship " << ship->id << std::endl;
        return true;
    }

    return false;
}

void GameWorld::handleCargoDropOrLoad(Player* player) {
    Cargo* cargo = nullptr;
    for (auto* obj : objects) {
        if (obj->id == player->carryingCargoId && obj->active) {
            cargo = static_cast<Cargo*>(obj);
            break;
        }
    }
    if (!cargo) {
        player->carryingCargoId = 0;
        return;
    }

    float centerX = player->x + player->width / 2.0f;
    float centerY = player->y + player->height / 2.0f;

    // Check if near a docked ship - try to load cargo
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::SHIP && obj->active) {
            auto* ship = static_cast<Ship*>(obj);
            if (ship->state == ShipState::WAITING_RESUPPLY) {
                float shipCenterX = ship->x + ship->width / 2.0f;
                float shipCenterY = ship->y + ship->height / 2.0f;
                float dx = centerX - shipCenterX;
                float dy = centerY - shipCenterY;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < INTERACTION_RANGE + ship->width / 2.0f) {
                    if (shipManager.loadCargoOntoShip(ship, cargo)) {
                        player->carryingCargoId = 0;
                        std::cout << "[SERVER] Player '" << player->name << "' loaded cargo onto ship " << ship->id << std::endl;
                        return;
                    }
                }
            }
        }
    }

    // Otherwise just drop cargo on ground
    cargo->carriedByPlayerId = 0;
    cargo->x = player->x;
    cargo->y = player->y + player->height;
    player->carryingCargoId = 0;
    std::cout << "[SERVER] Player '" << player->name << "' dropped cargo" << std::endl;
}

std::vector<Door*> GameWorld::getAllDoors() const {
    std::vector<Door*> doors;
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::DOOR && obj->active) {
            doors.push_back(static_cast<Door*>(obj));
        }
    }
    return doors;
}

std::vector<Ship*> GameWorld::getDockedShips() const {
    std::vector<Ship*> ships;
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::SHIP && obj->active) {
            auto* ship = static_cast<Ship*>(obj);
            if (ship->state == ShipState::UNLOADING || ship->state == ShipState::WAITING_RESUPPLY) {
                ships.push_back(ship);
            }
        }
    }
    return ships;
}

template<typename T>
T* GameWorld::findNearestObject(float x, float y, float range) const {
    T* nearest = nullptr;
    float nearestDist = range;

    for (auto* obj : objects) {
        if (!obj->active) continue;
        auto* typed = dynamic_cast<T*>(obj);
        if (!typed) continue;

        float cx = obj->x + obj->width / 2.0f;
        float cy = obj->y + obj->height / 2.0f;
        float dx = x - cx;
        float dy = y - cy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = typed;
        }
    }
    return nearest;
}

template<typename T>
std::vector<T*> GameWorld::findObjectsOfType() const {
    std::vector<T*> result;
    for (auto* obj : objects) {
        auto* typed = dynamic_cast<T*>(obj);
        if (typed && typed->active) {
            result.push_back(typed);
        }
    }
    return result;
}

// --- Cargo Placement (click-to-place) ---

void GameWorld::onCargoPlace(uint32_t clientIndex, float targetX, float targetY) {
    Player* player = findPlayer(clientIndex);
    if (!player || !player->isCarrying()) return;

    // Find the carried cargo
    Cargo* cargo = nullptr;
    for (auto* obj : objects) {
        if (obj->id == player->carryingCargoId && obj->active) {
            cargo = static_cast<Cargo*>(obj);
            break;
        }
    }
    if (!cargo) {
        player->carryingCargoId = 0;
        return;
    }

    float playerCenterX = player->x + player->width / 2.0f;
    float playerCenterY = player->y + player->height / 2.0f;

    // Check range — target must be within INTERACTION_RANGE * 3 of player
    float dx = targetX - playerCenterX;
    float dy = targetY - playerCenterY;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > INTERACTION_RANGE * 3.0f) return;

    // Check if target is near a docked ship — try to load cargo onto it
    for (auto* obj : objects) {
        if (obj->type == GameObjectType::SHIP && obj->active) {
            auto* ship = static_cast<Ship*>(obj);
            if (ship->state == ShipState::WAITING_RESUPPLY) {
                // Check if click target is within the ship bounds
                if (targetX >= ship->x && targetX < ship->x + ship->width &&
                    targetY >= ship->y && targetY < ship->y + ship->height) {
                    if (shipManager.loadCargoOntoShip(ship, cargo)) {
                        player->carryingCargoId = 0;
                        std::cout << "Player '" << player->name << "' loaded cargo onto ship "
                                  << ship->id << " via click" << std::endl;
                        return;
                    }
                }
            }
        }
    }

    // Otherwise place on ground — validate target cell is walkable
    int gridX = static_cast<int>(std::floor(targetX / CELL_SIZE));
    int gridY = static_cast<int>(std::floor(targetY / CELL_SIZE));
    CellType cell = map.getCell(gridX, gridY);

    if (cell != CellType::FLOOR && cell != CellType::STORAGE &&
        cell != CellType::SPAWN_POINT && cell != CellType::AIRLOCK &&
        cell != CellType::DOOR) {
        return; // can't place on walls, empty, terminals, etc.
    }

    // Place cargo at target position
    cargo->carriedByPlayerId = 0;
    cargo->x = targetX - cargo->width / 2.0f;
    cargo->y = targetY - cargo->height / 2.0f;
    player->carryingCargoId = 0;

    std::cout << "Player '" << player->name << "' placed cargo at ("
              << targetX << ", " << targetY << ")" << std::endl;
}

// --- Edit Mode ---

bool GameWorld::onCellEdit(uint32_t clientIndex, int16_t gridX, int16_t gridY, CellType cellType) {
    if (!validateEdit(gridX, gridY, cellType)) {
        return false;
    }

    CellType oldType = map.getCell(gridX, gridY);
    if (oldType == cellType) return false; // no change

    map.setCell(gridX, gridY, cellType);
    rebuildCellObjects(gridX, gridY, oldType, cellType);

    std::cout << "Cell edit at (" << gridX << ", " << gridY << "): "
              << static_cast<int>(oldType) << " -> " << static_cast<int>(cellType) << std::endl;
    return true;
}

bool GameWorld::validateEdit(int16_t gridX, int16_t gridY, CellType newType) {
    float wx = gridX * CELL_SIZE;
    float wy = gridY * CELL_SIZE;

    // 1. Can't place a wall on a cell occupied by a player
    if (newType == CellType::WALL) {
        for (auto& [idx, player] : playersByClientIndex) {
            if (!player->active) continue;
            // Check if player overlaps this grid cell
            if (player->x + player->width > wx && player->x < wx + CELL_SIZE &&
                player->y + player->height > wy && player->y < wy + CELL_SIZE) {
                return false;
            }
        }
    }

    // 2. Docking collar must be on station edge:
    //    at least one EMPTY neighbor AND at least one non-EMPTY neighbor
    if (newType == CellType::DOCKING_COLLAR) {
        bool hasEmptyNeighbor = false;
        bool hasNonEmptyNeighbor = false;
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            CellType neighbor = map.getCell(gridX + dx[i], gridY + dy[i]);
            if (neighbor == CellType::EMPTY)
                hasEmptyNeighbor = true;
            else
                hasNonEmptyNeighbor = true;
        }
        if (!hasEmptyNeighbor || !hasNonEmptyNeighbor) {
            return false;
        }
    }

    // 3. Doors need walls on two opposite sides (horizontal or vertical corridor)
    if (newType == CellType::DOOR) {
        CellType left = map.getCell(gridX - 1, gridY);
        CellType right = map.getCell(gridX + 1, gridY);
        CellType up = map.getCell(gridX, gridY - 1);
        CellType down = map.getCell(gridX, gridY + 1);
        bool horizontalCorridor = (left == CellType::WALL && right == CellType::WALL);
        bool verticalCorridor = (up == CellType::WALL && down == CellType::WALL);
        if (!horizontalCorridor && !verticalCorridor) {
            return false;
        }
    }

    // 4. Can't remove collar with docked ship
    CellType oldType = map.getCell(gridX, gridY);
    if (oldType == CellType::DOCKING_COLLAR && newType != CellType::DOCKING_COLLAR) {
        for (auto* obj : objects) {
            if (obj->type == GameObjectType::DOCKING_COLLAR && obj->active) {
                auto* collar = static_cast<DockingCollar*>(obj);
                int collarGX = static_cast<int>(std::floor(collar->x / CELL_SIZE));
                int collarGY = static_cast<int>(std::floor(collar->y / CELL_SIZE));
                if (collarGX == gridX && collarGY == gridY && collar->hasShip()) {
                    return false;
                }
            }
        }
    }

    return true;
}

void GameWorld::removeObjectsAtGrid(int16_t gridX, int16_t gridY) {
    float wx = gridX * CELL_SIZE;
    float wy = gridY * CELL_SIZE;
    float tolerance = 1.0f;

    for (auto* obj : objects) {
        if (!obj->active) continue;
        // Only remove static map objects (walls, floors, doors, terminals, docking collars)
        if (obj->type == GameObjectType::WALL ||
            obj->type == GameObjectType::FLOOR ||
            obj->type == GameObjectType::DOOR ||
            obj->type == GameObjectType::TERMINAL ||
            obj->type == GameObjectType::DOCKING_COLLAR) {

            if (std::abs(obj->x - wx) < tolerance && std::abs(obj->y - wy) < tolerance) {
                // If it's a docking collar, remove from ship manager
                if (obj->type == GameObjectType::DOCKING_COLLAR) {
                    shipManager.removeCollar(static_cast<DockingCollar*>(obj));
                }
                obj->active = false;
            }
        }
    }
}

void GameWorld::rebuildCellObjects(int16_t gridX, int16_t gridY, CellType oldType, CellType newType) {
    // Remove existing objects at this grid position
    removeObjectsAtGrid(gridX, gridY);

    float wx = gridX * CELL_SIZE;
    float wy = gridY * CELL_SIZE;

    // Create new objects for the new cell type
    switch (newType) {
        case CellType::WALL: {
            auto* wall = new Wall(generateId(), wx, wy);
            objects.push_back(wall);
            break;
        }
        case CellType::FLOOR:
        case CellType::SPAWN_POINT:
        case CellType::AIRLOCK:
        case CellType::STORAGE: {
            auto* floor = new Floor(generateId(), wx, wy);
            objects.push_back(floor);
            break;
        }
        case CellType::DOOR: {
            auto* floor = new Floor(generateId(), wx, wy);
            objects.push_back(floor);
            auto* door = new Door(generateId(), wx, wy);
            objects.push_back(door);
            break;
        }
        case CellType::TERMINAL: {
            auto* floor = new Floor(generateId(), wx, wy);
            objects.push_back(floor);
            // Find nearest door to link
            uint32_t nearestDoorId = 0;
            float nearestDist = 999999.0f;
            for (auto* obj : objects) {
                if (obj->type == GameObjectType::DOOR && obj->active) {
                    float dx = obj->x - wx;
                    float dy = obj->y - wy;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearestDoorId = obj->id;
                    }
                }
            }
            auto* terminal = new Terminal(generateId(), wx, wy, nearestDoorId);
            objects.push_back(terminal);
            break;
        }
        case CellType::DOCKING_COLLAR: {
            auto* floor = new Floor(generateId(), wx, wy);
            objects.push_back(floor);
            auto* collar = new DockingCollar(generateId(), wx, wy);
            objects.push_back(collar);
            shipManager.addCollar(collar);
            break;
        }
        case CellType::EMPTY:
            // No objects needed
            break;
        default:
            break;
    }
}

// Explicit template instantiations
template Terminal* GameWorld::findNearestObject<Terminal>(float, float, float) const;
template Cargo* GameWorld::findNearestObject<Cargo>(float, float, float) const;
template DockingCollar* GameWorld::findNearestObject<DockingCollar>(float, float, float) const;
template Ship* GameWorld::findNearestObject<Ship>(float, float, float) const;
template std::vector<DockingCollar*> GameWorld::findObjectsOfType<DockingCollar>() const;
template std::vector<Door*> GameWorld::findObjectsOfType<Door>() const;

} // namespace ssm
