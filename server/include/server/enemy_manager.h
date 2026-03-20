#pragma once

#include "shared/game_objects.h"
#include "shared/map.h"
#include <vector>
#include <functional>
#include <random>

namespace ssm {

class EnemyManager {
public:
    using IdGenerator = std::function<uint32_t()>;

    void init(IdGenerator idGen);
    void update(float dt, std::vector<GameObject*>& allObjects,
                const StationMap& map, float stationCenterX, float stationCenterY);

    int getCurrentWave() const { return currentWave; }

private:
    IdGenerator generateId;
    float waveTimer = ENEMY_WAVE_INITIAL_DELAY;
    int currentWave = 0;
    std::mt19937 rng{std::random_device{}()};

    void spawnWave(std::vector<GameObject*>& allObjects,
                   float centerX, float centerY);
    void updateEnemy(EnemyShip* enemy, float dt,
                     std::vector<GameObject*>& allObjects,
                     const StationMap& map, float stationCenterX, float stationCenterY);

    // Find nearest wall cell from a world position
    bool findNearestWall(const StationMap& map, float worldX, float worldY,
                         int& outGX, int& outGY) const;
};

} // namespace ssm
