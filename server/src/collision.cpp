#include "server/collision.h"
#include <cmath>
#include <algorithm>

namespace ssm {

bool CollisionSystem::wouldCollide(const StationMap& map, const std::vector<Door*>& doors,
                                    const std::vector<Ship*>& dockedShips,
                                    float x, float y, float w, float h) const {
    return checkGridCollision(map, x, y, w, h) ||
           checkDoorCollision(doors, x, y, w, h) ||
           checkShipWallCollision(dockedShips, x, y, w, h);
}

void CollisionSystem::moveWithCollision(const StationMap& map, const std::vector<Door*>& doors,
                                         const std::vector<Ship*>& dockedShips,
                                         Player& player, float dx, float dy, float dt) {
    float moveX = dx * player.speed * dt;
    float moveY = dy * player.speed * dt;

    // Axis-separated collision resolution

    // Try X movement
    float newX = player.x + moveX;
    if (!wouldCollide(map, doors, dockedShips, newX, player.y, player.width, player.height)) {
        player.x = newX;
    } else {
        // Slide along wall: try to get as close as possible
        if (moveX > 0) {
            // Moving right - find the left edge of the blocking cell
            int cellX = static_cast<int>((newX + player.width) / CELL_SIZE);
            player.x = cellX * CELL_SIZE - player.width - 0.01f;
        } else if (moveX < 0) {
            // Moving left - find the right edge of the blocking cell
            int cellX = static_cast<int>(newX / CELL_SIZE);
            player.x = (cellX + 1) * CELL_SIZE + 0.01f;
        }
        // Re-check - if still colliding, revert
        if (wouldCollide(map, doors, dockedShips, player.x, player.y, player.width, player.height)) {
            player.x = newX - moveX; // revert
        }
    }

    // Try Y movement
    float newY = player.y + moveY;
    if (!wouldCollide(map, doors, dockedShips, player.x, newY, player.width, player.height)) {
        player.y = newY;
    } else {
        if (moveY > 0) {
            int cellY = static_cast<int>((newY + player.height) / CELL_SIZE);
            player.y = cellY * CELL_SIZE - player.height - 0.01f;
        } else if (moveY < 0) {
            int cellY = static_cast<int>(newY / CELL_SIZE);
            player.y = (cellY + 1) * CELL_SIZE + 0.01f;
        }
        if (wouldCollide(map, doors, dockedShips, player.x, player.y, player.width, player.height)) {
            player.y = newY - moveY; // revert
        }
    }
}

bool CollisionSystem::checkGridCollision(const StationMap& map, float x, float y, float w, float h) const {
    // Check all grid cells the rectangle overlaps (no clamping - map handles out-of-bounds)
    int startCellX = static_cast<int>(std::floor(x / CELL_SIZE));
    int startCellY = static_cast<int>(std::floor(y / CELL_SIZE));
    int endCellX = static_cast<int>(std::floor((x + w - 0.01f) / CELL_SIZE));
    int endCellY = static_cast<int>(std::floor((y + h - 0.01f) / CELL_SIZE));

    for (int cy = startCellY; cy <= endCellY; cy++) {
        for (int cx = startCellX; cx <= endCellX; cx++) {
            if (map.isSolid(cx, cy)) {
                return true;
            }
        }
    }
    return false;
}

bool CollisionSystem::checkDoorCollision(const std::vector<Door*>& doors, float x, float y, float w, float h) const {
    for (const auto* door : doors) {
        if (door->isSolid() && door->active) {
            if (x < door->x + door->width && x + w > door->x &&
                y < door->y + door->height && y + h > door->y) {
                return true;
            }
        }
    }
    return false;
}

bool CollisionSystem::checkShipWallCollision(const std::vector<Ship*>& dockedShips,
                                              float x, float y, float w, float h) const {
    constexpr float WALL_THICKNESS = 4.0f;

    for (const auto* ship : dockedShips) {
        float sx = ship->x;
        float sy = ship->y;
        float sw = ship->width;
        float sh = ship->height;

        // Only check collision if the player is near the ship
        // Quick broad-phase: skip if clearly outside the ship bounds + margin
        if (x + w < sx - WALL_THICKNESS || x > sx + sw + WALL_THICKNESS ||
            y + h < sy - WALL_THICKNESS || y > sy + sh + WALL_THICKNESS) {
            continue;
        }

        // Left wall: (sx, sy, WALL_THICKNESS, sh)
        if (x < sx + WALL_THICKNESS && x + w > sx &&
            y < sy + sh && y + h > sy) {
            return true;
        }

        // Right wall: (sx + sw - WALL_THICKNESS, sy, WALL_THICKNESS, sh)
        if (x < sx + sw && x + w > sx + sw - WALL_THICKNESS &&
            y < sy + sh && y + h > sy) {
            return true;
        }

        // Top wall: (sx, sy, sw, WALL_THICKNESS)
        if (x < sx + sw && x + w > sx &&
            y < sy + WALL_THICKNESS && y + h > sy) {
            return true;
        }

        // Bottom edge: OPEN (no collision — docking collar connection)
    }

    return false;
}

} // namespace ssm
