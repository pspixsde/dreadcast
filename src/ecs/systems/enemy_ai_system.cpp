#include "ecs/systems/enemy_ai_system.hpp"

#include <algorithm>
#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/iso_utils.hpp"
#include "core/poly_helpers.hpp"
#include "core/types.hpp"
#include "ecs/combat_resolution.hpp"
#include "ecs/components.hpp"
#include "game/equipment_snapshot.hpp"
#include "game/game_data.hpp"
#include "game/item_effects.hpp"
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
    const auto polys = registry.view<SolidPolygon, Transform>();
    for (const auto pe : polys) {
        const auto &poly = registry.get<SolidPolygon>(pe);
        if (poly.vertsWorld.size() >= 3 &&
            dreadcast::segmentIntersectsPolygonEdges(from, to, poly.vertsWorld)) {
            return false;
        }
    }
    return true;
}

void applySteering(Velocity &vel, Vector2 desired, float fixedDt) {
    const float k = std::min(1.0F, dreadcast::enemyAiGlobals().steerRate * fixedDt);
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

/// True if `p` lies inside the oriented slam rectangle starting at `origin`, pointing along unit
/// `dir`, extending `length` forward and `halfWidth` to each side.
bool pointInForwardLine(Vector2 p, Vector2 origin, Vector2 dir, float length, float halfWidth) {
    const Vector2 rel{p.x - origin.x, p.y - origin.y};
    const float along = rel.x * dir.x + rel.y * dir.y;
    const float side = rel.x * -dir.y + rel.y * dir.x;
    return along >= 0.0F && along <= length && std::fabs(side) <= halfWidth;
}

void updateStuckDetection(EnemyAI &ai, const Transform &transform, Vector2 desiredVel,
                          float fixedDt) {
    const float dispX = transform.position.x - ai.prevPosition.x;
    const float dispY = transform.position.y - ai.prevPosition.y;
    const float disp = std::sqrt(dispX * dispX + dispY * dispY);
    const float desSpd = std::sqrt(desiredVel.x * desiredVel.x + desiredVel.y * desiredVel.y);

    if (desSpd > 5.0F && disp < dreadcast::enemyAiGlobals().stuckMinDisp) {
        ai.stuckTimer += fixedDt;
    } else {
        ai.stuckTimer = 0.0F;
    }
    ai.prevPosition = transform.position;
}

Vector2 applyStuckAvoidance(entt::registry &registry, Vector2 pos, Vector2 desiredVel,
                            float stuckTimer) {
    if (stuckTimer < dreadcast::enemyAiGlobals().stuckThreshold) {
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
                     dreadcast::InventoryState *inventory,
                     const dreadcast::PlayerEquipmentSnapshot *equipSnapshot) {
    (void)inventory;
    if (playerEntity == entt::null || !registry.all_of<Transform>(playerEntity)) {
        return;
    }
    const auto &playerT = registry.get<Transform>(playerEntity);
    const dreadcast::EnemyAiGlobals &aiGlobals = dreadcast::enemyAiGlobals();

    const auto view = registry.view<Enemy, EnemyAI, Transform, Sprite, Facing, Agitation, Velocity>();
    for (const auto e : view) {
        auto &ai = registry.get<EnemyAI>(e);
        auto &transform = registry.get<Transform>(e);
        auto &facing = registry.get<Facing>(e);
        auto &ag = registry.get<Agitation>(e);
        auto &vel = registry.get<Velocity>(e);
        const auto &enemySprite = registry.get<Sprite>(e);

        float speedMul = 1.0F;
        if (const auto *buff = registry.try_get<EnemySpeedBuff>(e); buff != nullptr) {
            speedMul = buff->multiplier;
        }

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
            // A stun stops the Warden's basic attack: cancel any in-progress cast and put the
            // attack back on cooldown.
            if (auto *ws = registry.try_get<WardenState>(e); ws != nullptr && ws->telegraphActive) {
                ws->telegraphActive = false;
                if (const auto *wt = registry.try_get<WardenTuning>(e); wt != nullptr) {
                    ws->attackCooldownTimer = wt->attackCooldown;
                }
            }
            continue;
        }

        const float dx = playerT.position.x - transform.position.x;
        const float dy = playerT.position.y - transform.position.y;
        const float distSq = dx * dx + dy * dy;
        const float dist = std::sqrt(distSq);
        const float agRange = ag.agitationRange > 0.0F ? ag.agitationRange : 350.0F;

        const bool hasLOS = hasLineOfSight(registry, transform.position, playerT.position);
        const bool inAggroRange = dist <= agRange;
        const bool wantsAggro = inAggroRange && hasLOS;

        if (ai.permanentAggro) {
            // Relentless: never calms and always tracks the live player position (chases through
            // cover toward the real player rather than a stale last-known spot).
            ag.agitated = true;
            ag.calmDownTimer = 0.0F;
            ag.lastKnownPlayerPos = playerT.position;
            ag.hasLastKnownPos = true;
        } else if (wantsAggro) {
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
            if (ag.hasLastKnownPos && distLast < aiGlobals.seekArriveRadius && !hasLOS) {
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
                if (ai.type == EnemyType::Hellhound || ai.type == EnemyType::Dreg) {
                    const float cs = ai.chaseSpeed * speedMul;
                    desiredVel = {toLast.x * cs, toLast.y * cs};
                } else {
                    desiredVel = {toLast.x * ai.advanceSpeed, toLast.y * ai.advanceSpeed};
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

        if (ai.type == EnemyType::Hellhound || ai.type == EnemyType::Dreg) {
            ai.meleeTimer -= fixedDt;
            if (dist > ai.meleeRange) {
                const float cs = ai.chaseSpeed * speedMul;
                desiredVel = {toPlayer.x * cs, toPlayer.y * cs};
            } else {
                desiredVel = {0.0F, 0.0F};
                if (ai.meleeTimer <= 0.0F) {
                    if (registry.all_of<Health>(playerEntity)) {
                        if (dreadcast::playerHasManicEffect(registry, playerEntity)) {
                            ai.meleeTimer = ai.meleeCooldown;
                        } else {
                            CombatResolutionOpts ropts{};
                            ropts.victimEquip = equipSnapshot;
                            ropts.victimInventory = inventory;
                            const DamagePacket pkt{e, ai.meleeDamage, DamageCategory::MeleeEnemyVsPlayer};
                            (void)resolveDamage(registry, playerEntity, pkt, ropts);
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

        if (ai.type == EnemyType::Warden) {
            auto *tunePtr = registry.try_get<WardenTuning>(e);
            if (tunePtr == nullptr) {
                continue;
            }
            const WardenTuning &tune = *tunePtr;
            auto &ws = registry.get_or_emplace<WardenState>(e);
            ws.attackCooldownTimer = std::max(0.0F, ws.attackCooldownTimer - fixedDt);
            ws.abilityCooldownTimer = std::max(0.0F, ws.abilityCooldownTimer - fixedDt);
            ws.slamFlashTimer = std::max(0.0F, ws.slamFlashTimer - fixedDt);
            ws.abilityFlashTimer = std::max(0.0F, ws.abilityFlashTimer - fixedDt);

            const bool playerImmune =
                !registry.all_of<Health>(playerEntity) ||
                dreadcast::playerHasManicEffect(registry, playerEntity);

            // Close-range push ability: charges while the player lingers inside close range.
            if (dist <= tune.closeRange) {
                ws.closeContactTimer += fixedDt;
            } else {
                ws.closeContactTimer = 0.0F;
            }
            if (ws.closeContactTimer >= tune.abilityChargeTime && ws.abilityCooldownTimer <= 0.0F) {
                ws.abilityCooldownTimer = tune.abilityCooldown;
                ws.closeContactTimer = 0.0F;
                ws.abilityFlashTimer = 0.45F;
                ws.abilitySfxPending = true;
                ws.telegraphActive = false;
                if (!playerImmune && dist > 0.001F) {
                    auto &pvel = registry.get_or_emplace<Velocity>(playerEntity);
                    pvel.value = {toPlayer.x * tune.abilityKnockback,
                                  toPlayer.y * tune.abilityKnockback};
                    registry.emplace_or_replace<KnockbackState>(
                        playerEntity, KnockbackState{tune.abilityKnockbackDuration, 0.0F});
                    registry.emplace_or_replace<PlayerSlow>(
                        playerEntity,
                        PlayerSlow{tune.slowMultiplier, tune.slowDuration, 0.0F});
                }
                applySteering(vel, {0.0F, 0.0F}, fixedDt);
                continue;
            }

            // Telegraphed line slam: hold position while the area is highlighted, then strike.
            // The strike zone originates at the Warden's live position (it stays attached if the
            // Warden is shoved during the cast).
            if (ws.telegraphActive) {
                ws.telegraphTimer -= fixedDt;
                if (ws.telegraphTimer <= 0.0F) {
                    ws.telegraphActive = false;
                    ws.attackCooldownTimer = tune.attackCooldown;
                    ws.slamFlashTimer = 0.28F;
                    ws.attackSfxPending = true;
                    if (!playerImmune &&
                        pointInForwardLine(playerT.position, transform.position, ws.attackDir,
                                           tune.attackLineLength, tune.attackLineHalfWidth)) {
                        CombatResolutionOpts ropts{};
                        ropts.victimEquip = equipSnapshot;
                        ropts.victimInventory = inventory;
                        const DamagePacket pkt{e, tune.attackDamage,
                                               DamageCategory::MeleeEnemyVsPlayer};
                        (void)resolveDamage(registry, playerEntity, pkt, ropts);
                    }
                }
                // No self-movement while casting (knockback is handled before this branch).
                vel.value.x = 0.0F;
                vel.value.y = 0.0F;
                continue;
            }

            // Mid-range positioning: close in if far, back off if the player gets too near.
            if (dist > tune.preferredRange * 1.15F) {
                desiredVel = {toPlayer.x * ai.chaseSpeed, toPlayer.y * ai.chaseSpeed};
            } else if (dist < tune.preferredRange * 0.75F) {
                desiredVel = {-toPlayer.x * ai.chaseSpeed * 0.6F,
                              -toPlayer.y * ai.chaseSpeed * 0.6F};
            } else {
                desiredVel = {0.0F, 0.0F};
            }
            updateStuckDetection(ai, transform, desiredVel, fixedDt);
            desiredVel = applyStuckAvoidance(registry, transform.position, desiredVel, ai.stuckTimer);
            applySteering(vel, desiredVel, fixedDt);

            // Commit a slam toward the player's current position when off cooldown and in range.
            if (ws.attackCooldownTimer <= 0.0F && dist <= tune.attackRange) {
                ws.telegraphActive = true;
                ws.telegraphTimer = tune.attackTelegraph;
                ws.attackDir = toPlayer;
            }
            continue;
        }

        // Imp: chase-first with panic kite when very close.
        if (dist > 0.001F) {
            const float sign = (static_cast<int>(entt::to_integral(e)) % 2 == 0) ? 1.0F : -1.0F;
            const Vector2 perp = {-toPlayer.y * sign, toPlayer.x * sign};

            if (dist < ai.panicRange) {
                Vector2 moveDir = Vec2Normalize(
                    Vector2{-toPlayer.x + perp.x * ai.strafeBias, -toPlayer.y + perp.y * ai.strafeBias});
                const Vector2 testPos = {transform.position.x + moveDir.x * 30.0F,
                                         transform.position.y + moveDir.y * 30.0F};
                if (!hasLineOfSight(registry, testPos, playerT.position)) {
                    moveDir = perp;
                }
                desiredVel = {moveDir.x * ai.kiteSpeed, moveDir.y * ai.kiteSpeed};
            } else if (dist < ai.preferredRange) {
                const Vector2 chaseBias = Vec2Normalize(
                    Vector2{toPlayer.x * 0.75F + perp.x * 0.35F, toPlayer.y * 0.75F + perp.y * 0.35F});
                desiredVel = {chaseBias.x * ai.advanceSpeed * 1.15F,
                              chaseBias.y * ai.advanceSpeed * 1.15F};
            } else if (dist > ai.preferredRange * 1.3F) {
                desiredVel = {toPlayer.x * ai.advanceSpeed, toPlayer.y * ai.advanceSpeed};
            } else {
                desiredVel = {perp.x * (ai.advanceSpeed * 0.7F), perp.y * (ai.advanceSpeed * 0.7F)};
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
            proj, Velocity{{dir.x * ai.projectileSpeed, dir.y * ai.projectileSpeed}});
        registry.emplace<Sprite>(proj, Sprite{{255, 120, 90, 255}, 9.0F, 9.0F});
        registry.emplace<Projectile>(
            proj, Projectile{ai.projectileDamage, ai.projectileSpeed, ai.projectileRange, 0.0F, dir,
                             false, e});
    }
}

} // namespace dreadcast::ecs
