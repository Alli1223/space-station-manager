#pragma once

#include "shared/game_object.h"
#include <string>

namespace ssm {

class Wall : public GameObject {
public:
    Wall();
    Wall(uint32_t id, float x, float y);
};

class Floor : public GameObject {
public:
    Floor();
    Floor(uint32_t id, float x, float y);
};

class Door : public GameObject {
public:
    DoorState state = DoorState::CLOSED;
    float openAmount = 0.0f;    // 0.0 = fully closed, 1.0 = fully open
    uint8_t orientation = 0;     // 0 = vertical door (walls left+right, opens horizontally)
                                 // 1 = horizontal door (walls up+down, opens vertically)
    bool isAirlock = false;      // true = manual E-press toggle only (docking bay doors)
    bool isHangarDoor = false;   // true = controlled by ShipManager for ship entry/exit

    Door();
    Door(uint32_t id, float x, float y);

    bool isSolid() const { return openAmount < 0.5f; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class Terminal : public GameObject {
public:
    uint32_t linkedDoorId = 0; // the door this terminal controls

    Terminal();
    Terminal(uint32_t id, float x, float y, uint32_t linkedDoorId);

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class DockingCollar : public GameObject {
public:
    uint32_t dockedShipId = 0; // 0 = no ship docked
    uint32_t linkedDoorId = 0; // airlock door created on this collar

    DockingCollar();
    DockingCollar(uint32_t id, float x, float y);

    bool hasShip() const { return dockedShipId != 0; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class Ship : public GameObject {
public:
    ShipClass shipClass = ShipClass::MEDIUM;
    ShipState state = ShipState::APPROACHING;
    uint32_t targetCollarId = 0;
    float fuel = 0.0f;
    float maxFuel = 100.0f;
    float food = 0.0f;
    float maxFood = 50.0f;
    float stateTimer = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    // Cargo manifest: what the ship is carrying to unload
    uint8_t metalToUnload = 5;
    uint8_t oreToUnload = 3;
    uint8_t crystalsToUnload = 0;
    uint8_t plasmaToUnload = 0;
    uint8_t totalMetal = 5;       // original amount (for HUD rendering)
    uint8_t totalOre = 3;
    uint8_t totalCrystals = 0;
    uint8_t totalPlasma = 0;
    uint8_t passengers = 0;   // stored for future use
    float patienceTimer = 60.0f;
    float maxPatience = 60.0f;
    float departAngle = 0.0f; // angle ship arrived from (used for departure direction)

    bool isImpatient() const { return patienceTimer <= 0.0f; }

    Ship();
    Ship(uint32_t id, float x, float y);

    void applyClassStats(ShipClass sc);

    bool needsFuel() const { return fuel < maxFuel; }
    bool needsFood() const { return food < maxFood; }
    bool isResupplied() const { return fuel >= maxFuel && food >= maxFood; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class Cargo : public GameObject {
public:
    CargoType cargoType = CargoType::NONE;
    uint8_t quantity = 1;
    uint32_t carriedByPlayerId = 0; // 0 = on ground
    uint32_t tetheredToPlayerId = 0; // 0 = not tethered
    uint8_t tetherOrder = 0; // position in chain: 0 = closest to player

    Cargo();
    Cargo(uint32_t id, float x, float y, CargoType cargoType, uint8_t quantity);

    bool isOnGround() const { return carriedByPlayerId == 0; }
    bool isTethered() const { return tetheredToPlayerId != 0; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class Player : public GameObject {
public:
    std::string name;
    float speed = PLAYER_SPEED;
    uint32_t carryingCargoId = 0; // 0 = not carrying
    float dx = 0.0f; // pending input
    float dy = 0.0f;
    bool interacting = false;
    bool sprinting = false;      // client wants to sprint
    uint8_t colorIndex = 0;

    // Stamina system
    float stamina = 100.0f;      // current stamina (0–100)
    float maxStamina = 100.0f;

    // Turret control
    uint32_t operatingTurretId = 0; // 0 = not in a turret

    Player();
    Player(uint32_t id, float x, float y, const std::string& name);

    bool isCarrying() const { return carryingCargoId != 0; }
    bool isInTurret() const { return operatingTurretId != 0; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class Turret : public GameObject {
public:
    TurretType turretType = TurretType::ENERGY;
    float aimAngle = 0.0f;       // current aim direction (radians)
    float facingAngle = 0.0f;    // base direction toward space (radians)
    uint32_t operatorId = 0;     // player controlling this turret (0 = unmanned)
    int16_t ammo = -1;           // -1 = unlimited (energy)
    int16_t maxAmmo = -1;
    float fireCooldown = 0.0f;

    Turret();
    Turret(uint32_t id, float x, float y, TurretType type, float facing);

    bool isOccupied() const { return operatorId != 0; }
    bool hasAmmo() const { return ammo < 0 || ammo > 0; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class Projectile : public GameObject {
public:
    ProjectileOwner owner = ProjectileOwner::STATION;
    float velX = 0.0f;
    float velY = 0.0f;
    float damage = 10.0f;
    float lifetime = PROJECTILE_LIFETIME;
    uint32_t sourceId = 0; // turret or enemy that fired

    Projectile();
    Projectile(uint32_t id, float x, float y, ProjectileOwner owner,
               float vx, float vy, float dmg, uint32_t source);

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

class EnemyShip : public GameObject {
public:
    float health = ENEMY_HP;
    float maxHealth = ENEMY_HP;
    float speed = ENEMY_SPEED;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float fireCooldown = 0.0f;
    int waveIndex = 0;

    EnemyShip();
    EnemyShip(uint32_t id, float x, float y);

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

} // namespace ssm
