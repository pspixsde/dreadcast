#pragma once

namespace dreadcast {

struct CharacterClass {
    const char *name;
    const char *description;
    /// Shown on the character-select detail panel (abilities / playstyle summary).
    const char *detailAbilities;
    /// Passive regen rates (applied every fixed tick while alive).
    float hpRegen{0.0F};
    float manaRegen{0.0F};
};

inline constexpr CharacterClass AVAILABLE_CLASSES[] = {
    {"Undead Hunter", "Masters of cursed weaponry and relentless aggression.",
     "• Ranged curse bolt (LMB) — mana cost, hits at range.\n"
     "• Three-hit melee combo (RMB) — frontal cone, hold to loop, per-hit knockback.\n"
     "• [1] Lead Fever — 6s: shots become 4 scattered pellets (half damage each) with knockback; "
     "25 mana, 20s cooldown.\n"
     "• [2] Deadlight Snare — throw a net forward, dash back; on hit, pull nearby foes together and "
     "stun 2s; 20 mana, 20s cooldown.\n"
     "• [3] Calamity Slug — channel 1s, then a huge piercing shot for 50 damage with sideways knockback; "
     "30 mana, 25s cooldown.\n"
     "• Built for aggression: close the gap, manage mana, punish mistakes.",
     0.5F, 0.5F},
};
inline constexpr int CLASS_COUNT = 1;

} // namespace dreadcast
