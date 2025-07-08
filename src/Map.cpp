#include "Map.h"

GameMap::GameMap() {
    // Initialize 20x20 area with walls around the edge
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            if (x == 0 || y == 0 || x == 19 || y == 19)
                set(x, y, Cell::Wall);
            else
                set(x, y, Cell::Walkable);
        }
    }
    // Example door inside
    set(5, 5, Cell::DoorClosed);
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
