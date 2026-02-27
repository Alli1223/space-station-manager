#include "client/ui/ui_element.h"
#include "client/renderer.h"

namespace ssm {

void UIElement::render(Renderer& renderer) {
    if (!visible) return;
    for (auto& child : children) {
        child->render(renderer);
    }
}

bool UIElement::hitTest(float px, float py) const {
    return visible && px >= x && px < x + width && py >= y && py < y + height;
}

bool UIElement::onMouseDown(float px, float py, int button) {
    return dispatchMouseDown(px, py, button);
}

bool UIElement::onMouseUp(float px, float py, int button) {
    return dispatchMouseUp(px, py, button);
}

void UIElement::onHover(float px, float py) {
    hovered = true;
    dispatchHover(px, py);
}

void UIElement::onHoverExit() {
    hovered = false;
    for (auto& child : children) {
        child->onHoverExit();
    }
}

void UIElement::addChild(std::unique_ptr<UIElement> child) {
    children.push_back(std::move(child));
}

bool UIElement::dispatchMouseDown(float px, float py, int button) {
    // Reverse order so topmost children get priority
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        auto& child = *it;
        if (child->visible && child->hitTest(px, py)) {
            if (child->onMouseDown(px, py, button)) return true;
        }
    }
    return false;
}

bool UIElement::dispatchMouseUp(float px, float py, int button) {
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        auto& child = *it;
        if (child->visible && child->hitTest(px, py)) {
            if (child->onMouseUp(px, py, button)) return true;
        }
    }
    return false;
}

void UIElement::dispatchHover(float px, float py) {
    for (auto& child : children) {
        if (child->visible && child->hitTest(px, py)) {
            child->onHover(px, py);
        } else if (child->hovered) {
            child->onHoverExit();
        }
    }
}

} // namespace ssm
