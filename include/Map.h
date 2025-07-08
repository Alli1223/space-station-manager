#ifndef MAP_H
#define MAP_H

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

struct Cell {
    enum Type { Empty, Walkable, Wall, DoorClosed, DoorOpen } type{Empty};
    static bool isWalkable(Type t) {
        return t == Walkable || t == DoorOpen;
    }
    static const char* toString(Type t) {
        switch (t) {
            case Empty: return "empty";
            case Walkable: return "walkable";
            case Wall: return "wall";
            case DoorClosed: return "door_closed";
            case DoorOpen: return "door_open";
        }
        return "empty"; // fallback
    }
    static Type fromString(const std::string& s) {
        if (s == "walkable") return Walkable;
        if (s == "wall") return Wall;
        if (s == "door_closed") return DoorClosed;
        if (s == "door_open") return DoorOpen;
        return Empty;
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
