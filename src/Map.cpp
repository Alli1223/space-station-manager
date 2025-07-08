#include "Map.h"

GameMap::GameMap() {
    // Initialize 20x20 walkable grid at origin
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            set(x, y, Cell::Walkable);
        }
    }
}

const Cell& GameMap::get(int x, int y) const {
    static Cell empty{Cell::Empty};
    auto it = cells_.find(Point{x,y});
    if (it == cells_.end()) return empty;
    return it->second;
}

void GameMap::set(int x, int y, Cell::Type type) {
    cells_[Point{x,y}] = Cell{type};
}
