#pragma once

#include <entt/fwd.hpp>

#include <raylib.h>

namespace dreadcast {

class InputManager;

namespace ecs {

/// Reads input: WASD moves player, mouse aims rotation. Sets Velocity on player.
void input_system(entt::registry &registry, const InputManager &input, const Camera2D &camera,
                  Vector2 aimScreenPos);

} // namespace ecs
} // namespace dreadcast
