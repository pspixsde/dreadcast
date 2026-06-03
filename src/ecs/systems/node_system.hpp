#pragma once

#include <entt/fwd.hpp>

namespace dreadcast::ecs {

/// Drives `NodeSpawner` entities (Dreg spawners) and ticks active `EnemySpeedBuff` timers.
/// Wakes a Node when the player enters its trigger radius (bursting Dregs), sustains spawns while
/// the player lingers in the enlarged active area, and returns it to dormant (regenerating health)
/// after the player leaves for `dormantDelay` seconds.
void node_system(entt::registry &registry, float fixedDt, entt::entity playerEntity);

} // namespace dreadcast::ecs
