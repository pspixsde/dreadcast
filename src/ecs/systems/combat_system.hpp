#pragma once

#include <entt/fwd.hpp>

#include <raylib.h>

namespace dreadcast {

class InputManager;
class ResourceManager;

namespace ecs {

/// Left-click ranged attack (frame input). Uses `ChamberState` (ammo / reload); `noManaFlashTimer`
/// is unused (kept for call-site compatibility).
/// @return true if a shot was fired (projectiles spawned).
[[nodiscard]] bool combat_player_ranged(entt::registry &registry, const InputManager &input,
                                          const Camera2D &camera, entt::entity player,
                                          float &noManaFlashTimer, Vector2 aimScreenPos,
                                          ResourceManager *resources);

/// Right-click melee: cooldown, damage, knockback (fixed timestep).
void combat_player_melee(entt::registry &registry, const InputManager &input, entt::entity player,
                         float fixedDt);

} // namespace ecs
} // namespace dreadcast
