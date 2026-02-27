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

    auto* ship = new Ship(generateId(), collar->x, collar->y - 200.0f);
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
                // Cargo stays as virtual counts on the ship (metalToUnload, woodToUnload)
                // Players withdraw items by interacting near the docked ship
                ship->state = ShipState::WAITING_RESUPPLY;
                std::cout << "Ship " << ship->id << " docked with cargo: "
                          << ship->metalToUnload << " metal, "
                          << ship->woodToUnload << " wood. Waiting for player interaction." << std::endl;
            }
            break;
        }
        case ShipState::WAITING_RESUPPLY: {
            if (ship->isResupplied()) {
                ship->state = ShipState::DEPARTING;
                ship->stateTimer = 2.0f;

                // Close the airlock door and free the docking collar
                DockingCollar* collar = findCollar(ship->targetCollarId);
                if (collar) {
                    setAirlockState(collar, allObjects, false);
                    collar->dockedShipId = 0;
                }

                std::cout << "Ship " << ship->id << " departing, airlock sealed" << std::endl;
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
    DockingCollar* collar = findCollar(ship->targetCollarId);
    if (!collar) return;

    // Place cargo near the docking collar (inside the station)
    float cargoX = collar->x;
    float cargoY = collar->y + CELL_SIZE; // one cell below the collar (inside)
    float offset = 0;

    for (int i = 0; i < ship->metalToUnload; i++) {
        auto* cargo = new Cargo(generateId(), cargoX + offset, cargoY, CargoType::METAL, 1);
        allObjects.push_back(cargo);
        offset += 18.0f;
        if (offset > CELL_SIZE * 2) { offset = 0; cargoY += 18.0f; }
    }
    for (int i = 0; i < ship->woodToUnload; i++) {
        auto* cargo = new Cargo(generateId(), cargoX + offset, cargoY, CargoType::WOOD, 1);
        allObjects.push_back(cargo);
        offset += 18.0f;
        if (offset > CELL_SIZE * 2) { offset = 0; cargoY += 18.0f; }
    }

    ship->metalToUnload = 0;
    ship->woodToUnload = 0;
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
            return false; // ships don't accept metal/wood back
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
