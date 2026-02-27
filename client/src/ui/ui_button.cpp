#include "client/ui/ui_button.h"
#include "client/renderer.h"

namespace ssm {

UIButton::UIButton(float x, float y, float w, float h, ClickCallback cb)
    : onClick(std::move(cb))
{
    this->x = x;
    this->y = y;
    this->width = w;
    this->height = h;
}

void UIButton::render(Renderer& renderer) {
    if (!visible) return;

    float r, g, b;
    if (pressed) {
        r = pressR; g = pressG; b = pressB;
    } else if (hovered) {
        r = hoverR; g = hoverG; b = hoverB;
    } else if (toggled) {
        r = hoverR; g = hoverG; b = hoverB;
    } else {
        r = colorR; g = colorG; b = colorB;
    }

    renderer.drawUIRect(x, y, width, height, r, g, b, 0.9f);

    // Draw outline when toggled
    if (toggled) {
        renderer.drawUIOutline(x, y, width, height, 2.0f, 1.0f, 1.0f, 1.0f);
    }

    // Render children (e.g., label icons)
    UIElement::render(renderer);
}

bool UIButton::onMouseDown(float px, float py, int button) {
    if (button == 0) { // left click
        pressed = true;
        return true;
    }
    return false;
}

bool UIButton::onMouseUp(float px, float py, int button) {
    if (button == 0 && pressed) {
        pressed = false;
        if (onClick) onClick();
        return true;
    }
    return false;
}

} // namespace ssm
