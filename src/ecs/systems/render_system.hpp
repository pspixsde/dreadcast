#pragma once

#include <entt/fwd.hpp>

#include <raylib.h>

namespace dreadcast {
class ResourceManager;
}

namespace dreadcast::ecs {

void render_system(entt::registry &registry, const Font &font, ResourceManager &resources);

} // namespace dreadcast::ecs
