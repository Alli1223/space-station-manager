#include "server/game_world.h"
#include "server/network_server.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <string>

static bool running = true;

void signalHandler(int) {
    running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    uint16_t port = ssm::DEFAULT_PORT;
    std::string mapFile;
    uint32_t genSeed = 0;

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--map" && i + 1 < argc) {
            mapFile = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            genSeed = static_cast<uint32_t>(std::stoul(argv[++i]));
        }
    }

    std::cout << "=== Space Station Manager Server ===" << std::endl;

    // Initialize game world
    ssm::GameWorld world;
    bool initOk;
    if (!mapFile.empty()) {
        std::cout << "Loading map from file: " << mapFile << std::endl;
        initOk = world.init(mapFile);
    } else {
        std::cout << "Generating random station..." << std::endl;
        initOk = world.initGenerated(genSeed);
    }
    if (!initOk) {
        std::cerr << "Failed to initialize game world" << std::endl;
        return 1;
    }

    // Initialize network
    ssm::NetworkServer network;

    network.onJoin = [&](uint32_t clientIndex, const std::string& name) {
        world.onPlayerJoin(clientIndex, name);

        // Find the player to get their ID
        auto& objs = world.getObjects();
        for (auto it = objs.rbegin(); it != objs.rend(); ++it) {
            if ((*it)->type == ssm::GameObjectType::PLAYER) {
                auto* player = static_cast<ssm::Player*>(*it);
                if (player->name == name) {
                    auto welcome = world.buildWelcome(player->id);
                    network.sendToClient(clientIndex, ssm::MessageType::MSG_WELCOME, welcome);
                    break;
                }
            }
        }
    };

    network.onInput = [&](uint32_t clientIndex, float dx, float dy, bool interact, bool sprint) {
        world.onPlayerInput(clientIndex, dx, dy, interact, sprint);
    };

    network.onDisconnect = [&](uint32_t clientIndex) {
        world.onPlayerDisconnect(clientIndex);
    };

    network.onCellEdit = [&](uint32_t clientIndex, int16_t gx, int16_t gy, ssm::CellType ct) {
        if (world.onCellEdit(clientIndex, gx, gy, ct)) {
            // Edit accepted — broadcast to all clients
            auto update = ssm::buildMapUpdateMessage(gx, gy, ct);
            network.broadcast(ssm::MessageType::MSG_MAP_UPDATE, update);
        }
    };

    network.onCargoPlace = [&](uint32_t clientIndex, float tx, float ty) {
        world.onCargoPlace(clientIndex, tx, ty);
    };

    network.onTetherToggle = [&](uint32_t clientIndex, uint32_t cargoId) {
        world.onTetherToggle(clientIndex, cargoId);
    };

    if (!network.start(port)) {
        std::cerr << "Failed to start network server" << std::endl;
        return 1;
    }

    // Main game loop
    auto lastTick = std::chrono::steady_clock::now();
    auto lastBroadcast = lastTick;

    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;

    while (running) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;

        // Cap dt to prevent spiral of death
        if (dt > 0.1f) dt = 0.1f;

        // Poll network (accept connections, read data)
        network.poll();

        // Update game world
        world.update(dt);

        // Broadcast state at tick rate
        float timeSinceBroadcast = std::chrono::duration<float>(now - lastBroadcast).count();
        if (timeSinceBroadcast >= ssm::SERVER_TICK_INTERVAL) {
            auto state = world.buildStateSnapshot();
            network.broadcast(ssm::MessageType::MSG_STATE, state);

            // Also broadcast station economy state
            auto stationState = ssm::buildStationStateMessage(world.getMoney());
            network.broadcast(ssm::MessageType::MSG_STATION_STATE, stationState);

            lastBroadcast = now;
        }

        // Sleep to not burn CPU (aim for ~60 internal updates/sec)
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    std::cout << "\nShutting down server..." << std::endl;
    network.stop();
    return 0;
}
