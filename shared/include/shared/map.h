#pragma once

#include "shared/types.h"
#include <vector>
#include <string>

namespace ssm {

// Cell types in the grid
enum class CellType : uint8_t {
    EMPTY = 0,
    WALL,
    FLOOR,
    DOOR,
    TERMINAL,
    DOCKING_COLLAR,
    SPAWN_POINT,
    AIRLOCK,     // floor tile inside airlock
    STORAGE,     // storage area for cargo
};

class StationMap {
public:
    int width = 0;
    int height = 0;
    int originX = 0; // world grid X of cells[0]
    int originY = 0; // world grid Y of cells[0]
    std::vector<CellType> cells; // row-major: cells[(y-originY) * width + (x-originX)]

    bool loadFromFile(const std::string& filename);
    bool loadFromString(const std::string& mapStr);

    CellType getCell(int x, int y) const;
    void setCell(int x, int y, CellType type);

    bool isSolid(int x, int y) const;
    bool isInBounds(int x, int y) const;

    // Get raw cell data for network transmission
    std::vector<uint8_t> getRawData() const;
    void loadFromRawData(const std::vector<uint8_t>& data, int w, int h, int ox, int oy);

    // Find all cells of a given type (returns logical/world grid coords)
    std::vector<std::pair<int, int>> findCells(CellType type) const;

    static CellType charToCell(char c);
    static char cellToChar(CellType cell);

private:
    // Grow the map if (x,y) is outside current bounds
    void ensureBounds(int x, int y);
};

} // namespace ssm
