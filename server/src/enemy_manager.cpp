#include "server/enemy_manager.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace ssm {

void EnemyManager::init(IdGenerator idGen) {
    generateId = idGen;
    waveTimer = ENEMY_WAVE_INITIAL_DELAY;
    currentWave = 0;
}

void EnemyManager::update(float dt, std::vector<GameObject*>& allObjects,
                           const StationMap& map, float stationCenterX, float stationCenterY) {
    // Wave timer
    waveTimer -= dt;
    if (waveTimer <= 0.0f) {
        currentWave++;
        spawnWave(allObjects, stationCenterX, stationCenterY);
        waveTimer = ENEMY_WAVE_INTERVAL;
    }

    // Update each active enemy
    for (auto* obj : allObjects) {
        if (obj->type == GameObjectType::ENEMY_SHIP && obj->active) {
            auto* enemy = static_cast<EnemyShip*>(obj);
            updateEnemy(enemy, dt, allObjects, map, stationCenterX, stationCenterY);
        }
    }
}

void EnemyManager::spawnWave(std::vector<GameObject*>& allObjects,
                              float centerX, float centerY) {
    int count = (std::min)(2 + currentWave, 8);
    float spawnRadius = 2500.0f;

    std::cout << "[COMBAT] Wave " << currentWave << " spawning " << count << " enemies" << std::endl;

    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    float baseAngle = angleDist(rng);

    for (int i = 0; i < count; i++) {
        float angle = baseAngle + (6.283185f / count) * i;
        float spawnX = centerX + std::cos(angle) * spawnRadius;
        float spawnY = centerY + std::sin(angle) * spawnRadius;

        auto* enemy = new EnemyShip(generateId(), spawnX, spawnY);
        enemy->health = ENEMY_HP + currentWave * 20.0f;
        enemy->maxHealth = enemy->health;
        enemy->speed = ENEMY_SPEED + currentWave * 3.0f;
        enemy->waveIndex = currentWave;

        // Target: station center (movement bounded by station boundary check)
        enemy->targetX = centerX;
        enemy->targetY = centerY;

        allObjects.push_back(enemy);
    }
}

