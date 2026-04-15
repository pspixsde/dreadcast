#pragma once

#include <array>
#include <string>

#include "game/character.hpp"
#include "game/items.hpp"

namespace dreadcast {

/// One ability row for HUD / tooltips / mana and cooldown checks (Undead Hunter bar).
struct AbilityDef {
    std::string name;
    std::string description;
    std::string iconPath;
    float manaCost{0.0F};
    float cooldown{0.0F};
};

struct CharacterAbilities {
    std::array<AbilityDef, 3> abilities{};
};

/// Loads `assets/data/items.json` and `assets/data/abilities.json` (also tries next to executable).
/// Safe to call more than once; last successful load wins. Logs on failure and keeps prior data.
[[nodiscard]] bool loadGameData();

[[nodiscard]] bool gameDataLoaded();

/// Map `ITEM` / editor kind string → item instance. Empty item if unknown id.
[[nodiscard]] ItemData makeItemFromMapKind(const std::string &kind);

/// Undead Hunter ability bar (must call `loadGameData()` before gameplay).
[[nodiscard]] const CharacterAbilities &undeadHunterAbilities();

/// Loaded characters (`assets/data/characters.json`). Always at least one after `loadGameData()`.
[[nodiscard]] int characterCount();

[[nodiscard]] const CharacterClass &characterAt(int index);

} // namespace dreadcast
