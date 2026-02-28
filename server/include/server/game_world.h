#pragma once

#include "shared/game_objects.h"
#include "shared/map.h"
#include "server/collision.h"
#include "server/ship_manager.h"
#include "server/network_server.h"
#include <vector>
#include <unordered_map>

namespace ssm {

class GameWorld {
public:
    ~GameWorld();

    bool init(const std::string& mapFile);
    void update(float dt);

    // Network event handlers
    void onPlayerJoin(uint32_t clientIndex, const std::string& name);
    void onPlayerInput(uint32_t clientIndex, float dx, float dy, bool interact);
    void onPlayerDisconnect(uint32_t clientIndex);

    // Edit mode: returns true if the edit was accepted
    bool onCellEdit(uint32_t clientIndex, int16_t gridX, int16_t gridY, CellType cellType);

    // Cargo placement: player clicks to place carried cargo at world position
    void onCargoPlace(uint32_t clientIndex, float targetX, float targetY);

    // Build state snapshot for network
    ByteBuffer buildStateSnapshot();
    ByteBuffer buildWelcome(uint32_t playerId);

    StationMap& getMap() { return map; }
    const std::vector<GameObject*>& getObjects() const { return objects; }
    int32_t getMoney() const { return stationMoney; }

private:
    StationMap map;
    CollisionSystem collision;
    ShipManager shipManager;
    std::vector<GameObject*> objects;
    std::unordered_map<uint32_t, Player*> playersByClientIndex;
    uint32_t nextId = 1;
    int32_t stationMoney = STARTING_MONEY;

    uint32_t generateId() { return nextId++; }

    void buildMapObjects();
    Player* findPlayer(uint32_t clientIndex);
    void handleInteraction(Player* player);
    void handleTerminalUse(Player* player, Terminal* terminal);
    void handleCargoPickup(Player* player);
    bool handleShipWithdraw(Player* player);
    void handleCargoDropOrLoad(Player* player);

    // Edit mode helpers
    bool validateEdit(int16_t gridX, int16_t gridY, CellType newType);
    void rebuildCellObjects(int16_t gridX, int16_t gridY, CellType oldType, CellType newType);
    void removeObjectsAtGrid(int16_t gridX, int16_t gridY);

    // Helpers
    std::vector<Door*> getAllDoors() const;
    std::vector<Ship*> getDockedShips() const;
    template<typename T>
    T* findNearestObject(float x, float y, float range) const;
    template<typename T>
    std::vector<T*> findObjectsOfType() const;
};

} // namespace ssm
