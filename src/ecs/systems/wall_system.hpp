#pragma once

#include <entt/fwd.hpp>

namespace dreadcast::ecs {

void wall_resolve_collisions(entt::registry &registry);
void unit_resolve_collisions(entt::registry &registry);
void wall_destroy_projectiles(entt::registry &registry);

} // namespace dreadcast::ecs
