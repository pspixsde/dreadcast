#include "ecs/systems/input_system.hpp"

#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/iso_utils.hpp"
#include "core/types.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

void input_system(entt::registry &registry, const InputManager &input, const Camera2D &camera) {
    const Vector2 isoMouse = GetScreenToWorld2D(input.mousePosition(), camera);
    const Vector2 worldMouse = dreadcast::isoToWorld(isoMouse);

    const auto view = registry.view<Player, Transform, Velocity, Facing>();
    for (const auto entity : view) {
        auto &transform = view.get<Transform>(entity);
        auto &vel = view.get<Velocity>(entity);
        auto &facing = view.get<Facing>(entity);

        Vec2 move{0.0F, 0.0F};
        if (input.isKeyHeld(KEY_W) || input.isKeyHeld(KEY_UP)) {
            move.y -= 1.0F;
        }
        if (input.isKeyHeld(KEY_S) || input.isKeyHeld(KEY_DOWN)) {
            move.y += 1.0F;
        }
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) {
            move.x -= 1.0F;
        }
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) {
            move.x += 1.0F;
        }

        move = Vec2Normalize(move);
        if (Vec2Length(move) < 0.001F) {
            vel.value.x = 0.0F;
            vel.value.y = 0.0F;
        } else {
            const Vec2 worldDir = Vec2Normalize(dreadcast::isoToWorld(move));
            const Vec2 isoDir = dreadcast::worldToIso(worldDir);
            const float isoLen = Vec2Length(isoDir);
            const float scale =
                (isoLen > 0.001F) ? (config::PLAYER_MOVE_SPEED / isoLen) : 0.0F;
            vel.value.x = worldDir.x * scale;
            vel.value.y = worldDir.y * scale;
        }

        const float dx = worldMouse.x - transform.position.x;
        const float dy = worldMouse.y - transform.position.y;
        transform.rotation = std::atan2f(dy, dx);
        facing.dir = dreadcast::facingFromAngle(transform.rotation);
    }
}

} // namespace dreadcast::ecs
