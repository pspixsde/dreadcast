#pragma once

#include <entt/fwd.hpp>

namespace dreadcast {
struct InventoryState;
}

namespace dreadcast::ecs {

void enemy_ai_system(entt::registry &registry, float fixedDt, entt::entity playerEntity,
                     dreadcast::InventoryState *inventory);

} // namespace dreadcast::ecs