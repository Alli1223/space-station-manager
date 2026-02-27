#include "shared/game_object.h"
#include "shared/game_objects.h"
#include <cstring>

namespace ssm {

// --- ByteBuffer implementation ---

void ByteBuffer::writeU8(uint8_t v) { data.push_back(v); }
void ByteBuffer::writeU16(uint16_t v) {
    data.push_back(static_cast<uint8_t>(v >> 8));
    data.push_back(static_cast<uint8_t>(v & 0xFF));
}
void ByteBuffer::writeI16(int16_t v) {
    writeU16(static_cast<uint16_t>(v));
}
void ByteBuffer::writeU32(uint32_t v) {
    data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(v & 0xFF));
}
void ByteBuffer::writeFloat(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    writeU32(bits);
}
void ByteBuffer::writeString(const std::string& s) {
    writeU16(static_cast<uint16_t>(s.size()));
    data.insert(data.end(), s.begin(), s.end());
}
void ByteBuffer::writeBool(bool v) { writeU8(v ? 1 : 0); }

uint8_t ByteBuffer::readU8() {
    if (readPos >= data.size()) return 0;
    return data[readPos++];
}
uint16_t ByteBuffer::readU16() {
    if (readPos + 2 > data.size()) { readPos = data.size(); return 0; }
    uint16_t v = (static_cast<uint16_t>(data[readPos]) << 8) | data[readPos + 1];
    readPos += 2;
    return v;
}
int16_t ByteBuffer::readI16() {
    return static_cast<int16_t>(readU16());
}
uint32_t ByteBuffer::readU32() {
    if (readPos + 4 > data.size()) { readPos = data.size(); return 0; }
    uint32_t v = (static_cast<uint32_t>(data[readPos]) << 24) |
                 (static_cast<uint32_t>(data[readPos + 1]) << 16) |
                 (static_cast<uint32_t>(data[readPos + 2]) << 8) |
                 data[readPos + 3];
    readPos += 4;
    return v;
}
float ByteBuffer::readFloat() {
    uint32_t bits = readU32();
    float v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}
std::string ByteBuffer::readString() {
    uint16_t len = readU16();
    if (readPos + len > data.size()) { readPos = data.size(); return ""; }
    std::string s(data.begin() + readPos, data.begin() + readPos + len);
    readPos += len;
    return s;
}
bool ByteBuffer::readBool() { return readU8() != 0; }

// --- GameObject ---

GameObject::GameObject(GameObjectType type, uint32_t id, float x, float y)
    : id(id), type(type), x(x), y(y) {}

void GameObject::serialize(ByteBuffer& buf) const {
    buf.writeU8(static_cast<uint8_t>(type));
    buf.writeU32(id);
    buf.writeFloat(x);
    buf.writeFloat(y);
    buf.writeFloat(width);
    buf.writeFloat(height);
    buf.writeBool(active);
}

void GameObject::deserialize(ByteBuffer& buf) {
    // type already read by factory
    id = buf.readU32();
    x = buf.readFloat();
    y = buf.readFloat();
    width = buf.readFloat();
    height = buf.readFloat();
    active = buf.readBool();
}

bool GameObject::overlaps(float ox, float oy, float ow, float oh) const {
    return x < ox + ow && x + width > ox && y < oy + oh && y + height > oy;
}

GameObject* GameObject::createFromBuffer(ByteBuffer& buf) {
    auto objType = static_cast<GameObjectType>(buf.readU8());
    GameObject* obj = nullptr;
    switch (objType) {
        case GameObjectType::PLAYER:         obj = new Player(); break;
        case GameObjectType::WALL:           obj = new Wall(); break;
        case GameObjectType::FLOOR:          obj = new Floor(); break;
        case GameObjectType::DOOR:           obj = new Door(); break;
        case GameObjectType::TERMINAL:       obj = new Terminal(); break;
        case GameObjectType::DOCKING_COLLAR: obj = new DockingCollar(); break;
        case GameObjectType::SHIP:           obj = new Ship(); break;
        case GameObjectType::CARGO:          obj = new Cargo(); break;
        default: return nullptr;
    }
    if (obj) {
        obj->type = objType;
        obj->deserialize(buf);
    }
    return obj;
}

} // namespace ssm
