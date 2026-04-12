#pragma once

namespace dreadcast {

struct CharacterClass {
    const char *name;
    const char *description;
    /// Lore / flavor for the character select detail panel (after mechanical description).
    const char *bio;
    /// Shown on the character-select detail panel (abilities / playstyle summary).
    const char *detailAbilities;
    /// Passive regen rates (applied every fixed tick while alive).
    float hpRegen{0.0F};
    float manaRegen{0.0F};
    /// Base stats at level start (no gear); used for character select readout.
    float baseMaxHp{100.0F};
    float baseMaxMana{100.0F};
    float meleeDamage{20.0F};
    float meleeRange{60.0F};
    float rangedDamage{10.0F};
    float rangedRange{800.0F};
    float moveSpeed{280.0F};
    float visionRange{500.0F};
};

inline constexpr CharacterClass AVAILABLE_CLASSES[] = {
    {"Undead Hunter",
     "Masters of cursed weaponry and relentless aggression.",
     "Raised among the ash-choked barrows, they trade sanity for certainty: every curse has a "
     "price, and they pay in blood so the dead stay buried.",
     "- Ranged curse bolt (LMB) — mana cost, hits at range.\n"
     "- Three-hit melee combo (RMB) — frontal cone, hold to loop, per-hit knockback.\n"
     "- [1] Lead Fever — 6s: shots become 4 randomly scattered pellets (full damage) with "
     "knockback; 25 mana, 20s cooldown.\n"
     "- [2] Deadlight Snare — throw a net forward, dash back; on hit, pull nearby foes together and "
     "stun 2s; 20 mana, 20s cooldown.\n"
     "- [3] Calamity Slug — channel 1s, then a huge piercing shot for 50 damage with sideways "
     "knockback; 30 mana, 25s cooldown.\n"
     "- Built for aggression: close the gap, manage mana, punish mistakes.",
     0.5F,
     0.5F,
     100.0F,
     100.0F,
     20.0F,
     60.0F,
     10.0F,
     800.0F,
     280.0F,
     500.0F},
};
inline constexpr int CLASS_COUNT = 1;

} // namespace dreadcast
