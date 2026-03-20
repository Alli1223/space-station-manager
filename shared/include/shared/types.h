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

// Economy constants
constexpr int32_t STARTING_MONEY = 500;
constexpr int32_t SMALL_SHIP_PAYOUT = 100;
constexpr int32_t MEDIUM_SHIP_PAYOUT = 200;
constexpr int32_t LARGE_SHIP_PAYOUT = 400;
constexpr int32_t ANGRY_DEPART_PENALTY = -50;

// Player color palette (8 distinct colors)
constexpr float PLAYER_COLORS[8][3] = {
    {0.2f, 0.6f, 1.0f},   // blue
    {1.0f, 0.4f, 0.3f},   // red
    {0.3f, 0.9f, 0.4f},   // green
    {1.0f, 0.8f, 0.2f},   // yellow
    {0.9f, 0.5f, 1.0f},   // purple
    {1.0f, 0.6f, 0.2f},   // orange
    {0.3f, 0.9f, 0.9f},   // cyan
    {1.0f, 0.7f, 0.7f},   // pink
};

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
    TURRET,
    PROJECTILE,
    ENEMY_SHIP,
};

enum class CargoType : uint8_t {
    NONE = 0,
    METAL,
    ORE,
    FUEL,
    FOOD,
    CRYSTALS,
    PLASMA,
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
    uint8_t oreToUnload;
    uint8_t crystalsToUnload;
    uint8_t plasmaToUnload;
    uint8_t passengers;
    float patienceTime;
};

inline const ShipClassStats& getShipClassStats(ShipClass sc) {
    //                                  w       h      speed  dock   fuel    food   met ore cry pla pass patience
    static const ShipClassStats small  {  96.0f, 128.0f, 70.0f, 1.5f,  60.0f, 30.0f, 3, 2, 1, 0,  2, 90.0f };
    static const ShipClassStats medium { 128.0f, 160.0f, 50.0f, 2.0f, 100.0f, 50.0f, 5, 3, 1, 1,  5, 70.0f };
    static const ShipClassStats large  { 160.0f, 192.0f, 30.0f, 3.0f, 160.0f, 80.0f, 8, 5, 2, 1, 10, 50.0f };
    switch (sc) {
        case ShipClass::SMALL:  return small;
        case ShipClass::MEDIUM: return medium;
        case ShipClass::LARGE:  return large;
    }
    return medium;
}

// Turret types
enum class TurretType : uint8_t {
    ENERGY = 0,
    KINETIC,
};

enum class ProjectileOwner : uint8_t {
    STATION = 0,
    ENEMY,
};

struct TurretStats {
    float range;
    float halfAngle;    // radians
    float damage;
    float fireRate;     // shots per second
    float projSpeed;
    int maxAmmo;        // -1 = unlimited
};

inline const TurretStats& getTurretStats(TurretType tt) {
    static const TurretStats energy  { 400.0f, 0.524f, 25.0f, 2.0f, 600.0f, -1 };  // ~30 deg
    static const TurretStats kinetic { 700.0f, 1.396f, 10.0f, 5.0f, 800.0f, 20 };  // ~80 deg
    switch (tt) {
        case TurretType::ENERGY:  return energy;
        case TurretType::KINETIC: return kinetic;
    }
    return energy;
}

// Combat constants
constexpr int16_t WALL_MAX_HP = 50;
constexpr float ENEMY_WAVE_INITIAL_DELAY = 180.0f;
constexpr float ENEMY_WAVE_INTERVAL = 120.0f;
constexpr float ENEMY_SPEED = 40.0f;
constexpr float ENEMY_FIRE_RATE = 0.8f;  // shots per second
constexpr float ENEMY_DAMAGE = 15.0f;
constexpr float ENEMY_HP = 100.0f;
constexpr float ENEMY_FIRE_RANGE = 300.0f;
constexpr float PROJECTILE_SIZE = 6.0f;
constexpr float PROJECTILE_LIFETIME = 3.0f;

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
