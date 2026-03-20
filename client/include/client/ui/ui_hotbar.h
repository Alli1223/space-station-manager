#pragma once

#include "client/ui/ui_element.h"
#include "shared/map.h"
#include <vector>

namespace ssm {

class UIHotbarSlot : public UIElement {
public:
    CellType cellType = CellType::WALL;
    bool selected = false;

    UIHotbarSlot() = default;
    UIHotbarSlot(float x, float y, float size, CellType type);

    void render(Renderer& renderer) override;
    bool onMouseDown(float px, float py, int button) override;
};

class UIHotbar : public UIElement {
public:
    static constexpr int SLOT_COUNT = 10;
    static constexpr float SLOT_SIZE = 48.0f;
    static constexpr float SLOT_PADDING = 4.0f;

    UIHotbar() = default;

    void init(float windowWidth, float windowHeight);

    // Select a slot by index (0-7)
    void selectSlot(int index);

    // Get the currently selected cell type
    CellType getSelectedType() const { return selectedType; }

    void render(Renderer& renderer) override;
    bool onMouseDown(float px, float py, int button) override;

    // Update position when window resizes
    void updatePosition(float windowWidth, float windowHeight);

private:
    CellType selectedType = CellType::WALL;
    int selectedIndex = 0;
    std::vector<UIHotbarSlot> slots;

    static constexpr CellType slotTypes[SLOT_COUNT] = {
        CellType::WALL,
        CellType::FLOOR,
        CellType::DOOR,
        CellType::TERMINAL,
        CellType::LANDING_PAD,
        CellType::HANGAR_DOOR,
        CellType::STORAGE,
        CellType::AIRLOCK,
        CellType::SPAWN_POINT,
        CellType::REFINERY
    };
};

} // namespace ssm
