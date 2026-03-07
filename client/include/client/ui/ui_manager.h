#pragma once

#include "client/ui/ui_element.h"
#include <vector>
#include <memory>

struct GLFWwindow;

namespace ssm {

class Renderer;

class UIManager {
public:
    // Process mouse input, returns true if UI consumed the click
    bool processInput(GLFWwindow* window);

    // Render all UI elements
    void render(Renderer& renderer);

    // Add a top-level element
    void addElement(std::unique_ptr<UIElement> element);

    // Did the UI consume the last mouse click?
    bool wasClickConsumed() const { return clickConsumed; }

    // Get raw element pointer by index (for external wiring)
    UIElement* getElement(size_t index);

private:
    std::vector<std::unique_ptr<UIElement>> elements;
    bool clickConsumed = false;

    // Mouse state tracking
    bool prevMouseLeft = false;
    bool prevMouseRight = false;
    float mouseX = 0, mouseY = 0;
};

} // namespace ssm
