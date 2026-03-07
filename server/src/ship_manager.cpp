#include "server/ship_manager.h"
#include <iostream>
#include <cmath>

namespace ssm {

void ShipManager::init(const std::vector<DockingCollar*>& collars, IdGenerator idGen) {
    dockingCollars = collars;
    generateId = idGen;
    spawnTimer = 10.0f; // first ship arrives after 10 seconds
}

void ShipManager::update(float dt, std::vector<GameObject*>& allObjects) {
    spawnTimer -= dt;
    if (spawnTimer <= 0.0f) {
        spawnShip(allObjects);
        spawnTimer = spawnInterval;
    }

    // Update all ships
    for (auto* obj : allObjects) {
        if (obj->type == GameObjectType::SHIP && obj->active) {
            updateShip(static_cast<Ship*>(obj), dt, allObjects);
        }
    }
}

ShipClass ShipManager::rollShipClass() {
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);
    if (roll < 50) return ShipClass::SMALL;
    if (roll < 85) return ShipClass::MEDIUM;
    return ShipClass::LARGE;
}

void ShipManager::spawnShip(std::vector<GameObject*>& allObjects) {
    DockingCollar* collar = findFreeCollar();
    if (!collar) {
        std::cout << "No free docking collar available" << std::endl;
        return;
    }

    ShipClass sc = rollShipClass();

    auto* ship = new Ship(generateId(), collar->x, collar->y - 400.0f);
    ship->applyClassStats(sc);
    ship->targetCollarId = collar->id;
    ship->targetX = collar->x - (ship->width - CELL_SIZE) / 2.0f;
    ship->targetY = collar->y - ship->height;
    ship->state = ShipState::APPROACHING;
    ship->fuel = 0.0f;
    ship->food = 0.0f;

    allObjects.push_back(ship);
    collar->dockedShipId = ship->id; // reserve the collar

    std::cout << "Ship " << ship->id << " spawned (class " << static_cast<int>(sc)
              << "), heading to collar " << collar->id << std::endl;
}

