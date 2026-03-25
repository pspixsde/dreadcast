#include "ecs/systems/death_system.hpp"

#include <vector>

#include <entt/entt.hpp>

#include "config.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

void death_system(entt::registry &registry, entt::entity player, int *enemiesSlainOut) {
    std::vector<entt::entity> toDestroy{};

    const auto dead = registry.view<Health>();
    for (const auto e : dead) {
        const auto &hp = registry.get<Health>(e);
        if (hp.current > 0.0F) {
            continue;
        }

        if (registry.valid(player) && e == player) {
            continue;
        }

        if (registry.all_of<Enemy>(e)) {
            if (enemiesSlainOut != nullptr) {
                ++(*enemiesSlainOut);
            }
            if (registry.all_of<Transform, Sprite>(e)) {
                const auto &t = registry.get<Transform>(e);
                const auto &s = registry.get<Sprite>(e);
                (void)s;
                const auto shard = registry.create();
                registry.emplace<Transform>(shard, Transform{t.position, 0.0F});
                registry.emplace<Velocity>(shard, Velocity{});
                registry.emplace<Sprite>(shard, Sprite{{120, 200, 255, 255}, 14.0F, 14.0F});
                registry.emplace<ManaShard>(shard, ManaShard{config::MANA_SHARD_AMOUNT});
            }
        }
        toDestroy.push_back(e);
    }

    for (const auto e : toDestroy) {
        if (registry.valid(e)) {
            registry.destroy(e);
        }
    }
}

} // namespace dreadcast::ecs
