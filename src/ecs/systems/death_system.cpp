#include "ecs/systems/death_system.hpp"

#include <algorithm>
#include <vector>

#include <entt/entt.hpp>

#include "ecs/components.hpp"

namespace dreadcast::ecs {

namespace {

void applyLevelUp(entt::registry &registry, entt::entity player, PlayerLevel &pl) {
    if (!registry.valid(player)) {
        return;
    }
    ++pl.level;
    pl.rangedDamageBonus += pl.perLevelRangedDamage;

    if (registry.all_of<PlayerClassStats>(player)) {
        auto &pc = registry.get<PlayerClassStats>(player);
        pc.baseMaxHp += pl.perLevelMaxHp;
        pc.baseMaxMana += pl.perLevelMaxMana;
    }
    if (registry.all_of<Health>(player)) {
        auto &hp = registry.get<Health>(player);
        hp.max += pl.perLevelMaxHp;
        hp.current = std::min(hp.max, hp.current + pl.perLevelMaxHp);
    }
    if (registry.all_of<Mana>(player)) {
        auto &mp = registry.get<Mana>(player);
        mp.max += pl.perLevelMaxMana;
        mp.current = std::min(mp.max, mp.current + pl.perLevelMaxMana);
    }
    if (registry.all_of<MeleeAttacker>(player)) {
        auto &melee = registry.get<MeleeAttacker>(player);
        melee.damage += pl.perLevelMeleeDamage;
    }
}

void awardXpToPlayer(entt::registry &registry, entt::entity player, float amount) {
    if (!registry.valid(player) || !registry.all_of<PlayerLevel>(player)) {
        return;
    }
    auto &pl = registry.get<PlayerLevel>(player);
    pl.xp += amount;
    while (pl.xp + 1.0e-4F >= pl.xpToNextLevel) {
        pl.xp -= pl.xpToNextLevel;
        applyLevelUp(registry, player, pl);
    }
}

} // namespace

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
            float xpAmt = 25.0F;
            if (registry.all_of<EnemyXpReward>(e)) {
                xpAmt = registry.get<EnemyXpReward>(e).xp;
            }
            awardXpToPlayer(registry, player, xpAmt);
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
