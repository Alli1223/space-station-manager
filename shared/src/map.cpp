#include "shared/map.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace ssm {

CellType StationMap::charToCell(char c) {
    switch (c) {
        case '#': return CellType::WALL;
        case '.': return CellType::FLOOR;
        case 'D': return CellType::DOOR;
        case 'T': return CellType::TERMINAL;
        case 'C': return CellType::DOCKING_COLLAR;
        case 'S': return CellType::SPAWN_POINT;
        case 'A': return CellType::AIRLOCK;
        case 'R': return CellType::STORAGE;
        default:  return CellType::EMPTY;
    }
}

char StationMap::cellToChar(CellType cell) {
    switch (cell) {
        case CellType::WALL:           return '#';
        case CellType::FLOOR:          return '.';
        case CellType::DOOR:           return 'D';
        case CellType::TERMINAL:       return 'T';
        case CellType::DOCKING_COLLAR: return 'C';
        case CellType::SPAWN_POINT:    return 'S';
        case CellType::AIRLOCK:        return 'A';
        case CellType::STORAGE:        return 'R';
        default:                       return ' ';
    }
}

bool StationMap::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open map file: " << filename << std::endl;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return loadFromString(content);
}

bool StationMap::loadFromString(const std::string& mapStr) {
    cells.clear();
    width = 0;
    height = 0;
    originX = 0;
    originY = 0;

    std::istringstream stream(mapStr);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(stream, line)) {
        // Strip trailing \r for Windows line endings
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        lines.push_back(line);
        if (static_cast<int>(line.size()) > width) {
            width = static_cast<int>(line.size());
        }
    }

    height = static_cast<int>(lines.size());
    cells.resize(width * height, CellType::EMPTY);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < static_cast<int>(lines[y].size()); x++) {
            cells[y * width + x] = charToCell(lines[y][x]);
        }
    }

    std::cout << "Map loaded: " << width << "x" << height << std::endl;
    return true;
}

CellType StationMap::getCell(int x, int y) const {
    if (!isInBounds(x, y)) return CellType::EMPTY;
    int lx = x - originX;
    int ly = y - originY;
    return cells[ly * width + lx];
}

void StationMap::setCell(int x, int y, CellType type) {
    ensureBounds(x, y);
    int lx = x - originX;
    int ly = y - originY;
    cells[ly * width + lx] = type;
}

bool StationMap::isSolid(int x, int y) const {
    CellType cell = getCell(x, y);
    // Only walls are solid. EMPTY is not solid (infinite open space).
    return cell == CellType::WALL;
}

bool StationMap::isInBounds(int x, int y) const {
    int lx = x - originX;
    int ly = y - originY;
    return lx >= 0 && lx < width && ly >= 0 && ly < height;
}

std::vector<uint8_t> StationMap::getRawData() const {
    std::vector<uint8_t> raw(cells.size());
    for (size_t i = 0; i < cells.size(); i++) {
        raw[i] = static_cast<uint8_t>(cells[i]);
    }
    return raw;
}

void StationMap::loadFromRawData(const std::vector<uint8_t>& data, int w, int h, int ox, int oy) {
    width = w;
    height = h;
    originX = ox;
    originY = oy;
    cells.resize(w * h);
    for (size_t i = 0; i < data.size() && i < cells.size(); i++) {
        cells[i] = static_cast<CellType>(data[i]);
    }
}

std::vector<std::pair<int, int>> StationMap::findCells(CellType type) const {
    std::vector<std::pair<int, int>> result;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (cells[y * width + x] == type) {
                // Return logical/world coords (with origin offset)
                result.push_back({x + originX, y + originY});
            }
        }
    }
    return result;
}

void StationMap::ensureBounds(int x, int y) {
    if (width == 0 && height == 0) {
        // First cell ever placed
        originX = x;
        originY = y;
        width = 1;
        height = 1;
        cells.resize(1, CellType::EMPTY);
        return;
    }

    int newOriginX = std::min(originX, x);
    int newOriginY = std::min(originY, y);
    int newMaxX = std::max(originX + width - 1, x);
    int newMaxY = std::max(originY + height - 1, y);
    int newWidth = newMaxX - newOriginX + 1;
    int newHeight = newMaxY - newOriginY + 1;

    if (newWidth == width && newHeight == height &&
        newOriginX == originX && newOriginY == originY) {
        return; // no resize needed
    }

    std::vector<CellType> newCells(newWidth * newHeight, CellType::EMPTY);

    // Copy old data at the correct offset
    int offsetX = originX - newOriginX;
    int offsetY = originY - newOriginY;
    for (int oy = 0; oy < height; oy++) {
        for (int ox = 0; ox < width; ox++) {
            newCells[(oy + offsetY) * newWidth + (ox + offsetX)] = cells[oy * width + ox];
        }
    }

    cells = std::move(newCells);
    originX = newOriginX;
    originY = newOriginY;
    width = newWidth;
    height = newHeight;
}

} // namespace ssm
