#pragma once

#include <string>
#include <vector>

namespace dreadcast {

enum class ItemEffectTrigger : uint8_t {
    None = 0,
    OnEquipped,
    OnUnequipped,
    OnUse,
    OnTick,
    OnTakeDamage,
    OnAbilityCooldownEnd,
    OnHpThreshold,
    OnStandingStill,
};

enum class ItemEffectActionKind : uint8_t {
    None = 0,
    StatModifier,
    HealOverTime,
    ManaOverTime,
    SpeedWindow,
    AoeShockwave,
    ManaRefund,
    /// Still-standing extra vision (Vigilant Eye).
    VisionStandingBonus,
};

enum class StatModifierKind : uint8_t { Add, Mul };

enum class HpThresholdEdge : uint8_t { Any, Downward };

/// Flat action payload (kind selects which fields apply).
struct ItemEffectAction {
    ItemEffectActionKind kind{ItemEffectActionKind::None};
    /// statModifier
    std::string statName{}; // maxHp, maxMana, hpRegen, moveSpeed, visionRange, damageReflect,
                            // abilityManaCost, abilityCooldownManaRefund
    float statValue{0.0F};
    StatModifierKind statKind{StatModifierKind::Add};
    /// healOverTime / manaOverTime
    float overTimeTotal{0.0F};
    float overTimeDuration{0.0F};
    /// speedWindow (Cordial Manic)
    float speedMultiplier{1.0F};
    float speedWindowDuration{0.0F};
    float speedWindowMinHpFraction{0.0F};
    float speedWindowDrainHpFraction{0.0F};
    bool speedWindowInvulnerable{false};
    /// aoeShockwave (Runic Shell proc)
    float shockwaveRadius{0.0F};
    float shockwaveDamage{0.0F};
    float shockwaveKnockback{0.0F};
    float shockwaveSelfHeal{0.0F};
    float shockwaveCooldown{0.0F};
    /// manaRefund (Pulse Link style — handled separately; kept for schema)
    float manaRefundAmount{0.0F};
    /// visionStandingBonus (Vigilant Eye)
    float standingStillSeconds{0.0F};
    float standingVisionBonus{0.0F};
};

struct ItemEffectCondition {
    float hpFractionAtMost{-1.0F}; // < 0 = unused
    HpThresholdEdge hpEdge{HpThresholdEdge::Any};
    float cooldownSeconds{0.0F};
    float stillForSeconds{0.0F};
    std::string requireSlot{}; // "Amulet" etc., empty = any
};

struct ItemEffectDef {
    ItemEffectTrigger trigger{ItemEffectTrigger::None};
    ItemEffectCondition condition{};
    ItemEffectAction action{};
};

} // namespace dreadcast
