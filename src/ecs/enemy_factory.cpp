#include "ecs/enemy_factory.hpp"

#include "ecs/components.hpp"
#include "game/enemy_archetype.hpp"

namespace dreadcast::ecs {

entt::entity spawnEnemyFromArchetype(entt::registry &registry, const EnemyArchetype &arch,
                                     Vector2 pos) {
    const auto e = registry.create();
    registry.emplace<Transform>(e, Transform{pos, 0.0F});
    registry.emplace<Velocity>(e, Velocity{});
    registry.emplace<Sprite>(e, Sprite{arch.tint, arch.spriteSize, arch.spriteSize});
    registry.emplace<Facing>(e, Facing{});
    registry.emplace<Health>(e, Health{arch.hp, arch.hp});
    registry.emplace<Enemy>(e);
    registry.emplace<NameTag>(e, NameTag{arch.displayName.c_str()});

    EnemyAI aiComp{};
    aiComp.type = enemyTypeFromBehavior(arch.behavior);
    if (arch.behavior == "melee_chaser" || arch.behavior == "dreg_swarmer") {
        aiComp.chaseSpeed = arch.melee.chaseSpeed;
        aiComp.meleeDamage = arch.melee.meleeDamage;
        aiComp.meleeRange = arch.melee.meleeRange;
        aiComp.meleeCooldown = arch.melee.meleeCooldown;
    } else if (arch.behavior == "mid_bruiser") {
        aiComp.chaseSpeed = arch.bruiser.chaseSpeed;
    } else {
        aiComp.shootCooldown = arch.ranged.shootCooldown;
        aiComp.shootTimer = arch.ranged.shootCooldown;
        aiComp.minShootRange = arch.ranged.minShootRange;
        aiComp.preferredRange = arch.ranged.preferredRange;
        aiComp.kiteSpeed = arch.ranged.kiteSpeed;
        aiComp.advanceSpeed = arch.ranged.advanceSpeed;
        aiComp.panicRange = arch.ranged.panicRange;
        aiComp.strafeBias = arch.ranged.strafeBias;
        aiComp.projectileDamage = arch.ranged.projectileDamage;
        aiComp.projectileSpeed = arch.ranged.projectileSpeed;
        aiComp.projectileRange = arch.ranged.projectileRange;
    }
    registry.emplace<EnemyAI>(e, aiComp);

    if (arch.behavior == "mid_bruiser") {
        registry.emplace<WardenState>(e, WardenState{});
        WardenTuning wt{};
        wt.chaseSpeed = arch.bruiser.chaseSpeed;
        wt.preferredRange = arch.bruiser.preferredRange;
        wt.attackDamage = arch.bruiser.attackDamage;
        wt.attackCooldown = arch.bruiser.attackCooldown;
        wt.attackTelegraph = arch.bruiser.attackTelegraph;
        wt.attackRange = arch.bruiser.attackRange;
        wt.attackLineLength = arch.bruiser.attackLineLength;
        wt.attackLineHalfWidth = arch.bruiser.attackLineHalfWidth;
        wt.closeRange = arch.bruiser.closeRange;
        wt.abilityChargeTime = arch.bruiser.abilityChargeTime;
        wt.abilityCooldown = arch.bruiser.abilityCooldown;
        wt.abilityKnockback = arch.bruiser.abilityKnockback;
        wt.abilityKnockbackDuration = arch.bruiser.abilityKnockbackDuration;
        wt.slowMultiplier = arch.bruiser.slowMultiplier;
        wt.slowDuration = arch.bruiser.slowDuration;
        registry.emplace<WardenTuning>(e, wt);
    }

    registry.emplace<Agitation>(e, Agitation{arch.agitationRange, arch.calmDownDelay, 0.0F, false});
    registry.emplace<EnemyXpReward>(e, EnemyXpReward{arch.xp});
    registry.emplace<EnemyDisplayLevel>(e, EnemyDisplayLevel{1});
    return e;
}

} // namespace dreadcast::ecs
