#include "ecs/systems/projectile_system.hpp"

#include <cmath>

#include <entt/entt.hpp>

#include "ecs/components.hpp"

namespace dreadcast::ecs {

void projectile_system(entt::registry &registry, float fixedDt) {
    const auto view = registry.view<Projectile, Transform, Velocity>();
    for (const auto entity : view) {
        auto &proj = view.get<Projectile>(entity);
        const auto &vel = view.get<Velocity>(entity);
        const float speed = std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
        proj.distanceTraveled += speed * fixedDt;
        if (proj.distanceTraveled >= proj.maxRange) {
            registry.destroy(entity);
        }
    }
}

} // namespace dreadcast::ecs
