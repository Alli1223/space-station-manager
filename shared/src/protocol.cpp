#include "shared/protocol.h"
#include "shared/game_object.h"

namespace ssm {

std::vector<uint8_t> encodeMessage(MessageType type, const ByteBuffer& payload) {
    uint16_t totalLen = static_cast<uint16_t>(1 + payload.size());
    std::vector<uint8_t> msg;
    msg.reserve(2 + totalLen);
    msg.push_back(static_cast<uint8_t>(totalLen >> 8));
    msg.push_back(static_cast<uint8_t>(totalLen & 0xFF));
    msg.push_back(static_cast<uint8_t>(type));
    msg.insert(msg.end(), payload.data.begin(), payload.data.end());
    return msg;
}

bool decodeMessage(std::vector<uint8_t>& incoming, MessageType& outType, ByteBuffer& outPayload) {
    if (incoming.size() < 3) return false;
    uint16_t totalLen = (static_cast<uint16_t>(incoming[0]) << 8) | incoming[1];
    if (incoming.size() < 2u + totalLen) return false;
    outType = static_cast<MessageType>(incoming[2]);
    outPayload.clear();
    outPayload.data.assign(incoming.begin() + 3, incoming.begin() + 2 + totalLen);
    incoming.erase(incoming.begin(), incoming.begin() + 2 + totalLen);
    return true;
}

// --- Client -> Server ---

ByteBuffer buildJoinMessage(const std::string& playerName) {
    ByteBuffer buf;
    buf.writeString(playerName);
    return buf;
}

ByteBuffer buildInputMessage(float dx, float dy, bool interact, bool sprint) {
    ByteBuffer buf;
    buf.writeFloat(dx);
    buf.writeFloat(dy);
    buf.writeBool(interact);
    buf.writeBool(sprint);
    return buf;
}

ByteBuffer buildCellEditMessage(int16_t gridX, int16_t gridY, CellType cellType) {
    ByteBuffer buf;
    buf.writeI16(gridX);
    buf.writeI16(gridY);
    buf.writeU8(static_cast<uint8_t>(cellType));
    return buf;
}

ByteBuffer buildCargoPlaceMessage(float targetX, float targetY) {
    ByteBuffer buf;
    buf.writeFloat(targetX);
    buf.writeFloat(targetY);
    return buf;
}

ByteBuffer buildTetherToggleMessage(uint32_t cargoId) {
    ByteBuffer buf;
    buf.writeU32(cargoId);
    return buf;
}

// --- Server -> Client ---

ByteBuffer buildWelcomeMessage(uint32_t playerId, const std::vector<uint8_t>& mapData,
                                int mapWidth, int mapHeight, int originX, int originY) {
    ByteBuffer buf;
    buf.writeU32(playerId);
    buf.writeU16(static_cast<uint16_t>(mapWidth));
    buf.writeU16(static_cast<uint16_t>(mapHeight));
    buf.writeI16(static_cast<int16_t>(originX));
    buf.writeI16(static_cast<int16_t>(originY));
    buf.writeU32(static_cast<uint32_t>(mapData.size()));
    for (auto b : mapData) buf.writeU8(b);
    return buf;
}

static bool isDynamic(const GameObject* obj) {
    return obj->type != GameObjectType::WALL && obj->type != GameObjectType::FLOOR;
}

ByteBuffer buildStateMessage(const std::vector<GameObject*>& objects) {
    ByteBuffer buf;
    uint16_t count = 0;
    for (auto* obj : objects) {
        if (obj && obj->active && isDynamic(obj)) count++;
    }
    buf.writeU16(count);
    for (auto* obj : objects) {
        if (obj && obj->active && isDynamic(obj)) {
            obj->serialize(buf);
        }
    }
    return buf;
}

ByteBuffer buildEventMessage(EventType event, uint32_t objectId) {
    ByteBuffer buf;
    buf.writeU8(static_cast<uint8_t>(event));
    buf.writeU32(objectId);
    return buf;
}

ByteBuffer buildPlayerLeftMessage(uint32_t playerId) {
    ByteBuffer buf;
    buf.writeU32(playerId);
    return buf;
}

ByteBuffer buildMapUpdateMessage(int16_t gridX, int16_t gridY, CellType cellType) {
    ByteBuffer buf;
    buf.writeI16(gridX);
    buf.writeI16(gridY);
    buf.writeU8(static_cast<uint8_t>(cellType));
    return buf;
}

ByteBuffer buildStationStateMessage(int32_t money) {
    ByteBuffer buf;
    buf.writeU32(static_cast<uint32_t>(money));
    return buf;
}

} // namespace ssm
