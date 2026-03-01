#pragma once

#include "shared/protocol.h"
#include "shared/map.h"
#include "shared/game_object.h"
#include <vector>
#include <string>
#include <functional>

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

class NetworkClient {
public:
    using WelcomeCallback = std::function<void(uint32_t playerId, const StationMap& map)>;
    using StateCallback = std::function<void(const std::vector<GameObject*>& objects)>;
    using EventCallback = std::function<void(EventType event, uint32_t objectId)>;
    using MapUpdateCallback = std::function<void(int16_t gridX, int16_t gridY, CellType cellType)>;
    using StationStateCallback = std::function<void(int32_t money)>;

    WelcomeCallback onWelcome;
    StateCallback onState;
    EventCallback onEvent;
    MapUpdateCallback onMapUpdate;
    StationStateCallback onStationState;

    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    void poll();

    void sendJoin(const std::string& playerName);
    void sendInput(float dx, float dy, bool interact);
    void sendCellEdit(int16_t gridX, int16_t gridY, CellType cellType);
    void sendCargoPlace(float targetX, float targetY);
    void sendTetherToggle(uint32_t cargoId);

    bool isConnected() const { return connected; }

private:
    SocketType sock = INVALID_SOCK;
    bool connected = false;
    std::vector<uint8_t> recvBuffer;

    void processMessages();
    void setNonBlocking(SocketType s);
    void closeSocket(SocketType s);
};

} // namespace ssm
