#include "ecs/combat_resolution.hpp"

#include <algorithm>

#include <entt/entt.hpp>

#include "ecs/components.hpp"
#include "game/equipment_snapshot.hpp"
#include "game/item_effects.hpp"
#include "game/items.hpp"

namespace dreadcast::ecs {

namespace {

[[nodiscard]] bool shouldSwallowProjectileDamageForManic(entt::registry &registry,
                                                         entt::entity victimEnt,
                                                         const DamagePacket &packet) {
    if (packet.category != DamageCategory::EnemyProjectileVsPlayer) {
        return false;
    }
    return dreadcast::playerHasManicEffect(registry, victimEnt);
}

[[nodiscard]] bool isReflectEligible(const DamagePacket &packet,
                                     const CombatResolutionOpts &opts) {
    if (packet.category != DamageCategory::EnemyProjectileVsPlayer &&
        packet.category != DamageCategory::MeleeEnemyVsPlayer) {
        return false;
    }
    if (opts.victimEquip == nullptr || opts.victimEquip->damageReflect <= 0.001F) {
        return false;
    }
    return packet.category != DamageCategory::Reflect;
}

} // namespace

DamageOutcome resolveDamage(entt::registry &registry, entt::entity victim, const DamagePacket &packet,
                            const CombatResolutionOpts &opts) {
    DamageOutcome out{};
    if (!registry.valid(victim) || !registry.all_of<Health>(victim)) {
        return out;
    }

    auto &health = registry.get<Health>(victim);
    if (packet.amount <= 0.001F || health.current <= 0.001F) {
        return out;
    }

    if (packet.category == DamageCategory::EnemyProjectileVsPlayer &&
        registry.all_of<PlayerLevel>(victim)) {
        if (shouldSwallowProjectileDamageForManic(registry, victim, packet)) {
            out.swallowedByInvulnerable = true;
            return out;
        }
    }

    const float dmg = packet.amount;
    health.current -= dmg;
    health.current = std::max(0.0F, health.current);
    out.dealt = dmg;

    if (isReflectEligible(packet, opts) && registry.valid(packet.source) &&
        registry.all_of<Health>(packet.source)) {
        const float rf = opts.victimEquip->damageReflect;
        DamagePacket refl{victim, dmg * rf, DamageCategory::Reflect};
        (void)resolveDamage(registry, packet.source, refl, {});
    }

    if (opts.victimInventory != nullptr) {
        dreadcast::dispatchOnTakeDamageItemEffects(registry, victim, opts.victimInventory, out.dealt);
    }

    return out;
}

float applyHeal(entt::registry &registry, entt::entity victim, const HealPacket &packet) {
    if (!registry.valid(victim) || !registry.all_of<Health>(victim) || packet.amount <= 0.001F) {
        return 0.0F;
    }
    auto &health = registry.get<Health>(victim);
    if (health.current <= 0.001F || health.max <= 0.001F) {
        return 0.0F;
    }
    const float before = health.current;
    health.current = std::min(health.max, health.current + packet.amount);
    return health.current - before;
}

} // namespace dreadcast::ecs
