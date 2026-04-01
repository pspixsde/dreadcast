#pragma once

#include <entt/fwd.hpp>

#include <raylib.h>

namespace dreadcast {
class ResourceManager;
}

namespace dreadcast::ecs {

void render_system(entt::registry &registry, const Font &font, ResourceManager &resources);
bool visible_to_player(entt::registry &registry, Vector2 playerPos, Vector2 targetPos, float radius);

} // namespace dreadcast::ecs
