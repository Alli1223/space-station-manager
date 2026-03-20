#include "client/network_client.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <errno.h>
#endif

namespace ssm {

bool NetworkClient::connect(const std::string& host, uint16_t port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        closeSocket(sock);
        sock = INVALID_SOCK;
        return false;
    }

    setNonBlocking(sock);
    connected = true;
    std::cout << "Connected to server " << host << ":" << port << std::endl;
    return true;
}

void NetworkClient::disconnect() {
    if (sock != INVALID_SOCK) {
        closeSocket(sock);
        sock = INVALID_SOCK;
    }
    connected = false;
#ifdef _WIN32
    WSACleanup();
#endif
}

void NetworkClient::poll() {
    if (!connected) return;

    uint8_t buf[65536];
#ifdef _WIN32
    int bytesRead = recv(sock, (char*)buf, sizeof(buf), 0);
#else
    int bytesRead = recv(sock, buf, sizeof(buf), 0);
#endif

    if (bytesRead > 0) {
        recvBuffer.insert(recvBuffer.end(), buf, buf + bytesRead);
        processMessages();
    } else if (bytesRead == 0) {
        std::cout << "Disconnected from server" << std::endl;
        connected = false;
        closeSocket(sock);
        sock = INVALID_SOCK;
    } else {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            connected = false;
            closeSocket(sock);
            sock = INVALID_SOCK;
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            connected = false;
            closeSocket(sock);
            sock = INVALID_SOCK;
        }
#endif
    }
}

void NetworkClient::processMessages() {
    MessageType msgType;
    ByteBuffer payload;

    while (decodeMessage(recvBuffer, msgType, payload)) {
        switch (msgType) {
            case MessageType::MSG_WELCOME: {
                uint32_t playerId = payload.readU32();
                uint16_t mapW = payload.readU16();
                uint16_t mapH = payload.readU16();
                int16_t ox = payload.readI16();
                int16_t oy = payload.readI16();
                uint32_t mapSize = payload.readU32();
                std::vector<uint8_t> mapData(mapSize);
                for (uint32_t i = 0; i < mapSize; i++) {
                    mapData[i] = payload.readU8();
                }
                StationMap map;
                map.loadFromRawData(mapData, mapW, mapH, ox, oy);
                if (onWelcome) onWelcome(playerId, map);
                break;
            }
            case MessageType::MSG_STATE: {
                uint16_t count = payload.readU16();
                std::vector<GameObject*> objects;
                objects.reserve(count);
                for (uint16_t i = 0; i < count; i++) {
                    GameObject* obj = GameObject::createFromBuffer(payload);
                    if (obj) objects.push_back(obj);
                }
                if (onState) onState(objects);
                // Note: caller takes ownership of objects
                break;
            }
            case MessageType::MSG_EVENT: {
                EventType event = static_cast<EventType>(payload.readU8());
                uint32_t objectId = payload.readU32();
                if (onEvent) onEvent(event, objectId);
                break;
            }
            case MessageType::MSG_MAP_UPDATE: {
                int16_t gx = payload.readI16();
                int16_t gy = payload.readI16();
                CellType ct = static_cast<CellType>(payload.readU8());
                if (onMapUpdate) onMapUpdate(gx, gy, ct);
                break;
            }
            case MessageType::MSG_STATION_STATE: {
                int32_t money = static_cast<int32_t>(payload.readU32());
                if (onStationState) onStationState(money);
                break;
            }
            default:
                break;
        }
    }
}

void NetworkClient::sendJoin(const std::string& playerName) {
    if (!connected) return;
    auto payload = buildJoinMessage(playerName);
    auto encoded = encodeMessage(MessageType::MSG_JOIN, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::sendInput(float dx, float dy, bool interact, bool sprint) {
    if (!connected) return;
    auto payload = buildInputMessage(dx, dy, interact, sprint);
    auto encoded = encodeMessage(MessageType::MSG_INPUT, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::sendCellEdit(int16_t gridX, int16_t gridY, CellType cellType) {
    if (!connected) return;
    auto payload = buildCellEditMessage(gridX, gridY, cellType);
    auto encoded = encodeMessage(MessageType::MSG_CELL_EDIT, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::sendCargoPlace(float targetX, float targetY) {
    if (!connected) return;
    auto payload = buildCargoPlaceMessage(targetX, targetY);
    auto encoded = encodeMessage(MessageType::MSG_CARGO_PLACE, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::sendTetherToggle(uint32_t cargoId) {
    if (!connected) return;
    auto payload = buildTetherToggleMessage(cargoId);
    auto encoded = encodeMessage(MessageType::MSG_TETHER_TOGGLE, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::sendTurretAim(float angle, bool firing) {
    if (!connected) return;
    auto payload = buildTurretAimMessage(angle, firing);
    auto encoded = encodeMessage(MessageType::MSG_TURRET_AIM, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::sendTurretExit() {
    if (!connected) return;
    auto payload = buildTurretExitMessage();
    auto encoded = encodeMessage(MessageType::MSG_TURRET_EXIT, payload);
#ifdef _WIN32
    send(sock, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(sock, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkClient::setNonBlocking(SocketType s) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

void NetworkClient::closeSocket(SocketType s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

} // namespace ssm
