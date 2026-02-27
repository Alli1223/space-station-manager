#include "client/ui/ui_manager.h"
#include "client/renderer.h"
#include <GLFW/glfw3.h>

namespace ssm {

bool UIManager::processInput(GLFWwindow* window) {
    clickConsumed = false;

    // Get mouse position
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    mouseX = static_cast<float>(mx);
    mouseY = static_cast<float>(my);

    // Mouse button states
    bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // Hover update (always)
    for (auto& elem : elements) {
        if (elem->visible && elem->hitTest(mouseX, mouseY)) {
            elem->onHover(mouseX, mouseY);
        } else if (elem->hovered) {
            elem->onHoverExit();
        }
    }

    // Left click edge detection (press)
    if (leftDown && !prevMouseLeft) {
        // Reverse order for top-most first
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            auto& elem = *it;
            if (elem->visible && elem->hitTest(mouseX, mouseY)) {
                if (elem->onMouseDown(mouseX, mouseY, GLFW_MOUSE_BUTTON_LEFT)) {
                    clickConsumed = true;
                    break;
                }
            }
        }
    }

    // Left click release
    if (!leftDown && prevMouseLeft) {
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            auto& elem = *it;
            if (elem->visible && elem->hitTest(mouseX, mouseY)) {
                elem->onMouseUp(mouseX, mouseY, GLFW_MOUSE_BUTTON_LEFT);
            }
        }
    }

    // Right click edge detection
    if (rightDown && !prevMouseRight) {
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            auto& elem = *it;
            if (elem->visible && elem->hitTest(mouseX, mouseY)) {
                if (elem->onMouseDown(mouseX, mouseY, GLFW_MOUSE_BUTTON_RIGHT)) {
                    clickConsumed = true;
                    break;
                }
            }
        }
    }

    prevMouseLeft = leftDown;
    prevMouseRight = rightDown;

    return clickConsumed;
}

void UIManager::render(Renderer& renderer) {
    renderer.beginScreenSpace();
    for (auto& elem : elements) {
        elem->render(renderer);
    }
    renderer.endScreenSpace();
}

void UIManager::addElement(std::unique_ptr<UIElement> element) {
    elements.push_back(std::move(element));
}

UIElement* UIManager::getElement(size_t index) {
    if (index >= elements.size()) return nullptr;
    return elements[index].get();
}

} // namespace ssm
