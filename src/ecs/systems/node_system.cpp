#include "ecs/systems/node_system.hpp"

#include <cmath>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "ecs/components.hpp"
#include "ecs/enemy_factory.hpp"
#include "game/enemy_archetype.hpp"
#include "game/game_data.hpp"

namespace dreadcast::ecs {

namespace {

/// Spawns one Node-owned Dreg at `pos`: relentless permanent aggro, locked onto the player, and a
/// temporary burst speed buff.
void spawnDreg(entt::registry &registry, const EnemyArchetype &arch, Vector2 pos, Vector2 playerPos,
               float buffMult, float buffDuration) {
    const auto e = spawnEnemyFromArchetype(registry, arch, pos);
    if (auto *ai = registry.try_get<EnemyAI>(e)) {
        ai->permanentAggro = true;
    }
    if (auto *ag = registry.try_get<Agitation>(e)) {
        ag->agitated = true;
        ag->hasLastKnownPos = true;
        ag->lastKnownPlayerPos = playerPos;
        ag->calmDownTimer = 0.0F;
    }
    registry.emplace_or_replace<EnemySpeedBuff>(e, EnemySpeedBuff{buffMult, buffDuration, 0.0F});
}

/// Emits `count` Dregs in a ring around `center` so they fan out instead of stacking.
void spawnDregRing(entt::registry &registry, const EnemyArchetype &arch, Vector2 center,
                   Vector2 playerPos, int count, float ringRadius, float buffMult,
                   float buffDuration) {
    if (count <= 0) {
        return;
    }
    const float step = 2.0F * PI / static_cast<float>(count);
    const float phase = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
    for (int i = 0; i < count; ++i) {
        const float ang = phase + step * static_cast<float>(i);
        const float jitter = ringRadius * (0.85F + static_cast<float>(GetRandomValue(0, 30)) / 100.0F);
        const Vector2 pos{center.x + std::cos(ang) * jitter, center.y + std::sin(ang) * jitter};
        spawnDreg(registry, arch, pos, playerPos, buffMult, buffDuration);
    }
}

} // namespace

void node_system(entt::registry &registry, float fixedDt, entt::entity playerEntity) {
    // Advance / expire temporary enemy speed buffs (runs regardless of Node presence).
    {
        std::vector<entt::entity> expired;
        for (const auto e : registry.view<EnemySpeedBuff>()) {
            auto &buff = registry.get<EnemySpeedBuff>(e);
            buff.elapsed += fixedDt;
            if (buff.elapsed >= buff.duration) {
                expired.push_back(e);
            }
        }
        for (const auto e : expired) {
            registry.remove<EnemySpeedBuff>(e);
        }
    }

    if (playerEntity == entt::null || !registry.all_of<Transform>(playerEntity)) {
        return;
    }
    const Vector2 playerPos = registry.get<Transform>(playerEntity).position;

    const EnemyArchetype *dregArch = enemyArchetypeById("dreg");
    if (dregArch == nullptr) {
        return; // Without the Dreg archetype a Node cannot spawn anything.
    }

    for (const auto e : registry.view<NodeSpawner, Transform, Health>()) {
        auto &node = registry.get<NodeSpawner>(e);
        const auto &t = registry.get<Transform>(e);
        const auto &hp = registry.get<Health>(e);
        if (hp.current <= 0.0F) {
            continue; // Death handled by death_system.
        }

        // Detect damage between ticks; a hit while dormant temporarily expands the trigger radius.
        if (node.prevHealth < 0.0F) {
            node.prevHealth = hp.current;
        }
        const bool tookDamage = hp.current < node.prevHealth - 0.001F;
        if (node.damageTriggerTimer > 0.0F) {
            node.damageTriggerTimer -= fixedDt;
        }
        if (!node.active && tookDamage) {
            node.damageTriggerTimer = node.damageTriggerDuration;
        }

        const float dx = playerPos.x - t.position.x;
        const float dy = playerPos.y - t.position.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float ringRadius = registry.all_of<Sprite>(e)
                                     ? registry.get<Sprite>(e).width * 0.7F + 26.0F
                                     : 50.0F;

        if (!node.active) {
            const float trigger =
                node.damageTriggerTimer > 0.0F ? node.damageTriggerRadius : node.triggerRadius;
            if (dist <= trigger) {
                node.active = true;
                node.dormantTimer = 0.0F;
                node.damageTriggerTimer = 0.0F;
                node.sustainTimer = node.sustainInterval;
                spawnDregRing(registry, *dregArch, t.position, playerPos, node.burstCount,
                              ringRadius, node.buffMultiplier, node.buffDuration);
            }
            node.prevHealth = hp.current;
            continue;
        }

        const float activeRadius = node.activeRadius;
        if (dist <= activeRadius) {
            node.dormantTimer = 0.0F;
            node.sustainTimer -= fixedDt;
            if (node.sustainTimer <= 0.0F) {
                node.sustainTimer = node.sustainInterval;
                spawnDregRing(registry, *dregArch, t.position, playerPos, node.sustainCount,
                              ringRadius, node.buffMultiplier, node.buffDuration);
            }
        } else {
            node.dormantTimer += fixedDt;
            if (node.dormantTimer >= node.dormantDelay) {
                node.active = false;
                node.dormantTimer = 0.0F;
                node.sustainTimer = 0.0F;
                // Regenerate to full health on returning to dormancy.
                if (auto *h = registry.try_get<Health>(e)) {
                    h->current = h->max;
                }
            }
        }
        node.prevHealth = hp.current;
    }
}

} // namespace dreadcast::ecs
