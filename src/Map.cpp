#include "Map.h"
#include <fstream>

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
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cells_.find(Point{x,y});
    if (it == cells_.end()) return empty;
    return it->second;
}

void GameMap::set(int x, int y, Cell::Type type) {
    std::lock_guard<std::mutex> lock(mutex_);
    cells_[Point{x,y}] = Cell{type};
}

void GameMap::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;
    int x, y; std::string state;
    while (in >> x >> y >> state) {
        set(x, y, Cell::fromString(state));
    }
}

void GameMap::save(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(path);
    for (const auto& kv : cells_) {
        out << kv.first.x << ' ' << kv.first.y << ' ' << Cell::toString(kv.second.type) << '\n';
    }
}
