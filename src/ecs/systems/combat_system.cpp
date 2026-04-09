#include "ecs/systems/combat_system.hpp"

#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/iso_utils.hpp"
#include "core/types.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

void combat_player_ranged(entt::registry &registry, const InputManager &input,
                          const Camera2D &camera, entt::entity player, float &noManaFlashTimer,
                          Vector2 aimScreenPos) {
    if (!registry.valid(player) || !input.isMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }
    if (!registry.all_of<Transform, Mana, Sprite>(player)) {
        return;
    }
    if (registry.all_of<SlugAimState>(player)) {
        return;
    }

    auto &mana = registry.get<Mana>(player);
    if (mana.current < config::MANA_COST_SHOT) {
        noManaFlashTimer = 1.2F;
        return;
    }

    mana.current -= config::MANA_COST_SHOT;

    const auto &pt = registry.get<Transform>(player);
    const auto &ps = registry.get<Sprite>(player);
    const Vector2 isoMouse = GetScreenToWorld2D(aimScreenPos, camera);
    const Vector2 worldMouse = dreadcast::isoToWorld(isoMouse);
    const float dx = worldMouse.x - pt.position.x;
    const float dy = worldMouse.y - pt.position.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.001F) {
        return;
    }
    const Vector2 dir = {dx / len, dy / len};

    const float halfDiag =
        std::sqrt(ps.width * ps.width + ps.height * ps.height) * 0.5F;
    const float spawnDist = halfDiag + config::PROJECTILE_RADIUS + 2.0F;
    const bool leadFever = registry.all_of<LeadFeverEffect>(player);

    if (leadFever) {
        const int n = config::LEAD_FEVER_PELLET_COUNT;
        const float spread = config::LEAD_FEVER_SCATTER_ANGLE;
        for (int i = 0; i < n; ++i) {
            const float t = (n <= 1) ? 0.0F
                                     : (static_cast<float>(i) / static_cast<float>(n - 1) - 0.5F) *
                                           (2.0F * spread);
            const float ca = std::cosf(t);
            const float sa = std::sinf(t);
            const Vector2 d{dir.x * ca - dir.y * sa, dir.x * sa + dir.y * ca};
            const auto proj = registry.create();
            registry.emplace<Transform>(
                proj, Transform{{pt.position.x + d.x * spawnDist, pt.position.y + d.y * spawnDist},
                                std::atan2f(d.y, d.x)});
            registry.emplace<Velocity>(
                proj, Velocity{{d.x * config::PROJECTILE_SPEED, d.y * config::PROJECTILE_SPEED}});
            registry.emplace<Sprite>(proj, Sprite{{120, 220, 140, 255}, 9.0F, 9.0F});
            registry.emplace<Projectile>(
                proj,
                Projectile{config::PROJECTILE_DAMAGE * config::LEAD_FEVER_DAMAGE_MULT,
                           config::PROJECTILE_SPEED, config::PROJECTILE_MAX_RANGE, 0.0F, d, true,
                           entt::null, false, config::LEAD_FEVER_KNOCKBACK});
        }
        return;
    }

    const auto proj = registry.create();
    registry.emplace<Transform>(
        proj, Transform{{pt.position.x + dir.x * spawnDist, pt.position.y + dir.y * spawnDist},
                        std::atan2f(dir.y, dir.x)});
    registry.emplace<Velocity>(proj,
                               Velocity{{dir.x * config::PROJECTILE_SPEED, dir.y * config::PROJECTILE_SPEED}});
    registry.emplace<Sprite>(proj, Sprite{{255, 240, 120, 255}, 10.0F, 10.0F});
    registry.emplace<Projectile>(proj, Projectile{config::PROJECTILE_DAMAGE, config::PROJECTILE_SPEED,
                                                    config::PROJECTILE_MAX_RANGE, 0.0F, dir, true,
                                                    entt::null, false, 0.0F});
}

