#pragma once

#include "shared/types.h"
#include "shared/map.h"
#include <vector>
#include <memory>

namespace ssm {

enum class MessageType : uint8_t {
    // Client -> Server
    MSG_JOIN = 1,
    MSG_INPUT = 2,
    MSG_INTERACT = 3,
    MSG_CELL_EDIT = 4,
    MSG_CARGO_PLACE = 5,
    MSG_TETHER_TOGGLE = 6,

    // Server -> Client
    MSG_WELCOME = 10,
    MSG_STATE = 11,
    MSG_EVENT = 12,
    MSG_PLAYER_LEFT = 13,
    MSG_MAP_UPDATE = 14,
    MSG_STATION_STATE = 15,
};

enum class EventType : uint8_t {
    SHIP_ARRIVED = 1,
    SHIP_DEPARTED = 2,
    DOOR_TOGGLED = 3,
    CARGO_PICKUP = 4,
    CARGO_DROP = 5,
    SHIP_ANGRY_DEPART = 6,
};

// Encode a message with length prefix: [uint16 length][uint8 type][payload]
std::vector<uint8_t> encodeMessage(MessageType type, const ByteBuffer& payload);

// Try to decode a message from raw bytes. Returns true if a complete message was decoded.
// Consumes the bytes from the front of `incoming`.
bool decodeMessage(std::vector<uint8_t>& incoming, MessageType& outType, ByteBuffer& outPayload);

// --- Message builders ---

// Client -> Server
ByteBuffer buildJoinMessage(const std::string& playerName);
ByteBuffer buildInputMessage(float dx, float dy, bool interact, bool sprint = false);
ByteBuffer buildCellEditMessage(int16_t gridX, int16_t gridY, CellType cellType);
ByteBuffer buildCargoPlaceMessage(float targetX, float targetY);
ByteBuffer buildTetherToggleMessage(uint32_t cargoId);

// Server -> Client
ByteBuffer buildWelcomeMessage(uint32_t playerId, const std::vector<uint8_t>& mapData,
                                int mapWidth, int mapHeight, int originX, int originY);
ByteBuffer buildStateMessage(const std::vector<class GameObject*>& objects);
ByteBuffer buildEventMessage(EventType event, uint32_t objectId);
ByteBuffer buildPlayerLeftMessage(uint32_t playerId);
ByteBuffer buildMapUpdateMessage(int16_t gridX, int16_t gridY, CellType cellType);
ByteBuffer buildStationStateMessage(int32_t money);

} // namespace ssm
