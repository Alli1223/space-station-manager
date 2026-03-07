#pragma once

#include "client/ui/ui_element.h"
#include <functional>

namespace ssm {

class UIButton : public UIElement {
public:
    using ClickCallback = std::function<void()>;

    float colorR = 0.3f, colorG = 0.3f, colorB = 0.4f;
    float hoverR = 0.4f, hoverG = 0.4f, hoverB = 0.55f;
    float pressR = 0.25f, pressG = 0.25f, pressB = 0.35f;
    float textR = 1.0f, textG = 1.0f, textB = 1.0f;

    ClickCallback onClick;

    UIButton() = default;
    UIButton(float x, float y, float w, float h, ClickCallback cb = nullptr);

    void render(Renderer& renderer) override;
    bool onMouseDown(float px, float py, int button) override;
    bool onMouseUp(float px, float py, int button) override;

    // Toggle state for edit button
    bool toggled = false;

private:
    bool pressed = false;
};

} // namespace ssm
