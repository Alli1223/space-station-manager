#ifndef MAP_H
#define MAP_H

#include <unordered_map>
#include <vector>
#include <cstdint>

struct Cell {
    enum Type { Empty, Walkable, DoorClosed, DoorOpen } type{Empty};
    static bool isWalkable(Type t) {
        return t == Walkable || t == DoorOpen;
    }
};

struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

namespace std {
    template<>
    struct hash<Point> {
        size_t operator()(const Point& p) const noexcept {
            return ((size_t)p.x << 32) ^ (size_t)p.y;
        }
    };
}

class GameMap {
public:
    GameMap();
    const Cell& get(int x, int y) const;
    void set(int x, int y, Cell::Type type);
private:
    std::unordered_map<Point, Cell> cells_;
};

#endif // MAP_H
