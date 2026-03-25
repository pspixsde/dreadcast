#pragma once

#include <entt/fwd.hpp>

#include <raylib.h>

namespace dreadcast {

class InputManager;

namespace ecs {

/// Left-click ranged attack (frame input). Sets `noManaFlashTimer` when mana is too low.
void combat_player_ranged(entt::registry &registry, const InputManager &input,
                          const Camera2D &camera, entt::entity player, float &noManaFlashTimer);

/// Right-click melee: cooldown, damage, knockback (fixed timestep).
void combat_player_melee(entt::registry &registry, const InputManager &input, entt::entity player,
                         float fixedDt);

} // namespace ecs
} // namespace dreadcast
