#include "ecs/systems/movement_system.hpp"

#include <entt/entt.hpp>

#include "config.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

void movement_system(entt::registry &registry, float fixedDt) {
    const auto view = registry.view<Transform, Velocity>();
    for (const auto entity : view) {
        auto &transform = view.get<Transform>(entity);
        auto &vel = view.get<Velocity>(entity);
        transform.position.x += vel.value.x * fixedDt;
        transform.position.y += vel.value.y * fixedDt;

        if (registry.all_of<Enemy>(entity)) {
            vel.value.x *= config::ENEMY_VELOCITY_DAMPING;
            vel.value.y *= config::ENEMY_VELOCITY_DAMPING;
        }
    }
}

} // namespace dreadcast::ecs
