#pragma once

#include "shared/types.h"

namespace ssm {

class GameObject {
public:
    uint32_t id = 0;
    GameObjectType type = GameObjectType::NONE;
    float x = 0.0f;
    float y = 0.0f;
    float width = CELL_SIZE;
    float height = CELL_SIZE;
    bool active = true;

    GameObject() = default;
    GameObject(GameObjectType type, uint32_t id, float x, float y);
    virtual ~GameObject() = default;

    virtual void serialize(ByteBuffer& buf) const;
    virtual void deserialize(ByteBuffer& buf);

    bool overlaps(float ox, float oy, float ow, float oh) const;

    // Static factory: create the right derived type from a buffer
    static GameObject* createFromBuffer(ByteBuffer& buf);
};

} // namespace ssm
