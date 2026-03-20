#include "shared/game_objects.h"

namespace ssm {

// --- Wall ---
Wall::Wall() { type = GameObjectType::WALL; }
Wall::Wall(uint32_t id, float x, float y)
    : GameObject(GameObjectType::WALL, id, x, y) {}

// --- Floor ---
Floor::Floor() { type = GameObjectType::FLOOR; }
Floor::Floor(uint32_t id, float x, float y)
    : GameObject(GameObjectType::FLOOR, id, x, y) {}

// --- Door ---
Door::Door() { type = GameObjectType::DOOR; }
Door::Door(uint32_t id, float x, float y)
    : GameObject(GameObjectType::DOOR, id, x, y) {}

void Door::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU8(static_cast<uint8_t>(state));
    buf.writeFloat(openAmount);
    buf.writeU8(orientation);
    buf.writeU8(isAirlock ? 1 : 0);
    buf.writeU8(isHangarDoor ? 1 : 0);
}
void Door::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    state = static_cast<DoorState>(buf.readU8());
    openAmount = buf.readFloat();
    orientation = buf.readU8();
    isAirlock = (buf.readU8() != 0);
    if (buf.remaining() > 0) {
        isHangarDoor = (buf.readU8() != 0);
    }
}

// --- Terminal ---
Terminal::Terminal() { type = GameObjectType::TERMINAL; }
Terminal::Terminal(uint32_t id, float x, float y, uint32_t linkedDoorId)
    : GameObject(GameObjectType::TERMINAL, id, x, y), linkedDoorId(linkedDoorId) {
    width = CELL_SIZE;
    height = CELL_SIZE;
}

void Terminal::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU32(linkedDoorId);
}
void Terminal::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    linkedDoorId = buf.readU32();
}

// --- DockingCollar ---
DockingCollar::DockingCollar() { type = GameObjectType::DOCKING_COLLAR; }
DockingCollar::DockingCollar(uint32_t id, float x, float y)
    : GameObject(GameObjectType::DOCKING_COLLAR, id, x, y) {}

void DockingCollar::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU32(dockedShipId);
    buf.writeU32(linkedDoorId);
}
void DockingCollar::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    dockedShipId = buf.readU32();
    linkedDoorId = buf.readU32();
}

// --- Ship ---
Ship::Ship() {
    type = GameObjectType::SHIP;
    applyClassStats(ShipClass::MEDIUM);
}
Ship::Ship(uint32_t id, float x, float y)
    : GameObject(GameObjectType::SHIP, id, x, y) {
    applyClassStats(ShipClass::MEDIUM);
}

void Ship::applyClassStats(ShipClass sc) {
    shipClass = sc;
    const auto& stats = getShipClassStats(sc);
    width = stats.width;
    height = stats.height;
    maxFuel = stats.maxFuel;
    maxFood = stats.maxFood;
    metalToUnload = stats.metalToUnload;
    oreToUnload = stats.oreToUnload;
    crystalsToUnload = stats.crystalsToUnload;
    plasmaToUnload = stats.plasmaToUnload;
    totalMetal = stats.metalToUnload;
    totalOre = stats.oreToUnload;
    totalCrystals = stats.crystalsToUnload;
    totalPlasma = stats.plasmaToUnload;
    passengers = stats.passengers;
    maxPatience = stats.patienceTime;
    patienceTimer = maxPatience;
}

void Ship::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU8(static_cast<uint8_t>(shipClass));
    buf.writeU8(static_cast<uint8_t>(state));
    buf.writeU32(targetCollarId);
    buf.writeFloat(fuel);
    buf.writeFloat(maxFuel);
    buf.writeFloat(food);
    buf.writeFloat(maxFood);
    buf.writeFloat(stateTimer);
    buf.writeFloat(targetX);
    buf.writeFloat(targetY);
    buf.writeU8(metalToUnload);
    buf.writeU8(oreToUnload);
    buf.writeU8(crystalsToUnload);
    buf.writeU8(plasmaToUnload);
    buf.writeU8(totalMetal);
    buf.writeU8(totalOre);
    buf.writeU8(totalCrystals);
    buf.writeU8(totalPlasma);
    buf.writeU8(passengers);
    buf.writeFloat(patienceTimer);
    buf.writeFloat(maxPatience);
    buf.writeFloat(departAngle);
}
void Ship::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    shipClass = static_cast<ShipClass>(buf.readU8());
    state = static_cast<ShipState>(buf.readU8());
    targetCollarId = buf.readU32();
    fuel = buf.readFloat();
    maxFuel = buf.readFloat();
    food = buf.readFloat();
    maxFood = buf.readFloat();
    stateTimer = buf.readFloat();
    targetX = buf.readFloat();
    targetY = buf.readFloat();
    metalToUnload = buf.readU8();
    oreToUnload = buf.readU8();
    crystalsToUnload = buf.readU8();
    plasmaToUnload = buf.readU8();
    totalMetal = buf.readU8();
    totalOre = buf.readU8();
    totalCrystals = buf.readU8();
    totalPlasma = buf.readU8();
    passengers = buf.readU8();
    patienceTimer = buf.readFloat();
    maxPatience = buf.readFloat();
    departAngle = buf.readFloat();
}

