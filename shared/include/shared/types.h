#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ssm {

// Grid cell size in world units
constexpr float CELL_SIZE = 32.0f;

// Server tick rate
constexpr float SERVER_TICK_RATE = 20.0f;
constexpr float SERVER_TICK_INTERVAL = 1.0f / SERVER_TICK_RATE;

// Default network port
constexpr uint16_t DEFAULT_PORT = 7777;

// Player constants
constexpr float PLAYER_SPEED = 100.0f; // pixels per second
constexpr float PLAYER_SIZE = 20.0f;
constexpr float INTERACTION_RANGE = 60.0f;

// Ship constants
constexpr float SHIP_SPAWN_INTERVAL = 60.0f; // seconds

enum class GameObjectType : uint8_t {
    NONE = 0,
    PLAYER,
    WALL,
    FLOOR,
    DOOR,
    TERMINAL,
    DOCKING_COLLAR,
    SHIP,
    CARGO,
};

enum class CargoType : uint8_t {
    NONE = 0,
    METAL,
    WOOD,
    FUEL,
    FOOD,
};

enum class DoorState : uint8_t {
    CLOSED = 0,
    OPEN,
};

enum class ShipState : uint8_t {
    APPROACHING = 0,
    DOCKING,
    DOCKED,
    UNLOADING,
    WAITING_RESUPPLY,
    DEPARTING,
    GONE,
};

enum class ShipClass : uint8_t {
    SMALL = 0,
    MEDIUM,
    LARGE,
};

struct ShipClassStats {
    float width;
    float height;
    float approachSpeed;
    float dockingTime;
    float maxFuel;
    float maxFood;
    uint8_t metalToUnload;
    uint8_t woodToUnload;
    uint8_t passengers;
};

inline const ShipClassStats& getShipClassStats(ShipClass sc) {
    static const ShipClassStats small  { 48.0f,  72.0f, 70.0f, 1.5f,  60.0f, 30.0f, 3, 2,  2 };
    static const ShipClassStats medium { 64.0f,  96.0f, 50.0f, 2.0f, 100.0f, 50.0f, 5, 3,  5 };
    static const ShipClassStats large  { 96.0f, 128.0f, 30.0f, 3.0f, 160.0f, 80.0f, 8, 5, 10 };
    switch (sc) {
        case ShipClass::SMALL:  return small;
        case ShipClass::MEDIUM: return medium;
        case ShipClass::LARGE:  return large;
    }
    return medium;
}

// Simple byte buffer for serialization
class ByteBuffer {
public:
    std::vector<uint8_t> data;
    size_t readPos = 0;

    void writeU8(uint8_t v);
    void writeU16(uint16_t v);
    void writeI16(int16_t v);
    void writeU32(uint32_t v);
    void writeFloat(float v);
    void writeString(const std::string& s);
    void writeBool(bool v);

    uint8_t readU8();
    uint16_t readU16();
    int16_t readI16();
    uint32_t readU32();
    float readFloat();
    std::string readString();
    bool readBool();

    size_t size() const { return data.size(); }
    size_t remaining() const { return readPos < data.size() ? data.size() - readPos : 0; }
    const uint8_t* ptr() const { return data.data(); }
    void clear() { data.clear(); readPos = 0; }
};

} // namespace ssm
