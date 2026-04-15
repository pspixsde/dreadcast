#pragma once

#include <entt/fwd.hpp>

namespace dreadcast::ecs {

/// Removes dead non-player entities; awards XP on enemy death (no mana drops).
/// Increments `*enemiesSlainOut` once per destroyed `Enemy` when non-null.
void death_system(entt::registry &registry, entt::entity player, int *enemiesSlainOut = nullptr);

} // namespace dreadcast::ecs