// --- Cargo ---
Cargo::Cargo() {
    type = GameObjectType::CARGO;
    width = 16.0f;
    height = 16.0f;
}
Cargo::Cargo(uint32_t id, float x, float y, CargoType cargoType, uint8_t quantity)
    : GameObject(GameObjectType::CARGO, id, x, y), cargoType(cargoType), quantity(quantity) {
    width = 16.0f;
    height = 16.0f;
}

void Cargo::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU8(static_cast<uint8_t>(cargoType));
    buf.writeU8(quantity);
    buf.writeU32(carriedByPlayerId);
    buf.writeU32(tetheredToPlayerId);
    buf.writeU8(tetherOrder);
}
void Cargo::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    cargoType = static_cast<CargoType>(buf.readU8());
    quantity = buf.readU8();
    carriedByPlayerId = buf.readU32();
    tetheredToPlayerId = buf.readU32();
    tetherOrder = buf.readU8();
}

// --- Player ---
Player::Player() {
    type = GameObjectType::PLAYER;
    width = PLAYER_SIZE;
    height = PLAYER_SIZE;
}
Player::Player(uint32_t id, float x, float y, const std::string& name)
    : GameObject(GameObjectType::PLAYER, id, x, y), name(name) {
    width = PLAYER_SIZE;
    height = PLAYER_SIZE;
}

void Player::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeString(name);
    buf.writeFloat(speed);
    buf.writeU32(carryingCargoId);
    buf.writeU8(colorIndex);
    buf.writeFloat(stamina);
    buf.writeFloat(maxStamina);
    buf.writeU32(operatingTurretId);
}
void Player::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    name = buf.readString();
    speed = buf.readFloat();
    carryingCargoId = buf.readU32();
    colorIndex = buf.readU8();
    stamina = buf.readFloat();
    maxStamina = buf.readFloat();
    if (buf.remaining() > 0) {
        operatingTurretId = buf.readU32();
    }
}

// --- Turret ---
static constexpr float TURRET_SIZE = CELL_SIZE * 1.5f;

Turret::Turret() {
    type = GameObjectType::TURRET;
    width = TURRET_SIZE;
    height = TURRET_SIZE;
}
Turret::Turret(uint32_t id, float x, float y, TurretType ttype, float facing)
    : GameObject(GameObjectType::TURRET, id, x, y), turretType(ttype),
      aimAngle(facing), facingAngle(facing) {
    width = TURRET_SIZE;
    height = TURRET_SIZE;
    const auto& stats = getTurretStats(ttype);
    ammo = static_cast<int16_t>(stats.maxAmmo);
    maxAmmo = static_cast<int16_t>(stats.maxAmmo);
}

void Turret::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU8(static_cast<uint8_t>(turretType));
    buf.writeFloat(aimAngle);
    buf.writeFloat(facingAngle);
    buf.writeU32(operatorId);
    buf.writeI16(ammo);
    buf.writeI16(maxAmmo);
}
void Turret::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    turretType = static_cast<TurretType>(buf.readU8());
    aimAngle = buf.readFloat();
    facingAngle = buf.readFloat();
    operatorId = buf.readU32();
    ammo = buf.readI16();
    maxAmmo = buf.readI16();
}

// --- Projectile ---
Projectile::Projectile() {
    type = GameObjectType::PROJECTILE;
    width = PROJECTILE_SIZE;
    height = PROJECTILE_SIZE;
}
Projectile::Projectile(uint32_t id, float x, float y, ProjectileOwner own,
                       float vx, float vy, float dmg, uint32_t source)
    : GameObject(GameObjectType::PROJECTILE, id, x, y), owner(own),
      velX(vx), velY(vy), damage(dmg), sourceId(source) {
    width = PROJECTILE_SIZE;
    height = PROJECTILE_SIZE;
}

void Projectile::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeU8(static_cast<uint8_t>(owner));
    buf.writeFloat(velX);
    buf.writeFloat(velY);
    buf.writeFloat(damage);
    buf.writeFloat(lifetime);
    buf.writeU32(sourceId);
}
void Projectile::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    owner = static_cast<ProjectileOwner>(buf.readU8());
    velX = buf.readFloat();
    velY = buf.readFloat();
    damage = buf.readFloat();
    lifetime = buf.readFloat();
    sourceId = buf.readU32();
}

// --- EnemyShip ---
EnemyShip::EnemyShip() {
    type = GameObjectType::ENEMY_SHIP;
    width = 64.0f;
    height = 48.0f;
}
EnemyShip::EnemyShip(uint32_t id, float x, float y)
    : GameObject(GameObjectType::ENEMY_SHIP, id, x, y) {
    width = 64.0f;
    height = 48.0f;
}

void EnemyShip::serialize(ByteBuffer& buf) const {
    GameObject::serialize(buf);
    buf.writeFloat(health);
    buf.writeFloat(maxHealth);
    buf.writeFloat(targetX);
    buf.writeFloat(targetY);
    buf.writeU8(static_cast<uint8_t>(waveIndex));
}
void EnemyShip::deserialize(ByteBuffer& buf) {
    GameObject::deserialize(buf);
    health = buf.readFloat();
    maxHealth = buf.readFloat();
    targetX = buf.readFloat();
    targetY = buf.readFloat();
    waveIndex = buf.readU8();
}

} // namespace ssm
