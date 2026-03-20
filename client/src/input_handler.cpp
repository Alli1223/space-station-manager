#include "client/input_handler.h"

namespace ssm {

void InputHandler::update(GLFWwindow* window) {
    moveX = 0.0f;
    moveY = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        moveY -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        moveY += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        moveX -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        moveX += 1.0f;

    // Interact (E / Space)
    bool currentInteract = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    interactPressed = currentInteract && !prevInteract;
    interact = interactPressed;
    prevInteract = currentInteract;

    // Sprint (Left Shift)
    sprintHeld = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

    // Mouse position
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    mouseX = static_cast<float>(mx);
    mouseY = static_cast<float>(my);

    // Mouse buttons with edge detection
    leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    leftClickPressed = leftDown && !prevLeftDown;
    rightClickPressed = rightDown && !prevRightDown;
    prevLeftDown = leftDown;
    prevRightDown = rightDown;

    // Edit mode toggle (B key)
    bool editKey = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    editTogglePressed = editKey && !prevEditKey;
    prevEditKey = editKey;

    // Number keys 1-9 and 0 (for 10th slot)
    numberKeyPressed = -1;
    for (int i = 0; i < 9; i++) {
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
            numberKeyPressed = i;
            break;
        }
    }
    if (numberKeyPressed < 0 && glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
        numberKeyPressed = 9; // 0 key = 10th slot (index 9)
    }
}

} // namespace ssm
