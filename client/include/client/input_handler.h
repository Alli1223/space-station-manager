#pragma once

#include <GLFW/glfw3.h>

namespace ssm {

class InputHandler {
public:
    void update(GLFWwindow* window);

    float getMoveX() const { return moveX; }
    float getMoveY() const { return moveY; }
    bool isInteracting() const { return interact; }
    bool wasInteractPressed() const { return interactPressed; }

    // Mouse state
    float getMouseX() const { return mouseX; }
    float getMouseY() const { return mouseY; }
    bool wasLeftClickPressed() const { return leftClickPressed; }
    bool wasRightClickPressed() const { return rightClickPressed; }
    bool isLeftMouseDown() const { return leftDown; }
    bool isRightMouseDown() const { return rightDown; }

    // Edit mode toggle (B key)
    bool wasEditTogglePressed() const { return editTogglePressed; }

    // Number keys 1-8 for hotbar (returns -1 if none pressed)
    int getNumberKeyPressed() const { return numberKeyPressed; }

private:
    float moveX = 0.0f;
    float moveY = 0.0f;
    bool interact = false;
    bool interactPressed = false;
    bool prevInteract = false;

    // Mouse
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool leftDown = false;
    bool rightDown = false;
    bool leftClickPressed = false;
    bool rightClickPressed = false;
    bool prevLeftDown = false;
    bool prevRightDown = false;

    // Edit toggle
    bool editTogglePressed = false;
    bool prevEditKey = false;

    // Number keys
    int numberKeyPressed = -1;
};

} // namespace ssm
