#pragma once

#include <array>
#include <string>
#include <vector>

#include "game/character.hpp"
#include "game/items.hpp"

namespace dreadcast {

/// One ability row for HUD text/icons and runtime mechanics (Undead Hunter bar).
struct AbilityDef {
    std::string name;
    std::string description;
    std::string iconPath;
    float manaCost{0.0F};
    float cooldown{0.0F};
    float effectDuration{0.0F};
    int pelletCount{1};
    float scatterAngle{0.0F};
    float scatterRandom{0.0F};
    float knockback{0.0F};
    float projectileSpeed{0.0F};
    float projectileRange{0.0F};
    float pullRadius{0.0F};
    float stunDuration{0.0F};
    float dashDistance{0.0F};
    float dashSpeed{0.0F};
    float aimDuration{0.0F};
    float damage{0.0F};
    float projectileSize{0.0F};
    float knockbackSide{0.0F};
};

struct CharacterAbilities {
    std::array<AbilityDef, 3> abilities{};
};

/// One ingredient line for forging / disassembly.
struct CraftIngredient {
    std::string itemId{};
    int count{1};
};

/// Forge: exact multiset of inputs → output.
struct ForgeRecipe {
    std::vector<CraftIngredient> inputs{};
    std::string outputId{};
    int outputCount{1};
};

/// Disassemble: one gear id → outputs (amounts multiplied by stack of input when claiming).
struct DisassembleRecipe {
    std::string sourceId{};
    std::vector<CraftIngredient> outputs{};
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

[[nodiscard]] const std::vector<ForgeRecipe> &forgeRecipes();

[[nodiscard]] const std::vector<DisassembleRecipe> &disassembleRecipes();

} // namespace dreadcast
