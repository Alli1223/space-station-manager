#pragma once

#include <vector>
#include <memory>

namespace ssm {

class Renderer;

class UIElement {
public:
    float x = 0, y = 0;
    float width = 0, height = 0;
    bool visible = true;
    bool hovered = false;

    virtual ~UIElement() = default;

    // Non-copyable (has unique_ptr children), move OK
    UIElement(const UIElement&) = delete;
    UIElement& operator=(const UIElement&) = delete;
    UIElement(UIElement&&) = default;
    UIElement& operator=(UIElement&&) = default;

    UIElement() = default;

    // Render this element (screen-space coords already set)
    virtual void render(Renderer& renderer);

    // Hit test: returns true if point is inside this element
    virtual bool hitTest(float px, float py) const;

    // Input events (return true if consumed)
    virtual bool onMouseDown(float px, float py, int button);
    virtual bool onMouseUp(float px, float py, int button);
    virtual void onHover(float px, float py);
    virtual void onHoverExit();

    // Add/remove children
    void addChild(std::unique_ptr<UIElement> child);
    const std::vector<std::unique_ptr<UIElement>>& getChildren() const { return children; }

protected:
    std::vector<std::unique_ptr<UIElement>> children;

    // Dispatch input to children (reverse order for top-most first)
    bool dispatchMouseDown(float px, float py, int button);
    bool dispatchMouseUp(float px, float py, int button);
    void dispatchHover(float px, float py);
};

} // namespace ssm
