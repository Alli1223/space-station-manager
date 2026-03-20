#pragma once

#include "shared/game_objects.h"
#include "shared/map.h"
#include <vector>
#include <functional>
#include <random>

namespace ssm {

struct LandingPadInfo {
    float centerX = 0.0f;       // world X of pad center
    float centerY = 0.0f;       // world Y of pad center
    uint32_t dockedShipId = 0;  // 0 = free
};

class ShipManager {
public:
    using IdGenerator = std::function<uint32_t()>;
    using MoneyCallback = std::function<void(int32_t delta, bool happy)>;

    float spawnInterval = SHIP_SPAWN_INTERVAL;
    float spawnTimer = 0.0f;

    MoneyCallback onMoneyChange;

    void init(const std::vector<LandingPadInfo>& pads, IdGenerator idGen);
    void update(float dt, std::vector<GameObject*>& allObjects);

    // Try to load cargo onto a docked ship
    bool loadCargoOntoShip(Ship* ship, Cargo* cargo);

    // Dynamic pad management for edit mode
    void addPad(const LandingPadInfo& pad);
    void removePadAt(float cx, float cy);

    // Hangar door control: true if any ship is approaching or departing
    bool needsHangarOpen() const;

    const std::vector<LandingPadInfo>& getPads() const { return landingPads; }

private:
    std::vector<LandingPadInfo> landingPads;
    IdGenerator generateId;
    std::mt19937 rng{std::random_device{}()};

    ShipClass rollShipClass();
    void spawnShip(std::vector<GameObject*>& allObjects);
    void updateShip(Ship* ship, float dt, std::vector<GameObject*>& allObjects);
    void unloadCargo(Ship* ship, std::vector<GameObject*>& allObjects);
    void destroyCargoInsideShip(Ship* ship, std::vector<GameObject*>& allObjects);
    LandingPadInfo* findFreePad();
    LandingPadInfo* findPadForShip(uint32_t shipId);

    // Cached: set true when any ship is approaching/departing
    mutable bool hangarOpenNeeded = false;
};

} // namespace ssm
