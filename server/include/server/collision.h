#pragma once

#include "shared/map.h"
#include "shared/game_objects.h"
#include <vector>

namespace ssm {

class CollisionSystem {
public:
    // Check if a rectangle would collide with solid cells, closed doors, or ship walls
    bool wouldCollide(const StationMap& map, const std::vector<Door*>& doors,
                      const std::vector<Ship*>& dockedShips,
                      float x, float y, float w, float h) const;

    // Move a player with collision resolution (axis-separated)
    void moveWithCollision(const StationMap& map, const std::vector<Door*>& doors,
                           const std::vector<Ship*>& dockedShips,
                           Player& player, float dx, float dy, float dt);

    // Check if a cargo item would collide with walls/doors (not ship walls — cargo can be inside ships)
    bool wouldCargoCollide(const StationMap& map, const std::vector<Door*>& doors,
                           float x, float y, float w, float h) const;

private:
    // Check if a world-space rectangle overlaps any solid grid cell
    bool checkGridCollision(const StationMap& map, float x, float y, float w, float h) const;

    // Check if a world-space rectangle overlaps any closed door
    bool checkDoorCollision(const std::vector<Door*>& doors, float x, float y, float w, float h) const;

    // Check if a world-space rectangle overlaps any docked ship's walls (left/right/top, bottom open)
    bool checkShipWallCollision(const std::vector<Ship*>& dockedShips, float x, float y, float w, float h) const;
};

} // namespace ssm
