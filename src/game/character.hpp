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
     "• Built for aggression: close the gap, manage mana, punish mistakes.",
     0.5F, 0.5F},
};
inline constexpr int CLASS_COUNT = 1;

} // namespace dreadcast
