#pragma once

#include <entt/fwd.hpp>

namespace dreadcast {
struct InventoryState;
struct PlayerEquipmentSnapshot;
}

namespace dreadcast::ecs {

void enemy_ai_system(entt::registry &registry, float fixedDt, entt::entity playerEntity,
                     dreadcast::InventoryState *inventory,
                     const dreadcast::PlayerEquipmentSnapshot *equipSnapshot);

} // namespace dreadcast::ecs