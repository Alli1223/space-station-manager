#pragma once

#include "shared/protocol.h"
#include "shared/map.h"
#include <vector>
#include <functional>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using SocketType = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using SocketType = int;
    #define INVALID_SOCK (-1)
#endif

namespace ssm {

struct ClientConnection {
    SocketType socket = INVALID_SOCK;
    uint32_t playerId = 0;
    std::vector<uint8_t> recvBuffer;
    bool connected = true;
};

class NetworkServer {
public:
    using JoinCallback = std::function<void(uint32_t clientIndex, const std::string& name)>;
    using InputCallback = std::function<void(uint32_t clientIndex, float dx, float dy, bool interact)>;
    using DisconnectCallback = std::function<void(uint32_t clientIndex)>;
    using CellEditCallback = std::function<void(uint32_t clientIndex, int16_t gridX, int16_t gridY, CellType cellType)>;
    using CargoPlaceCallback = std::function<void(uint32_t clientIndex, float targetX, float targetY)>;

    JoinCallback onJoin;
    InputCallback onInput;
    DisconnectCallback onDisconnect;
    CellEditCallback onCellEdit;
    CargoPlaceCallback onCargoPlace;

    bool start(uint16_t port);
    void stop();
    void poll();    // Accept new connections and read data (non-blocking)
    void sendToClient(uint32_t clientIndex, MessageType type, const ByteBuffer& payload);
    void broadcast(MessageType type, const ByteBuffer& payload);

    ClientConnection* getClient(uint32_t index);
    size_t clientCount() const { return clients.size(); }

private:
    SocketType listenSocket = INVALID_SOCK;
    std::vector<ClientConnection> clients;

    void acceptNewClients();
    void readFromClients();
    void processMessages(uint32_t clientIndex);
    void setNonBlocking(SocketType sock);
    void closeSocket(SocketType sock);
};

} // namespace ssm
