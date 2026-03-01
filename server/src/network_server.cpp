#include "server/network_server.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <errno.h>
#endif

namespace ssm {

bool NetworkServer::start(uint16_t port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCK) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Allow address reuse
    int opt = 1;
#ifdef _WIN32
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        closeSocket(listenSocket);
        return false;
    }

    if (listen(listenSocket, 10) != 0) {
        std::cerr << "Failed to listen" << std::endl;
        closeSocket(listenSocket);
        return false;
    }

    setNonBlocking(listenSocket);

    std::cout << "Server listening on port " << port << std::endl;
    return true;
}

void NetworkServer::stop() {
    for (auto& client : clients) {
        if (client.connected) {
            closeSocket(client.socket);
        }
    }
    clients.clear();
    if (listenSocket != INVALID_SOCK) {
        closeSocket(listenSocket);
        listenSocket = INVALID_SOCK;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void NetworkServer::poll() {
    acceptNewClients();
    readFromClients();
}

void NetworkServer::acceptNewClients() {
    while (true) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int addrLen = sizeof(clientAddr);
#else
        socklen_t addrLen = sizeof(clientAddr);
#endif
        SocketType clientSock = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSock == INVALID_SOCK) break; // no more pending connections

        setNonBlocking(clientSock);

        ClientConnection conn;
        conn.socket = clientSock;
        conn.connected = true;
        clients.push_back(conn);

        uint32_t idx = static_cast<uint32_t>(clients.size() - 1);
        std::cout << "Client " << idx << " connected" << std::endl;
    }
}

void NetworkServer::readFromClients() {
    for (uint32_t i = 0; i < clients.size(); i++) {
        auto& client = clients[i];
        if (!client.connected) continue;

        uint8_t buf[4096];
#ifdef _WIN32
        int bytesRead = recv(client.socket, (char*)buf, sizeof(buf), 0);
#else
        int bytesRead = recv(client.socket, buf, sizeof(buf), 0);
#endif

        if (bytesRead > 0) {
            client.recvBuffer.insert(client.recvBuffer.end(), buf, buf + bytesRead);
            processMessages(i);
        } else if (bytesRead == 0) {
            // Clean disconnect
            std::cout << "Client " << i << " disconnected" << std::endl;
            client.connected = false;
            closeSocket(client.socket);
            if (onDisconnect) onDisconnect(i);
        } else {
            // Check for actual error vs would-block
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                std::cout << "Client " << i << " error: " << err << std::endl;
                client.connected = false;
                closeSocket(client.socket);
                if (onDisconnect) onDisconnect(i);
            }
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cout << "Client " << i << " error: " << errno << std::endl;
                client.connected = false;
                closeSocket(client.socket);
                if (onDisconnect) onDisconnect(i);
            }
#endif
        }
    }
}

void NetworkServer::processMessages(uint32_t clientIndex) {
    auto& client = clients[clientIndex];
    MessageType msgType;
    ByteBuffer payload;

    while (decodeMessage(client.recvBuffer, msgType, payload)) {
        switch (msgType) {
            case MessageType::MSG_JOIN: {
                std::string name = payload.readString();
                if (onJoin) onJoin(clientIndex, name);
                break;
            }
            case MessageType::MSG_INPUT: {
                float dx = payload.readFloat();
                float dy = payload.readFloat();
                bool interact = payload.readBool();
                if (onInput) onInput(clientIndex, dx, dy, interact);
                break;
            }
            case MessageType::MSG_CELL_EDIT: {
                int16_t gx = payload.readI16();
                int16_t gy = payload.readI16();
                CellType ct = static_cast<CellType>(payload.readU8());
                if (onCellEdit) onCellEdit(clientIndex, gx, gy, ct);
                break;
            }
            case MessageType::MSG_CARGO_PLACE: {
                float tx = payload.readFloat();
                float ty = payload.readFloat();
                if (onCargoPlace) onCargoPlace(clientIndex, tx, ty);
                break;
            }
            case MessageType::MSG_TETHER_TOGGLE: {
                uint32_t cargoId = payload.readU32();
                if (onTetherToggle) onTetherToggle(clientIndex, cargoId);
                break;
            }
            default:
                break;
        }
    }
}

void NetworkServer::sendToClient(uint32_t clientIndex, MessageType type, const ByteBuffer& payload) {
    if (clientIndex >= clients.size() || !clients[clientIndex].connected) return;

    auto encoded = encodeMessage(type, payload);
#ifdef _WIN32
    send(clients[clientIndex].socket, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
    send(clients[clientIndex].socket, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
}

void NetworkServer::broadcast(MessageType type, const ByteBuffer& payload) {
    auto encoded = encodeMessage(type, payload);
    for (auto& client : clients) {
        if (!client.connected) continue;
#ifdef _WIN32
        send(client.socket, (const char*)encoded.data(), static_cast<int>(encoded.size()), 0);
#else
        send(client.socket, encoded.data(), encoded.size(), MSG_NOSIGNAL);
#endif
    }
}

ClientConnection* NetworkServer::getClient(uint32_t index) {
    if (index >= clients.size()) return nullptr;
    return &clients[index];
}

void NetworkServer::setNonBlocking(SocketType sock) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

void NetworkServer::closeSocket(SocketType sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

} // namespace ssm
