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

    Door();
    Door(uint32_t id, float x, float y);

    bool isSolid() const { return state == DoorState::CLOSED; }
    void toggle() { state = (state == DoorState::CLOSED) ? DoorState::OPEN : DoorState::CLOSED; }

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
    uint8_t woodToUnload = 3;
    uint8_t totalMetal = 5;   // original amount (for HUD rendering)
    uint8_t totalWood = 3;    // original amount (for HUD rendering)
    uint8_t passengers = 0;   // stored for future use

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

    Cargo();
    Cargo(uint32_t id, float x, float y, CargoType cargoType, uint8_t quantity);

    bool isOnGround() const { return carriedByPlayerId == 0; }

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

    Player();
    Player(uint32_t id, float x, float y, const std::string& name);

    bool isCarrying() const { return carryingCargoId != 0; }

    void serialize(ByteBuffer& buf) const override;
    void deserialize(ByteBuffer& buf) override;
};

} // namespace ssm
