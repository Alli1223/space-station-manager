#pragma once

#include "shared/game_objects.h"
#include "shared/map.h"
#include <vector>
#include <functional>
#include <random>

namespace ssm {

class ShipManager {
public:
    using IdGenerator = std::function<uint32_t()>;

    float spawnInterval = SHIP_SPAWN_INTERVAL;
    float spawnTimer = 0.0f;

    void init(const std::vector<DockingCollar*>& collars, IdGenerator idGen);
    void update(float dt, std::vector<GameObject*>& allObjects);

    // Try to load cargo onto a docked ship
    bool loadCargoOntoShip(Ship* ship, Cargo* cargo);

    // Dynamic collar management for edit mode
    void addCollar(DockingCollar* collar);
    void removeCollar(DockingCollar* collar);

private:
    std::vector<DockingCollar*> dockingCollars;
    IdGenerator generateId;
    std::mt19937 rng{std::random_device{}()};

    ShipClass rollShipClass();
    void spawnShip(std::vector<GameObject*>& allObjects);
    void updateShip(Ship* ship, float dt, std::vector<GameObject*>& allObjects);
    void unloadCargo(Ship* ship, std::vector<GameObject*>& allObjects);
    DockingCollar* findCollar(uint32_t collarId);
    DockingCollar* findFreeCollar();
    void setAirlockState(DockingCollar* collar, std::vector<GameObject*>& allObjects, bool open);
};

} // namespace ssm
