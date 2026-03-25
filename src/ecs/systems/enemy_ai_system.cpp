#include "ecs/systems/enemy_ai_system.hpp"

#include <algorithm>
#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/iso_utils.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

namespace {

Rectangle wallRect(const Transform &t, const Wall &w) {
    return {t.position.x - w.halfW, t.position.y - w.halfH, w.halfW * 2.0F, w.halfH * 2.0F};
}

bool segmentIntersectsRect(Vector2 from, Vector2 to, Rectangle rect) {
    // Liang-Barsky segment-vs-AABB test in 2D.
    const float EPS = 1.0e-6F;
    Vector2 d{to.x - from.x, to.y - from.y};

    float t0 = 0.0F;
    float t1 = 1.0F;

    auto updateAxis = [&](float start, float delta, float minB, float maxB) -> bool {
        if (std::fabs(delta) < EPS) {
            // Parallel to the slab: must be within bounds to intersect.
            return start >= minB && start <= maxB;
        }
        const float invD = 1.0F / delta;
        float tNear = (minB - start) * invD;
        float tFar = (maxB - start) * invD;
        if (tNear > tFar) {
            std::swap(tNear, tFar);
        }
        t0 = std::max(t0, tNear);
        t1 = std::min(t1, tFar);
        return t0 <= t1;
    };

    if (!updateAxis(from.x, d.x, rect.x, rect.x + rect.width)) {
        return false;
    }
    if (!updateAxis(from.y, d.y, rect.y, rect.y + rect.height)) {
        return false;
    }
    return true;
}

bool hasLineOfSight(entt::registry &registry, Vector2 from, Vector2 to) {
    const auto walls = registry.view<Wall, Transform>();
    for (const auto w : walls) {
        const auto &t = registry.get<Transform>(w);
        const auto &wall = registry.get<Wall>(w);
        if (segmentIntersectsRect(from, to, wallRect(t, wall))) {
            return false;
        }
    }
    return true;
}

} // namespace

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

        const bool hasLOS = hasLineOfSight(registry, transform.position, playerT.position);
        ag.agitated = (dist <= agRange) && hasLOS;
        ag.calmDownTimer = ag.agitated ? 0.0F : ag.calmDownTimer + fixedDt;

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
