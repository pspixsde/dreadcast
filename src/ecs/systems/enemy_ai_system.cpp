#include "ecs/systems/enemy_ai_system.hpp"

#include <algorithm>
#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/iso_utils.hpp"
#include "core/types.hpp"
#include "ecs/components.hpp"
#include "game/items.hpp"

namespace dreadcast::ecs {

namespace {

Rectangle wallRect(const Transform &t, const Wall &w) {
    return {t.position.x - w.halfW, t.position.y - w.halfH, w.halfW * 2.0F, w.halfH * 2.0F};
}

bool segmentIntersectsRect(Vector2 from, Vector2 to, Rectangle rect) {
    const float EPS = 1.0e-6F;
    Vector2 d{to.x - from.x, to.y - from.y};

    float t0 = 0.0F;
    float t1 = 1.0F;

    auto updateAxis = [&](float start, float delta, float minB, float maxB) -> bool {
        if (std::fabs(delta) < EPS) {
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

void applySteering(Velocity &vel, Vector2 desired, float fixedDt) {
    const float k = std::min(1.0F, config::ENEMY_STEER_RATE * fixedDt);
    vel.value.x = dreadcast::Lerp(vel.value.x, desired.x, k);
    vel.value.y = dreadcast::Lerp(vel.value.y, desired.y, k);
}

/// If the direct path from `from` toward `dir` is blocked, try perpendicular directions
/// and return a modified direction that avoids the nearest wall.
Vector2 wallAvoidDir(entt::registry &registry, Vector2 from, Vector2 dir, float probeLen) {
    const Vector2 testPt{from.x + dir.x * probeLen, from.y + dir.y * probeLen};
    if (hasLineOfSight(registry, from, testPt)) {
        return dir;
    }
    const Vector2 perpL{-dir.y, dir.x};
    const Vector2 perpR{dir.y, -dir.x};
    const Vector2 testL{from.x + perpL.x * probeLen, from.y + perpL.y * probeLen};
    const Vector2 testR{from.x + perpR.x * probeLen, from.y + perpR.y * probeLen};
    const bool clearL = hasLineOfSight(registry, from, testL);
    const bool clearR = hasLineOfSight(registry, from, testR);

    if (clearL && !clearR) {
        return Vec2Normalize(Vector2{dir.x * 0.3F + perpL.x * 0.7F,
                                     dir.y * 0.3F + perpL.y * 0.7F});
    }
    if (clearR && !clearL) {
        return Vec2Normalize(Vector2{dir.x * 0.3F + perpR.x * 0.7F,
                                     dir.y * 0.3F + perpR.y * 0.7F});
    }
    if (clearL) {
        return Vec2Normalize(Vector2{dir.x * 0.3F + perpL.x * 0.7F,
                                     dir.y * 0.3F + perpL.y * 0.7F});
    }
    return dir;
}

void updateStuckDetection(EnemyAI &ai, const Transform &transform, Vector2 desiredVel,
                          float fixedDt) {
    const float dispX = transform.position.x - ai.prevPosition.x;
    const float dispY = transform.position.y - ai.prevPosition.y;
    const float disp = std::sqrt(dispX * dispX + dispY * dispY);
    const float desSpd = std::sqrt(desiredVel.x * desiredVel.x + desiredVel.y * desiredVel.y);

    if (desSpd > 5.0F && disp < config::ENEMY_STUCK_MIN_DISP) {
        ai.stuckTimer += fixedDt;
    } else {
        ai.stuckTimer = 0.0F;
    }
    ai.prevPosition = transform.position;
}

Vector2 applyStuckAvoidance(entt::registry &registry, Vector2 pos, Vector2 desiredVel,
                            float stuckTimer) {
    if (stuckTimer < config::ENEMY_STUCK_THRESHOLD) {
        return desiredVel;
    }
    const float spd = std::sqrt(desiredVel.x * desiredVel.x + desiredVel.y * desiredVel.y);
    if (spd < 0.001F) {
        return desiredVel;
    }
    const Vector2 dir{desiredVel.x / spd, desiredVel.y / spd};
    const Vector2 avoid = wallAvoidDir(registry, pos, dir, 50.0F);
    return {avoid.x * spd, avoid.y * spd};
}

} // namespace

void enemy_ai_system(entt::registry &registry, float fixedDt, entt::entity playerEntity,
                     dreadcast::InventoryState *inventory) {
    if (playerEntity == entt::null || !registry.all_of<Transform>(playerEntity)) {
        return;
    }
    const auto &playerT = registry.get<Transform>(playerEntity);

    const auto view = registry.view<Enemy, EnemyAI, Transform, Sprite, Facing, Agitation, Velocity>();
    for (const auto e : view) {
        auto &ai = registry.get<EnemyAI>(e);
        auto &transform = registry.get<Transform>(e);
        auto &facing = registry.get<Facing>(e);
        auto &ag = registry.get<Agitation>(e);
        auto &vel = registry.get<Velocity>(e);
        const auto &enemySprite = registry.get<Sprite>(e);

        if (registry.all_of<KnockbackState>(e)) {
            auto &kb = registry.get<KnockbackState>(e);
            kb.elapsed += fixedDt;
            if (kb.elapsed >= kb.duration) {
                registry.remove<KnockbackState>(e);
            } else {
                vel.value.x *= config::KNOCKBACK_FRICTION;
                vel.value.y *= config::KNOCKBACK_FRICTION;
                continue;
            }
        }

        if (registry.all_of<StunnedState>(e)) {
            vel.value.x = 0.0F;
            vel.value.y = 0.0F;
            continue;
        }

        const float dx = playerT.position.x - transform.position.x;
        const float dy = playerT.position.y - transform.position.y;
        const float distSq = dx * dx + dy * dy;
        const float dist = std::sqrt(distSq);
        const float agRange = ag.agitationRange > 0.0F ? ag.agitationRange : config::IMP_AGITATION_RANGE;

        const bool hasLOS = hasLineOfSight(registry, transform.position, playerT.position);
        const bool inAggroRange = dist <= agRange;
        const bool wantsAggro = inAggroRange && hasLOS;

        if (wantsAggro) {
            ag.calmDownTimer = 0.0F;
            ag.agitated = true;
            ag.lastKnownPlayerPos = playerT.position;
            ag.hasLastKnownPos = true;
        } else if (ag.agitated) {
            ag.calmDownTimer += fixedDt;
            const float ldx = ag.lastKnownPlayerPos.x - transform.position.x;
            const float ldy = ag.lastKnownPlayerPos.y - transform.position.y;
            const float distLastSq = ldx * ldx + ldy * ldy;
            const float distLast = std::sqrt(distLastSq);
            if (ag.hasLastKnownPos && distLast < config::ENEMY_SEEK_ARRIVE_RADIUS && !hasLOS) {
                ag.agitated = false;
                ag.calmDownTimer = 0.0F;
                ag.hasLastKnownPos = false;
            } else if (ag.calmDownTimer >= ag.calmDownDelay) {
                ag.agitated = false;
                ag.calmDownTimer = 0.0F;
                ag.hasLastKnownPos = false;
            }
        }

        if (!ag.agitated) {
            applySteering(vel, {0.0F, 0.0F}, fixedDt);
            continue;
        }

        const bool canSeePlayer = hasLOS;
        Vector2 desiredVel{0.0F, 0.0F};

        if (!canSeePlayer && ag.hasLastKnownPos) {
            const float sx = ag.lastKnownPlayerPos.x - transform.position.x;
            const float sy = ag.lastKnownPlayerPos.y - transform.position.y;
            const float sd = std::sqrt(sx * sx + sy * sy);
            if (sd > 0.001F) {
                const Vector2 toLast = {sx / sd, sy / sd};
                if (ai.type == EnemyType::Hellhound) {
                    desiredVel = {toLast.x * ai.chaseSpeed, toLast.y * ai.chaseSpeed};
                } else {
                    desiredVel = {toLast.x * config::IMP_ADVANCE_SPEED,
                                  toLast.y * config::IMP_ADVANCE_SPEED};
                }
                transform.rotation = std::atan2f(sy, sx);
                facing.dir = dreadcast::facingFromAngle(transform.rotation);
            }
            updateStuckDetection(ai, transform, desiredVel, fixedDt);
            desiredVel = applyStuckAvoidance(registry, transform.position, desiredVel, ai.stuckTimer);
            applySteering(vel, desiredVel, fixedDt);
            continue;
        }

        const float invDist = dist > 0.001F ? (1.0F / dist) : 0.0F;
        const Vector2 toPlayer = {dx * invDist, dy * invDist};

        if (dist > 0.001F) {
            transform.rotation = std::atan2f(dy, dx);
            facing.dir = dreadcast::facingFromAngle(transform.rotation);
        }

        if (ai.type == EnemyType::Hellhound) {
            ai.meleeTimer -= fixedDt;
            if (dist > ai.meleeRange) {
                desiredVel = {toPlayer.x * ai.chaseSpeed, toPlayer.y * ai.chaseSpeed};
            } else {
                desiredVel = {0.0F, 0.0F};
                if (ai.meleeTimer <= 0.0F) {
                    if (registry.all_of<Health>(playerEntity)) {
                        if (registry.all_of<ManicEffect>(playerEntity)) {
                            ai.meleeTimer = ai.meleeCooldown;
                        } else {
                            auto &playerHp = registry.get<Health>(playerEntity);
                            const float dmg = ai.meleeDamage;
                            playerHp.current -= dmg;
                            const float rf =
                                inventory ? inventory->totalEquippedDamageReflect() : 0.0F;
                            if (rf > 0.001F && registry.all_of<Health>(e)) {
                                auto &eh = registry.get<Health>(e);
                                eh.current -= dmg * rf;
                            }
                            ai.meleeTimer = ai.meleeCooldown;
                        }
                    }
                }
            }
            updateStuckDetection(ai, transform, desiredVel, fixedDt);
            desiredVel = applyStuckAvoidance(registry, transform.position, desiredVel, ai.stuckTimer);
            applySteering(vel, desiredVel, fixedDt);
            continue;
        }

        // Imp: chase-first with panic kite when very close.
        if (dist > 0.001F) {
            const float sign = (static_cast<int>(entt::to_integral(e)) % 2 == 0) ? 1.0F : -1.0F;
            const Vector2 perp = {-toPlayer.y * sign, toPlayer.x * sign};

            if (dist < config::IMP_PANIC_RANGE) {
                Vector2 moveDir = Vec2Normalize(
                    Vector2{-toPlayer.x + perp.x * config::IMP_STRAFE_BIAS,
                            -toPlayer.y + perp.y * config::IMP_STRAFE_BIAS});
                const Vector2 testPos = {transform.position.x + moveDir.x * 30.0F,
                                         transform.position.y + moveDir.y * 30.0F};
                if (!hasLineOfSight(registry, testPos, playerT.position)) {
                    moveDir = perp;
                }
                desiredVel = {moveDir.x * config::IMP_KITE_SPEED, moveDir.y * config::IMP_KITE_SPEED};
            } else if (dist < config::IMP_PREFERRED_RANGE) {
                const Vector2 chaseBias = Vec2Normalize(
                    Vector2{toPlayer.x * 0.75F + perp.x * 0.35F, toPlayer.y * 0.75F + perp.y * 0.35F});
                desiredVel = {chaseBias.x * config::IMP_ADVANCE_SPEED * 1.15F,
                              chaseBias.y * config::IMP_ADVANCE_SPEED * 1.15F};
            } else if (dist > config::IMP_PREFERRED_RANGE * 1.3F) {
                desiredVel = {toPlayer.x * config::IMP_ADVANCE_SPEED,
                              toPlayer.y * config::IMP_ADVANCE_SPEED};
            } else {
                desiredVel = {perp.x * (config::IMP_ADVANCE_SPEED * 0.7F),
                              perp.y * (config::IMP_ADVANCE_SPEED * 0.7F)};
            }
        }

        updateStuckDetection(ai, transform, desiredVel, fixedDt);
        desiredVel = applyStuckAvoidance(registry, transform.position, desiredVel, ai.stuckTimer);
        applySteering(vel, desiredVel, fixedDt);

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
            std::sqrt(enemySprite.width * enemySprite.width + enemySprite.height * enemySprite.height) *
            0.5F;
        const float spawnDist = halfDiag + config::PROJECTILE_RADIUS + 2.0F;
        const auto proj = registry.create();
        registry.emplace<Transform>(
            proj,
            Transform{{transform.position.x + dir.x * spawnDist, transform.position.y + dir.y * spawnDist},
                      std::atan2f(dir.y, dir.x)});
        registry.emplace<Velocity>(
            proj, Velocity{{dir.x * config::ENEMY_PROJECTILE_SPEED, dir.y * config::ENEMY_PROJECTILE_SPEED}});
        registry.emplace<Sprite>(proj, Sprite{{255, 120, 90, 255}, 9.0F, 9.0F});
        registry.emplace<Projectile>(
            proj, Projectile{config::PROJECTILE_DAMAGE, config::ENEMY_PROJECTILE_SPEED,
                             config::ENEMY_PROJECTILE_MAX_RANGE, 0.0F, dir, false, e});
    }
}

} // namespace dreadcast::ecs
