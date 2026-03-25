#pragma once

namespace dreadcast {

struct CharacterClass {
    const char *name;
    const char *description;
    /// Shown on the character-select detail panel (abilities / playstyle summary).
    const char *detailAbilities;
};

inline constexpr CharacterClass AVAILABLE_CLASSES[] = {
    {"Undead Hunter", "Masters of cursed weaponry and relentless aggression.",
     "• Ranged curse bolt (LMB) — mana cost, hits at range.\n"
     "• Relentless melee cleave (RMB) — cone damage and knockback.\n"
     "• Built for aggression: close the gap, manage mana, punish mistakes."},
};
inline constexpr int CLASS_COUNT = 1;

} // namespace dreadcast
