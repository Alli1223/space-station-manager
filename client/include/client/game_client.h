#pragma once

// network_client.h must come before renderer.h on Windows
// so that winsock2.h is included before glad.h (APIENTRY conflict)
#include "client/network_client.h"
#include "client/renderer.h"
#include "client/input_handler.h"
#include "client/ui/ui_manager.h"
#include "client/ui/ui_button.h"
#include "client/ui/ui_hotbar.h"
#include "shared/map.h"
#include "shared/game_objects.h"
#include <vector>
#include <string>

namespace ssm {

class GameClient {
public:
    ~GameClient();

    bool init(const std::string& host, uint16_t port, const std::string& playerName);
    void run();
    void shutdown();

private:
    Renderer renderer;
    NetworkClient network;
    InputHandler input;
    StationMap map;
    UIManager uiManager;

    uint32_t localPlayerId = 0;
    std::vector<GameObject*> gameObjects;
    bool mapReceived = false;
    bool running = false;
    std::string playerName;

    // Edit mode
    bool editMode = false;
    UIButton* editButton = nullptr;   // non-owning, UIManager owns
    UIHotbar* hotbar = nullptr;       // non-owning, UIManager owns

    // Accumulated interact press (persists until sent to server)
    bool pendingInteract = false;

    void clearGameObjects();
    Player* findLocalPlayer();
    void toggleEditMode();

    // Convert screen mouse pos to grid coords
    void screenToGrid(float screenX, float screenY, int& gridX, int& gridY);

    // Convert screen mouse pos to world coords
    void screenToWorld(float screenX, float screenY, float& worldX, float& worldY);
};

} // namespace ssm
