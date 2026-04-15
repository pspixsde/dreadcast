#pragma once

#include <string>

namespace dreadcast {

/// Character definition loaded from `assets/data/characters.json` (see `game_data.hpp`).
struct CharacterClass {
    std::string id;
    std::string name;
    std::string description;
    /// Lore / flavor for the character select detail panel.
    std::string bio;
    /// Abilities / playstyle summary (character select).
    std::string detailAbilities;
    float hpRegen{0.0F};
    float manaRegen{0.0F};
    float baseMaxHp{100.0F};
    float baseMaxMana{100.0F};
    float meleeDamage{20.0F};
    float meleeRange{60.0F};
    float rangedDamage{10.0F};
    float rangedRange{800.0F};
    float moveSpeed{280.0F};
    float visionRange{500.0F};
    float levelMaxHpGain{10.0F};
    float levelMaxManaGain{10.0F};
    float levelProjectileDamageGain{5.0F};
    float levelMeleeDamageGain{5.0F};
};

} // namespace dreadcast
