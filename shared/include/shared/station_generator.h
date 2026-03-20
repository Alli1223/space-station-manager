#pragma once

#include "shared/map.h"
#include <random>
#include <vector>
#include <string>

namespace ssm {

enum class RoomType {
    LANDING_BAY,
    STORAGE,
    COMMAND,
    MESS_HALL,
    ENGINEERING,
    MEDBAY,
    CREW_QUARTERS,
    REFINERY,
};

struct RoomDef {
    RoomType type;
    int minWidth;    // interior dimensions (excluding walls)
    int minHeight;
    int maxWidth;
    int maxHeight;
};

struct PlacedRoom {
    RoomType type;
    int x, y;        // top-left corner of the WALL bounding box in grid coords
    int width;       // total width including walls
    int height;      // total height including walls

    int interiorX() const { return x + 1; }
    int interiorY() const { return y + 1; }
    int interiorW() const { return width - 2; }
    int interiorH() const { return height - 2; }
    int centerX() const { return x + width / 2; }
    int centerY() const { return y + height / 2; }
};

class StationGenerator {
public:
    bool generate(StationMap& map, uint32_t seed = 0);

private:
    std::mt19937 rng;

    int gridWidth = 0;
    int gridHeight = 0;
    std::vector<CellType> grid;
    std::vector<PlacedRoom> rooms;

    // Grid helpers
    void initGrid(int w, int h);
    CellType getGrid(int x, int y) const;
    void setGrid(int x, int y, CellType type);
    bool isAreaClear(int x, int y, int w, int h) const;

    // Room placement
    void placeRoomWalls(const PlacedRoom& room);
    bool tryPlaceRoom(const RoomDef& def, int belowY);

    // Landing bay (special layout with hangar door + landing pads)
    void placeLandingBay();

    // Room furnishing
    void furnishRoom(const PlacedRoom& room);
    void furnishStorage(const PlacedRoom& room);
    void furnishCommand(const PlacedRoom& room);
    void furnishMessHall(const PlacedRoom& room);
    void furnishEngineering(const PlacedRoom& room);
    void furnishMedbay(const PlacedRoom& room);
    void furnishCrewQuarters(const PlacedRoom& room);
    void furnishRefinery(const PlacedRoom& room);

    // Corridor connection
    void connectAllRooms();
    void connectRooms(const PlacedRoom& a, const PlacedRoom& b);
    void carveCorridorH(int x1, int x2, int y);
    void carveCorridorV(int y1, int y2, int x);

    // Door placement pass
    void placeDoors();

    // Turret placement on exterior walls
    void placeTurrets();

    // Connectivity validation
    bool validateConnectivity();
    void emergencyConnect(const PlacedRoom& from, const PlacedRoom& to);

    // Finalization
    void trimAndCommit(StationMap& map);

    // Utility
    int randInt(int minVal, int maxVal);
};

} // namespace ssm
