#pragma once

#include <entt/fwd.hpp>

namespace dreadcast::ecs {

void movement_system(entt::registry &registry, float fixedDt);

} // namespace dreadcast::ecs