void ShipManager::updateShip(Ship* ship, float dt, std::vector<GameObject*>& allObjects) {
    switch (ship->state) {
        case ShipState::APPROACHING: {
            // Move toward docking collar
            float dx = ship->targetX - ship->x;
            float dy = ship->targetY - ship->y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 2.0f) {
                ship->x = ship->targetX;
                ship->y = ship->targetY;
                ship->state = ShipState::DOCKING;
                const auto& stats = getShipClassStats(ship->shipClass);
                ship->stateTimer = stats.dockingTime;
                std::cout << "Ship " << ship->id << " docking..." << std::endl;
            } else {
                float speed = getShipClassStats(ship->shipClass).approachSpeed;
                ship->x += (dx / dist) * speed * dt;
                ship->y += (dy / dist) * speed * dt;
            }
            break;
        }
        case ShipState::DOCKING: {
            ship->stateTimer -= dt;
            if (ship->stateTimer <= 0.0f) {
                ship->state = ShipState::UNLOADING;
                ship->stateTimer = 1.0f; // short delay before unloading

                // Open the airlock door on the docking collar
                DockingCollar* collar = findCollar(ship->targetCollarId);
                if (collar) {
                    setAirlockState(collar, allObjects, true);
                }

                std::cout << "Ship " << ship->id << " docked, airlock open, unloading..." << std::endl;
            }
            break;
        }
        case ShipState::UNLOADING: {
            ship->stateTimer -= dt;
            if (ship->stateTimer <= 0.0f) {
                // Spawn physical cargo objects inside the ship
                unloadCargo(ship, allObjects);
                ship->state = ShipState::WAITING_RESUPPLY;
                std::cout << "Ship " << ship->id << " docked with physical cargo inside. "
                          << "Waiting for player to tether and drag it out." << std::endl;
            }
            break;
        }
        case ShipState::WAITING_RESUPPLY: {
            // Countdown patience timer
            ship->patienceTimer -= dt;

            // Happy departure: fully resupplied
            if (ship->isResupplied()) {
                ship->state = ShipState::DEPARTING;
                ship->stateTimer = 2.0f;

                // Destroy any leftover cargo still inside the ship
                destroyCargoInsideShip(ship, allObjects);

                // Close the airlock door and free the docking collar
                DockingCollar* collar = findCollar(ship->targetCollarId);
                if (collar) {
                    setAirlockState(collar, allObjects, false);
                    collar->dockedShipId = 0;
                }

                // Calculate payout based on ship class
                int32_t payout = MEDIUM_SHIP_PAYOUT;
                switch (ship->shipClass) {
                    case ShipClass::SMALL:  payout = SMALL_SHIP_PAYOUT; break;
                    case ShipClass::MEDIUM: payout = MEDIUM_SHIP_PAYOUT; break;
                    case ShipClass::LARGE:  payout = LARGE_SHIP_PAYOUT; break;
                }
                if (onMoneyChange) onMoneyChange(payout, true);

                std::cout << "[SERVER] Ship " << ship->id << " departing HAPPY, payout +" << payout
                          << ", airlock sealed" << std::endl;
            }
            // Angry departure: patience ran out
            else if (ship->patienceTimer <= 0.0f) {
                ship->state = ShipState::DEPARTING;
                ship->stateTimer = 2.0f;

                // Destroy any leftover cargo still inside the ship
                destroyCargoInsideShip(ship, allObjects);

                // Close the airlock door and free the docking collar
                DockingCollar* collar = findCollar(ship->targetCollarId);
                if (collar) {
                    setAirlockState(collar, allObjects, false);
                    collar->dockedShipId = 0;
                }

                // Calculate penalty — partial resupply gives scaled payout instead of full penalty
                float fuelPct = (ship->maxFuel > 0) ? ship->fuel / ship->maxFuel : 1.0f;
                float foodPct = (ship->maxFood > 0) ? ship->food / ship->maxFood : 1.0f;
                float resupplyPct = (fuelPct + foodPct) / 2.0f;

                int32_t delta;
                if (resupplyPct > 0.1f) {
                    // Partial resupply: scale payout proportionally
                    int32_t fullPayout = MEDIUM_SHIP_PAYOUT;
                    switch (ship->shipClass) {
                        case ShipClass::SMALL:  fullPayout = SMALL_SHIP_PAYOUT; break;
                        case ShipClass::MEDIUM: fullPayout = MEDIUM_SHIP_PAYOUT; break;
                        case ShipClass::LARGE:  fullPayout = LARGE_SHIP_PAYOUT; break;
                    }
                    delta = static_cast<int32_t>(fullPayout * resupplyPct * 0.5f);
                    if (delta < 1) delta = 0;
                } else {
                    // Nearly nothing supplied — full penalty
                    delta = ANGRY_DEPART_PENALTY;
                }
                if (onMoneyChange) onMoneyChange(delta, false);

                std::cout << "[SERVER] Ship " << ship->id << " departing ANGRY (patience expired), delta "
                          << delta << ", airlock sealed" << std::endl;
            }
            break;
        }
        case ShipState::DEPARTING: {
            float departSpeed = getShipClassStats(ship->shipClass).approachSpeed;
            ship->y -= departSpeed * dt;
            if (ship->y < -ship->height * 2) {
                ship->active = false;
                ship->state = ShipState::GONE;
                std::cout << "Ship " << ship->id << " gone" << std::endl;
            }
            break;
        }
        default:
            break;
    }
}

