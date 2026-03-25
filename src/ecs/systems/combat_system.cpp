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
                          const Camera2D &camera, entt::entity player, float &noManaFlashTimer) {
    if (!registry.valid(player) || !input.isMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }
    if (!registry.all_of<Transform, Mana, Sprite>(player)) {
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
    const Vector2 isoMouse = GetScreenToWorld2D(input.mousePosition(), camera);
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
    const auto proj = registry.create();
    registry.emplace<Transform>(
        proj, Transform{{pt.position.x + dir.x * spawnDist, pt.position.y + dir.y * spawnDist},
                        std::atan2f(dir.y, dir.x)});
    registry.emplace<Velocity>(proj,
                               Velocity{{dir.x * config::PROJECTILE_SPEED, dir.y * config::PROJECTILE_SPEED}});
    registry.emplace<Sprite>(proj, Sprite{{255, 240, 120, 255}, 10.0F, 10.0F});
    registry.emplace<Projectile>(
        proj, Projectile{config::PROJECTILE_DAMAGE, config::PROJECTILE_SPEED,
                         config::PROJECTILE_MAX_RANGE, 0.0F, dir, true});
}

void combat_player_melee(entt::registry &registry, const InputManager &input, entt::entity player,
                         float fixedDt) {
    if (!registry.valid(player) || !registry.all_of<Transform, MeleeAttacker>(player)) {
        return;
    }

    auto &melee = registry.get<MeleeAttacker>(player);
    const auto &pt = registry.get<Transform>(player);

    const bool rmbHeld = input.isMouseButtonHeld(MOUSE_BUTTON_RIGHT);
    if (melee.rmbHeldPrev && !rmbHeld) {
        melee.reEngageCooldown = 1.0F; // prevent right-click spam
    }
    melee.rmbHeldPrev = rmbHeld;

    if (melee.reEngageCooldown > 0.0F) {
        melee.reEngageCooldown -= fixedDt;
        if (melee.reEngageCooldown < 0.0F) {
            melee.reEngageCooldown = 0.0F;
        }
    }

    melee.isAttacking = rmbHeld && melee.reEngageCooldown <= 0.0F;
    if (melee.isAttacking) {
        melee.swingPhase += fixedDt * 12.0F;
    } else {
        melee.swingPhase = 0.0F;
    }

    if (melee.cooldownTimer > 0.0F) {
        melee.cooldownTimer -= fixedDt;
    }

    if (!melee.isAttacking || melee.cooldownTimer > 0.0F) {
        return;
    }

    const Vector2 forward = {std::cosf(pt.rotation), std::sinf(pt.rotation)};

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
        health.current -= melee.damage;

        auto &vel = registry.get_or_emplace<Velocity>(e);
        vel.value.x += nd.x * melee.knockback;
        vel.value.y += nd.y * melee.knockback;

        melee.cooldownTimer = melee.cooldown;
    }
}

} // namespace dreadcast::ecs
