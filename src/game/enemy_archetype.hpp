#pragma once

#include <raylib.h>

#include <string>

namespace dreadcast {

/// Shared AI steering / stuck-detection knobs (`enemies.json` top-level `globals`).
struct EnemyAiGlobals {
    float steerRate{12.0F};
    float seekArriveRadius{22.0F};
    float stuckThreshold{0.25F};
    float stuckMinDisp{1.5F};
};

/// Ranged kiter tuning (`behavior`: `ranged_kiter`).
struct EnemyRangedTuning {
    float minShootRange{60.0F};
    float shootCooldown{2.0F};
    float preferredRange{220.0F};
    float kiteSpeed{145.0F};
    float advanceSpeed{100.0F};
    float panicRange{80.0F};
    float strafeBias{0.4F};
    float projectileDamage{15.0F};
    float projectileSpeed{400.0F};
    float projectileRange{400.0F};
};

/// Melee chaser tuning (`behavior`: `melee_chaser`).
struct EnemyMeleeTuning {
    float chaseSpeed{210.0F};
    float meleeDamage{15.0F};
    float meleeRange{44.0F};
    float meleeCooldown{1.0F};
};

/// Mid-range bruiser tuning (`behavior`: `mid_bruiser`).
struct EnemyBruiserTuning {
    float chaseSpeed{210.0F};
    float preferredRange{170.0F};
    float attackDamage{35.0F};
    float attackCooldown{1.6F};
    float attackTelegraph{0.5F};
    float attackRange{235.0F};
    float attackLineLength{245.0F};
    float attackLineHalfWidth{40.0F};
    float closeRange{95.0F};
    float abilityChargeTime{2.0F};
    float abilityCooldown{10.0F};
    float abilityKnockback{680.0F};
    float abilityKnockbackDuration{0.3F};
    float slowMultiplier{0.8F};
    float slowDuration{4.0F};
};

/// Authoritative enemy row from `assets/data/enemies.json` (keyed by `id`).
struct EnemyArchetype {
    std::string id;
    std::string displayName;
    /// `ranged_kiter`, `melee_chaser`, or `mid_bruiser`.
    std::string behavior;
    Color tint{220, 90, 60, 255};
    float spriteSize{40.0F};
    float hp{50.0F};
    float xp{25.0F};
    float agitationRange{350.0F};
    float calmDownDelay{5.0F};
    EnemyRangedTuning ranged{};
    EnemyMeleeTuning melee{};
    EnemyBruiserTuning bruiser{};
};

} // namespace dreadcast