void ShipManager::unloadCargo(Ship* ship, std::vector<GameObject*>& allObjects) {
    // Spawn physical Cargo objects inside the ship interior in a grid layout
    // Interior starts at (ship.x + 8, ship.y + 8) — 4px wall + 4px padding
    constexpr float WALL_PAD = 8.0f;
    constexpr float CARGO_SIZE = 16.0f;
    constexpr float CARGO_GAP = 4.0f;
    constexpr float STRIDE = CARGO_SIZE + CARGO_GAP; // 20px

    float interiorW = ship->width - WALL_PAD * 2.0f;
    int cols = static_cast<int>(interiorW / STRIDE);
    if (cols < 1) cols = 1;

    float startX = ship->x + WALL_PAD;
    float startY = ship->y + WALL_PAD;
    int col = 0;
    int row = 0;

    auto placeCargo = [&](CargoType type) {
        float cx = startX + col * STRIDE;
        float cy = startY + row * STRIDE;
        auto* cargo = new Cargo(generateId(), cx, cy, type, 1);
        allObjects.push_back(cargo);
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    };

    for (int i = 0; i < ship->metalToUnload; i++) placeCargo(CargoType::METAL);
    for (int i = 0; i < ship->oreToUnload; i++) placeCargo(CargoType::ORE);
    for (int i = 0; i < ship->crystalsToUnload; i++) placeCargo(CargoType::CRYSTALS);
    for (int i = 0; i < ship->plasmaToUnload; i++) placeCargo(CargoType::PLASMA);

    std::cout << "Spawned " << (ship->metalToUnload + ship->oreToUnload +
                 ship->crystalsToUnload + ship->plasmaToUnload)
              << " physical cargo items inside ship " << ship->id << std::endl;

    // Zero out virtual counts — totalMetal/etc stay for objectives panel
    ship->metalToUnload = 0;
    ship->oreToUnload = 0;
    ship->crystalsToUnload = 0;
    ship->plasmaToUnload = 0;
}

void ShipManager::destroyCargoInsideShip(Ship* ship, std::vector<GameObject*>& allObjects) {
    // Destroy any cargo objects that are still inside the ship bounds
    // but NOT tethered or carried by a player (those were saved!)
    int destroyed = 0;
    for (auto* obj : allObjects) {
        if (obj->type != GameObjectType::CARGO || !obj->active) continue;
        auto* cargo = static_cast<Cargo*>(obj);

        // Skip if a player is carrying or tethering this cargo
        if (cargo->carriedByPlayerId != 0) continue;
        if (cargo->tetheredToPlayerId != 0) continue;

        // Check if cargo center is inside ship bounds
        float ccx = cargo->x + cargo->width / 2.0f;
        float ccy = cargo->y + cargo->height / 2.0f;
        if (ccx >= ship->x && ccx < ship->x + ship->width &&
            ccy >= ship->y && ccy < ship->y + ship->height) {
            cargo->active = false;
            destroyed++;
        }
    }
    if (destroyed > 0) {
        std::cout << "Destroyed " << destroyed << " leftover cargo inside ship "
                  << ship->id << " on departure" << std::endl;
    }
}

bool ShipManager::loadCargoOntoShip(Ship* ship, Cargo* cargo) {
    if (ship->state != ShipState::WAITING_RESUPPLY) return false;

    switch (cargo->cargoType) {
        case CargoType::FUEL:
            ship->fuel += 25.0f;
            if (ship->fuel > ship->maxFuel) ship->fuel = ship->maxFuel;
            cargo->active = false;
            return true;
        case CargoType::FOOD:
            ship->food += 25.0f;
            if (ship->food > ship->maxFood) ship->food = ship->maxFood;
            cargo->active = false;
            return true;
        default:
            return false; // ships don't accept materials back
    }
}

DockingCollar* ShipManager::findCollar(uint32_t collarId) {
    for (auto* collar : dockingCollars) {
        if (collar->id == collarId) return collar;
    }
    return nullptr;
}

DockingCollar* ShipManager::findFreeCollar() {
    for (auto* collar : dockingCollars) {
        if (!collar->hasShip()) return collar;
    }
    return nullptr;
}

void ShipManager::addCollar(DockingCollar* collar) {
    dockingCollars.push_back(collar);
}

void ShipManager::removeCollar(DockingCollar* collar) {
    for (auto it = dockingCollars.begin(); it != dockingCollars.end(); ++it) {
        if (*it == collar) {
            dockingCollars.erase(it);
            return;
        }
    }
}

void ShipManager::setAirlockState(DockingCollar* collar, std::vector<GameObject*>& allObjects, bool open) {
    if (collar->linkedDoorId == 0) return;

    for (auto* obj : allObjects) {
        if (obj->id == collar->linkedDoorId && obj->type == GameObjectType::DOOR && obj->active) {
            auto* door = static_cast<Door*>(obj);
            door->state = open ? DoorState::OPEN : DoorState::CLOSED;
            std::cout << "Airlock on collar " << collar->id
                      << (open ? " opened" : " closed") << std::endl;
            break;
        }
    }
}

} // namespace ssm
