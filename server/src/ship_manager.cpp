#include "server/ship_manager.h"
#include <iostream>
#include <cmath>

namespace ssm {

void ShipManager::init(const std::vector<LandingPadInfo>& pads, IdGenerator idGen) {
    landingPads = pads;
    generateId = idGen;
    spawnTimer = 10.0f; // first ship arrives after 10 seconds
    std::cout << "[ShipManager] Initialized with " << landingPads.size() << " landing pads" << std::endl;
}

void ShipManager::update(float dt, std::vector<GameObject*>& allObjects) {
    spawnTimer -= dt;
    if (spawnTimer <= 0.0f) {
        spawnShip(allObjects);
        spawnTimer = spawnInterval;
    }

    // Track whether hangar door needs to be open
    hangarOpenNeeded = false;

    // Update all ships
    for (auto* obj : allObjects) {
        if (obj->type == GameObjectType::SHIP && obj->active) {
            auto* ship = static_cast<Ship*>(obj);
            if (ship->state == ShipState::APPROACHING || ship->state == ShipState::DEPARTING) {
                hangarOpenNeeded = true;
            }
            updateShip(ship, dt, allObjects);
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
    LandingPadInfo* pad = findFreePad();
    if (!pad) {
        std::cout << "No free landing pad available" << std::endl;
        return;
    }

    ShipClass sc = rollShipClass();
    const auto& stats = getShipClassStats(sc);

    // Spawn far away from a random direction
    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    float angle = angleDist(rng);
    float spawnDist = 2000.0f; // spawn very far from the pad
    float spawnX = pad->centerX + std::cos(angle) * spawnDist - stats.width / 2.0f;
    float spawnY = pad->centerY + std::sin(angle) * spawnDist - stats.height / 2.0f;

    auto* ship = new Ship(generateId(), spawnX, spawnY);
    ship->applyClassStats(sc);
    ship->targetCollarId = 0; // unused in landing pad system
    ship->targetX = pad->centerX - ship->width / 2.0f;
    ship->targetY = pad->centerY - ship->height / 2.0f;
    ship->state = ShipState::APPROACHING;
    ship->departAngle = angle; // remember arrival angle for departure
    ship->fuel = 0.0f;
    ship->food = 0.0f;

    // 30% chance: refuelling-only ship (no cargo to unload, just needs fuel + food)
    std::uniform_int_distribution<int> refuelDist(0, 99);
    bool refuelOnly = (refuelDist(rng) < 30);
    if (refuelOnly) {
        ship->metalToUnload = 0;
        ship->oreToUnload = 0;
        ship->crystalsToUnload = 0;
        ship->plasmaToUnload = 0;
        ship->totalMetal = 0;
        ship->totalOre = 0;
        ship->totalCrystals = 0;
        ship->totalPlasma = 0;
        // Give more patience since the task is simpler
        ship->patienceTimer *= 1.5f;
        ship->maxPatience *= 1.5f;
    }

    allObjects.push_back(ship);
    pad->dockedShipId = ship->id; // reserve the pad

    std::cout << "Ship " << ship->id << " spawned (class " << static_cast<int>(sc)
              << (refuelOnly ? ", refuel-only" : "")
              << "), heading to landing pad at (" << pad->centerX << ", " << pad->centerY << ")" << std::endl;
}

void ShipManager::updateShip(Ship* ship, float dt, std::vector<GameObject*>& allObjects) {
    switch (ship->state) {
        case ShipState::APPROACHING: {
            // Move toward landing pad
            float dx = ship->targetX - ship->x;
            float dy = ship->targetY - ship->y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 2.0f) {
                ship->x = ship->targetX;
                ship->y = ship->targetY;
                ship->state = ShipState::DOCKING;
                const auto& stats = getShipClassStats(ship->shipClass);
                ship->stateTimer = stats.dockingTime;
                std::cout << "Ship " << ship->id << " landing on pad..." << std::endl;
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
                ship->stateTimer = 1.0f;
                std::cout << "Ship " << ship->id << " landed, unloading cargo..." << std::endl;
            }
            break;
        }
        case ShipState::UNLOADING: {
            ship->stateTimer -= dt;
            if (ship->stateTimer <= 0.0f) {
                unloadCargo(ship, allObjects);
                ship->state = ShipState::WAITING_RESUPPLY;
                std::cout << "Ship " << ship->id << " landed with physical cargo inside. "
                          << "Waiting for player to take cargo and resupply." << std::endl;
            }
            break;
        }
        case ShipState::WAITING_RESUPPLY: {
            ship->patienceTimer -= dt;

            // Happy departure: fully resupplied
            if (ship->isResupplied()) {
                ship->state = ShipState::DEPARTING;
                ship->stateTimer = 2.0f;

                destroyCargoInsideShip(ship, allObjects);

                // Free the landing pad
                LandingPadInfo* pad = findPadForShip(ship->id);
                if (pad) pad->dockedShipId = 0;

                int32_t payout = MEDIUM_SHIP_PAYOUT;
                switch (ship->shipClass) {
                    case ShipClass::SMALL:  payout = SMALL_SHIP_PAYOUT; break;
                    case ShipClass::MEDIUM: payout = MEDIUM_SHIP_PAYOUT; break;
                    case ShipClass::LARGE:  payout = LARGE_SHIP_PAYOUT; break;
                }
                if (onMoneyChange) onMoneyChange(payout, true);

                std::cout << "[SERVER] Ship " << ship->id << " departing HAPPY, payout +" << payout << std::endl;
            }
            // Angry departure: patience ran out
            else if (ship->patienceTimer <= 0.0f) {
                ship->state = ShipState::DEPARTING;
                ship->stateTimer = 2.0f;

                destroyCargoInsideShip(ship, allObjects);

                LandingPadInfo* pad = findPadForShip(ship->id);
                if (pad) pad->dockedShipId = 0;

                float fuelPct = (ship->maxFuel > 0) ? ship->fuel / ship->maxFuel : 1.0f;
                float foodPct = (ship->maxFood > 0) ? ship->food / ship->maxFood : 1.0f;
                float resupplyPct = (fuelPct + foodPct) / 2.0f;

                int32_t delta;
                if (resupplyPct > 0.1f) {
                    int32_t fullPayout = MEDIUM_SHIP_PAYOUT;
                    switch (ship->shipClass) {
                        case ShipClass::SMALL:  fullPayout = SMALL_SHIP_PAYOUT; break;
                        case ShipClass::MEDIUM: fullPayout = MEDIUM_SHIP_PAYOUT; break;
                        case ShipClass::LARGE:  fullPayout = LARGE_SHIP_PAYOUT; break;
                    }
                    delta = static_cast<int32_t>(fullPayout * resupplyPct * 0.5f);
                    if (delta < 1) delta = 0;
                } else {
                    delta = ANGRY_DEPART_PENALTY;
                }
                if (onMoneyChange) onMoneyChange(delta, false);

                std::cout << "[SERVER] Ship " << ship->id << " departing ANGRY, delta " << delta << std::endl;
            }
            break;
        }
        case ShipState::DEPARTING: {
            float departSpeed = getShipClassStats(ship->shipClass).approachSpeed;
            // Fly outward in the direction the ship arrived from
            ship->x += std::cos(ship->departAngle) * departSpeed * dt;
            ship->y += std::sin(ship->departAngle) * departSpeed * dt;
            // Check if far enough from target to despawn
            float ddx = ship->x - ship->targetX;
            float ddy = ship->y - ship->targetY;
            if (std::sqrt(ddx * ddx + ddy * ddy) > 2500.0f) {
                ship->active = false;
                ship->state = ShipState::GONE;
                LandingPadInfo* pad = findPadForShip(ship->id);
                if (pad) pad->dockedShipId = 0;
                std::cout << "Ship " << ship->id << " gone" << std::endl;
            }
            break;
        }
        default:
            break;
    }
}

void ShipManager::unloadCargo(Ship* ship, std::vector<GameObject*>& allObjects) {
    constexpr float WALL_PAD = 8.0f;
    constexpr float CARGO_SIZE = 16.0f;
    constexpr float CARGO_GAP = 4.0f;
    constexpr float STRIDE = CARGO_SIZE + CARGO_GAP;

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

    ship->metalToUnload = 0;
    ship->oreToUnload = 0;
    ship->crystalsToUnload = 0;
    ship->plasmaToUnload = 0;
}

void ShipManager::destroyCargoInsideShip(Ship* ship, std::vector<GameObject*>& allObjects) {
    int destroyed = 0;
    for (auto* obj : allObjects) {
        if (obj->type != GameObjectType::CARGO || !obj->active) continue;
        auto* cargo = static_cast<Cargo*>(obj);
        if (cargo->carriedByPlayerId != 0) continue;
        if (cargo->tetheredToPlayerId != 0) continue;
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
            return false;
    }
}

LandingPadInfo* ShipManager::findFreePad() {
    for (auto& pad : landingPads) {
        if (pad.dockedShipId == 0) return &pad;
    }
    return nullptr;
}

LandingPadInfo* ShipManager::findPadForShip(uint32_t shipId) {
    for (auto& pad : landingPads) {
        if (pad.dockedShipId == shipId) return &pad;
    }
    return nullptr;
}

void ShipManager::addPad(const LandingPadInfo& pad) {
    landingPads.push_back(pad);
}

bool ShipManager::needsHangarOpen() const {
    return hangarOpenNeeded;
}

void ShipManager::removePadAt(float cx, float cy) {
    for (auto it = landingPads.begin(); it != landingPads.end(); ++it) {
        if (std::abs(it->centerX - cx) < CELL_SIZE && std::abs(it->centerY - cy) < CELL_SIZE) {
            landingPads.erase(it);
            return;
        }
    }
}

} // namespace ssm
