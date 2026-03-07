#include "client/ui/ui_hotbar.h"
#include "client/renderer.h"

namespace ssm {

// --- UIHotbarSlot ---

UIHotbarSlot::UIHotbarSlot(float x, float y, float size, CellType type)
    : cellType(type)
{
    this->x = x;
    this->y = y;
    this->width = size;
    this->height = size;
}

void UIHotbarSlot::render(Renderer& renderer) {
    if (!visible) return;

    // Background
    float bgAlpha = hovered ? 0.8f : 0.6f;
    renderer.drawUIRect(x, y, width, height, 0.15f, 0.15f, 0.2f, bgAlpha);

    // Cell type color swatch (inset)
    float inset = 6.0f;
    glm::vec4 color = Renderer::getCellTypeColor(cellType);
    renderer.drawUIRect(x + inset, y + inset, width - inset * 2, height - inset * 2,
                        color.r, color.g, color.b, 1.0f);

    // Selection outline
    if (selected) {
        renderer.drawUIOutline(x, y, width, height, 2.0f, 1.0f, 1.0f, 1.0f);
    } else if (hovered) {
        renderer.drawUIOutline(x, y, width, height, 1.0f, 0.7f, 0.7f, 0.7f);
    }
}

bool UIHotbarSlot::onMouseDown(float px, float py, int button) {
    if (button == 0) return true; // consumed, parent handles selection
    return false;
}

// --- UIHotbar ---

void UIHotbar::init(float windowWidth, float windowHeight) {
    slots.clear();
    slots.reserve(SLOT_COUNT);

    float totalWidth = SLOT_COUNT * SLOT_SIZE + (SLOT_COUNT - 1) * SLOT_PADDING;
    float startX = (windowWidth - totalWidth) / 2.0f;
    float startY = windowHeight - SLOT_SIZE - 10.0f;

    this->x = startX - SLOT_PADDING;
    this->y = startY - SLOT_PADDING;
    this->width = totalWidth + SLOT_PADDING * 2;
    this->height = SLOT_SIZE + SLOT_PADDING * 2;

    for (int i = 0; i < SLOT_COUNT; i++) {
        float sx = startX + i * (SLOT_SIZE + SLOT_PADDING);
        UIHotbarSlot slot(sx, startY, SLOT_SIZE, slotTypes[i]);
        slot.selected = (i == 0);
        slots.push_back(std::move(slot));
    }

    selectedIndex = 0;
    selectedType = slotTypes[0];
}

void UIHotbar::selectSlot(int index) {
    if (index < 0 || index >= SLOT_COUNT) return;
    slots[selectedIndex].selected = false;
    selectedIndex = index;
    slots[selectedIndex].selected = true;
    selectedType = slotTypes[selectedIndex];
}

void UIHotbar::render(Renderer& renderer) {
    if (!visible) return;

    // Draw background panel
    renderer.drawUIRect(x, y, width, height, 0.1f, 0.1f, 0.15f, 0.7f);

    // Draw slots
    for (auto& slot : slots) {
        slot.render(renderer);
    }
}

bool UIHotbar::onMouseDown(float px, float py, int button) {
    if (!visible || button != 0) return false;

    // Check which slot was clicked
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (slots[i].hitTest(px, py)) {
            selectSlot(i);
            return true;
        }
    }

    // Clicked on hotbar background — still consume
    if (hitTest(px, py)) return true;
    return false;
}

void UIHotbar::updatePosition(float windowWidth, float windowHeight) {
    float totalWidth = SLOT_COUNT * SLOT_SIZE + (SLOT_COUNT - 1) * SLOT_PADDING;
    float startX = (windowWidth - totalWidth) / 2.0f;
    float startY = windowHeight - SLOT_SIZE - 10.0f;

    this->x = startX - SLOT_PADDING;
    this->y = startY - SLOT_PADDING;
    this->width = totalWidth + SLOT_PADDING * 2;
    this->height = SLOT_SIZE + SLOT_PADDING * 2;

    for (int i = 0; i < SLOT_COUNT; i++) {
        slots[i].x = startX + i * (SLOT_SIZE + SLOT_PADDING);
        slots[i].y = startY;
    }
}

} // namespace ssm