void EnemyManager::updateEnemy(EnemyShip* enemy, float dt,
                                std::vector<GameObject*>& allObjects,
                                const StationMap& map,
                                float stationCenterX, float stationCenterY) {
    if (enemy->health <= 0.0f) {
        enemy->active = false;
        return;
    }

    float ecx = enemy->x + enemy->width / 2.0f;
    float ecy = enemy->y + enemy->height / 2.0f;

    // Move toward station center (will be blocked by station boundary)
    float dx = stationCenterX - ecx;
    float dy = stationCenterY - ecy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // Check if near enough to any station wall to start shooting
    bool inRange = false;
    int nearWallGX = -1, nearWallGY = -1;

    if (findNearestWall(map, ecx, ecy, nearWallGX, nearWallGY)) {
        float wallWX = (nearWallGX + 0.5f) * CELL_SIZE;
        float wallWY = (nearWallGY + 0.5f) * CELL_SIZE;
        float wdx = wallWX - ecx;
        float wdy = wallWY - ecy;
        float wallDist = std::sqrt(wdx * wdx + wdy * wdy);
        if (wallDist < ENEMY_FIRE_RANGE) {
            inRange = true;
        }
    }

    bool blocked = false;
    if (!inRange && dist > 10.0f) {
        // Move toward station, but never fly over station cells
        float dirX = dx / dist;
        float dirY = dy / dist;
        float newX = enemy->x + dirX * enemy->speed * dt;
        float newY = enemy->y + dirY * enemy->speed * dt;

        // Check all four corners of the enemy at new position
        auto isOpen = [&](float px, float py) {
            int gx = static_cast<int>(std::floor(px / CELL_SIZE));
            int gy = static_cast<int>(std::floor(py / CELL_SIZE));
            return map.getCell(gx, gy) == CellType::EMPTY;
        };
        bool canMove = isOpen(newX, newY) &&
                        isOpen(newX + enemy->width, newY) &&
                        isOpen(newX, newY + enemy->height) &&
                        isOpen(newX + enemy->width, newY + enemy->height);

        if (canMove) {
            enemy->x = newX;
            enemy->y = newY;
        } else {
            blocked = true;
        }
    }

    // Orbit tangentially when in firing range or blocked by station boundary
    if (inRange || blocked) {
        float toDx = stationCenterX - ecx;
        float toDy = stationCenterY - ecy;
        float toLen = std::sqrt(toDx * toDx + toDy * toDy);
        if (toLen > 1.0f) {
            float tangentX = -toDy / toLen;
            float tangentY = toDx / toLen;
            float orbitSpeed = enemy->speed * 0.7f;
            float orbX = enemy->x + tangentX * orbitSpeed * dt;
            float orbY = enemy->y + tangentY * orbitSpeed * dt;

            auto isOpen = [&](float px, float py) {
                int gx = static_cast<int>(std::floor(px / CELL_SIZE));
                int gy = static_cast<int>(std::floor(py / CELL_SIZE));
                return map.getCell(gx, gy) == CellType::EMPTY;
            };
            if (isOpen(orbX, orbY) &&
                isOpen(orbX + enemy->width, orbY) &&
                isOpen(orbX, orbY + enemy->height) &&
                isOpen(orbX + enemy->width, orbY + enemy->height)) {
                enemy->x = orbX;
                enemy->y = orbY;
            }
        }
    }

    // Fire at nearest wall when in range
    if (inRange && nearWallGX >= 0) {
        enemy->fireCooldown -= dt;
        if (enemy->fireCooldown <= 0.0f) {
            // Spawn projectile aimed at nearest wall
            float wallWX = (nearWallGX + 0.5f) * CELL_SIZE;
            float wallWY = (nearWallGY + 0.5f) * CELL_SIZE;
            float ecx = enemy->x + enemy->width / 2.0f;
            float ecy = enemy->y + enemy->height / 2.0f;
            float aimDx = wallWX - ecx;
            float aimDy = wallWY - ecy;
            float aimLen = std::sqrt(aimDx * aimDx + aimDy * aimDy);
            if (aimLen > 0.01f) {
                aimDx /= aimLen;
                aimDy /= aimLen;
            }

            float projSpeed = 500.0f;
            auto* proj = new Projectile(generateId(),
                ecx - PROJECTILE_SIZE / 2.0f,
                ecy - PROJECTILE_SIZE / 2.0f,
                ProjectileOwner::ENEMY,
                aimDx * projSpeed, aimDy * projSpeed,
                ENEMY_DAMAGE, enemy->id);
            allObjects.push_back(proj);

            enemy->fireCooldown = 1.0f / ENEMY_FIRE_RATE;
        }
    }
}

bool EnemyManager::findNearestWall(const StationMap& map, float worldX, float worldY,
                                    int& outGX, int& outGY) const {
    int centerGX = static_cast<int>(std::floor(worldX / CELL_SIZE));
    int centerGY = static_cast<int>(std::floor(worldY / CELL_SIZE));

    float nearestDist = 999999.0f;
    bool found = false;

    // Search in a radius around the enemy position
    int searchRadius = static_cast<int>(ENEMY_FIRE_RANGE / CELL_SIZE) + 2;
    for (int dy = -searchRadius; dy <= searchRadius; dy++) {
        for (int dx = -searchRadius; dx <= searchRadius; dx++) {
            int gx = centerGX + dx;
            int gy = centerGY + dy;
            if (map.getCell(gx, gy) == CellType::WALL) {
                float wx = (gx + 0.5f) * CELL_SIZE;
                float wy = (gy + 0.5f) * CELL_SIZE;
                float ddx = wx - worldX;
                float ddy = wy - worldY;
                float dist = std::sqrt(ddx * ddx + ddy * ddy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    outGX = gx;
                    outGY = gy;
                    found = true;
                }
            }
        }
    }

    return found;
}

} // namespace ssm
