#pragma once

#include <entt/entt.hpp>

#include "game/equipment_snapshot.hpp"

namespace dreadcast {
struct InventoryState;
}

namespace dreadcast::ecs {

enum class DamageCategory : uint8_t {
    PlayerProjectileVsEnemy,
    EnemyProjectileVsPlayer,
    MeleeEnemyVsPlayer,
    MeleePlayerVsEnemy,
    Environmental,
    Reflect,
};

struct DamagePacket {
    entt::entity source{entt::null};
    float amount{0.0F};
    DamageCategory category{DamageCategory::Environmental};
};

struct CombatResolutionOpts {
    /// When the victim is the player, reflective damage reads from equipment snapshot (optional).
    const PlayerEquipmentSnapshot *victimEquip{nullptr};
    /// For `OnTakeDamage` item-effect dispatch (optional).
    const InventoryState *victimInventory{nullptr};
};

struct DamageOutcome {
    float dealt{0.0F};
    bool swallowedByInvulnerable{false};
};

struct HealPacket {
    float amount{0.0F};
};

/// Applies damage resolution in one place: manic invuln (projectiles on player),
/// reflective damage vs `source`, and HP clamp / death bookkeeping.
DamageOutcome resolveDamage(entt::registry &registry, entt::entity victim, const DamagePacket &packet,
                            const CombatResolutionOpts &opts = {});

/// Clamp heal to max HP; returns amount actually applied.
float applyHeal(entt::registry &registry, entt::entity victim, const HealPacket &packet);

} // namespace dreadcast::ecs
