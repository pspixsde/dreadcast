#include "ecs/systems/enemy_ai_system.hpp"

#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/iso_utils.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

void enemy_ai_system(entt::registry &registry, float fixedDt) {
    entt::entity playerEntity = entt::null;
    for (const auto p : registry.view<Player>()) {
        playerEntity = p;
        break;
    }
    if (playerEntity == entt::null || !registry.all_of<Transform>(playerEntity)) {
        return;
    }
    const auto &playerT = registry.get<Transform>(playerEntity);

    const auto view = registry.view<Enemy, EnemyAI, Transform, Sprite, Facing, Agitation>();
    for (const auto e : view) {
        auto &ai = registry.get<EnemyAI>(e);
        auto &transform = registry.get<Transform>(e);
        auto &facing = registry.get<Facing>(e);
        auto &ag = registry.get<Agitation>(e);
        const auto &enemySprite = registry.get<Sprite>(e);

        const float dx = playerT.position.x - transform.position.x;
        const float dy = playerT.position.y - transform.position.y;
        const float distSq = dx * dx + dy * dy;
        const float dist = std::sqrt(distSq);
        const float agRange = ag.agitationRange > 0.0F ? ag.agitationRange : config::IMP_AGITATION_RANGE;

        if (dist <= agRange) {
            ag.agitated = true;
            ag.calmDownTimer = 0.0F;
        } else {
            ag.calmDownTimer += fixedDt;
            const float delay = ag.calmDownDelay > 0.0F ? ag.calmDownDelay : config::ENEMY_CALM_DOWN_DELAY;
            if (ag.calmDownTimer >= delay) {
                ag.agitated = false;
                ag.calmDownTimer = delay;
            }
        }

        if (!ag.agitated) {
            continue;
        }

        if (dist > 0.001F) {
            transform.rotation = std::atan2f(dy, dx);
            facing.dir = dreadcast::facingFromAngle(transform.rotation);
        }

        ai.shootTimer -= fixedDt;
        if (ai.shootTimer > 0.0F) {
            continue;
        }
        ai.shootTimer = ai.shootCooldown;

        if (dist <= ai.minShootRange) {
            continue;
        }

        const Vector2 dir = {dx / dist, dy / dist};
        const float halfDiag =
            std::sqrt(enemySprite.width * enemySprite.width +
                      enemySprite.height * enemySprite.height) *
            0.5F;
        const float spawnDist = halfDiag + config::PROJECTILE_RADIUS + 2.0F;
        const auto proj = registry.create();
        registry.emplace<Transform>(
            proj,
            Transform{{transform.position.x + dir.x * spawnDist, transform.position.y + dir.y * spawnDist},
                      std::atan2f(dir.y, dir.x)});
        registry.emplace<Velocity>(
            proj, Velocity{{dir.x * config::ENEMY_PROJECTILE_SPEED,
                            dir.y * config::ENEMY_PROJECTILE_SPEED}});
        registry.emplace<Sprite>(proj, Sprite{{255, 120, 90, 255}, 9.0F, 9.0F});
        registry.emplace<Projectile>(proj,
                                       Projectile{config::PROJECTILE_DAMAGE,
                                                  config::ENEMY_PROJECTILE_SPEED,
                                                  config::ENEMY_PROJECTILE_MAX_RANGE,
                                                  0.0F,
                                                  dir,
                                                  false});
    }
}

} // namespace dreadcast::ecs