namespace {

void applyMeleeSwingHit(entt::registry &registry, entt::entity player, MeleeAttacker &melee) {
    if (melee.hitAppliedThisSwing || !registry.all_of<Transform>(player)) {
        return;
    }
    const float dur = MeleeAttacker::kSwingDuration[melee.swingIndex];
    const float t = melee.phaseTimer / dur;
    if (t < MeleeAttacker::kHitWindowStart || t > MeleeAttacker::kHitWindowEnd) {
        return;
    }

    const auto &pt = registry.get<Transform>(player);
    const Vector2 forward = {std::cosf(pt.rotation), std::sinf(pt.rotation)};
    const float dmgScale = MeleeAttacker::kDamageScale[melee.swingIndex];
    const float kbScale = MeleeAttacker::kKnockbackScale[melee.swingIndex];

    const auto enemies = registry.view<Enemy, Transform, Health, Sprite>();
    for (const auto e : enemies) {
        if (e == player) {
            continue;
        }
        const auto &et = registry.get<Transform>(e);
        const auto &es = registry.get<Sprite>(e);
        const Vector2 toEnemy = {et.position.x - pt.position.x, et.position.y - pt.position.y};
        const float dist = Vec2Length(toEnemy);
        const float reach = melee.range + std::max(es.width, es.height) * 0.5F;
        if (dist > reach || dist < 0.001F) {
            continue;
        }
        const Vec2 nd = Vec2Normalize(toEnemy);
        const float dot = Vec2Dot(forward, nd);
        if (dot < 0.5F) {
            continue;
        }

        auto &health = registry.get<Health>(e);
        health.current -= melee.damage * dmgScale;

        if (registry.all_of<Agitation>(e)) {
            registry.get<Agitation>(e).agitationRange += 100.0F;
        }

        auto &vel = registry.get_or_emplace<Velocity>(e);
        vel.value.x = nd.x * melee.knockback * kbScale;
        vel.value.y = nd.y * melee.knockback * kbScale;
        registry.emplace_or_replace<KnockbackState>(e, KnockbackState{config::KNOCKBACK_DURATION, 0.0F});
    }

    melee.hitAppliedThisSwing = true;
}

} // namespace

void combat_player_melee(entt::registry &registry, const InputManager &input, entt::entity player,
                         float fixedDt) {
    if (!registry.valid(player) || !registry.all_of<Transform, MeleeAttacker>(player)) {
        return;
    }
    if (registry.all_of<SlugAimState>(player)) {
        return;
    }

    auto &melee = registry.get<MeleeAttacker>(player);
    const bool held = input.isMouseButtonHeld(MOUSE_BUTTON_RIGHT);
    const bool pressed = input.isMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (melee.singleSwingCooldownTimer > 0.0F) {
        melee.singleSwingCooldownTimer -= fixedDt;
        if (melee.singleSwingCooldownTimer < 0.0F) {
            melee.singleSwingCooldownTimer = 0.0F;
        }
    }

    switch (melee.phase) {
    case MeleeAttacker::Phase::Idle: {
        const bool holdEdge = held && !melee.rmbHeldPrev;
        if (melee.singleSwingCooldownTimer <= 0.0F && (pressed || holdEdge)) {
            melee.phase = MeleeAttacker::Phase::Swing;
            melee.swingIndex = 0;
            melee.phaseTimer = 0.0F;
            melee.hitAppliedThisSwing = false;
        }
        break;
    }
    case MeleeAttacker::Phase::Swing: {
        melee.phaseTimer += fixedDt;
        applyMeleeSwingHit(registry, player, melee);

        const float dur = MeleeAttacker::kSwingDuration[melee.swingIndex];
        if (melee.phaseTimer < dur) {
            break;
        }
        if (melee.swingIndex < 2) {
            if (held) {
                melee.phase = MeleeAttacker::Phase::BetweenSwings;
                melee.phaseTimer = 0.0F;
            } else {
                melee.phase = MeleeAttacker::Phase::Idle;
                melee.singleSwingCooldownTimer = MeleeAttacker::kSingleSwingCooldown;
            }
        } else {
            melee.phase = MeleeAttacker::Phase::Recovery;
            melee.phaseTimer = 0.0F;
        }
        break;
    }
    case MeleeAttacker::Phase::BetweenSwings: {
        if (!held) {
            melee.phase = MeleeAttacker::Phase::Idle;
            melee.singleSwingCooldownTimer = MeleeAttacker::kSingleSwingCooldown;
            break;
        }
        melee.phaseTimer += fixedDt;
        if (melee.phaseTimer >= MeleeAttacker::kBetweenSwingsPause) {
            ++melee.swingIndex;
            melee.phase = MeleeAttacker::Phase::Swing;
            melee.phaseTimer = 0.0F;
            melee.hitAppliedThisSwing = false;
        }
        break;
    }
    case MeleeAttacker::Phase::Recovery: {
        melee.phaseTimer += fixedDt;
        if (melee.phaseTimer < MeleeAttacker::kRecoveryDuration) {
            break;
        }
        if (held) {
            melee.swingIndex = 0;
            melee.phase = MeleeAttacker::Phase::Swing;
            melee.phaseTimer = 0.0F;
            melee.hitAppliedThisSwing = false;
        } else {
            melee.phase = MeleeAttacker::Phase::Idle;
            melee.singleSwingCooldownTimer = MeleeAttacker::kSingleSwingCooldown;
        }
        break;
    }
    }

    melee.rmbHeldPrev = held;
}

} // namespace dreadcast::ecs
