#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "game/character.hpp"
#include "game/enemy_archetype.hpp"
#include "game/items.hpp"

namespace dreadcast {

/// Defined in `game/map_data.hpp`; opaque-declared here so loot rolling can take it by value.
enum class CasketTier : std::uint8_t;

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

/// One ingredient line for forging / disassembly / outputs.
struct CraftIngredient {
    std::string itemId{};
    int count{1};
};

enum class RecipeKind : std::uint8_t { Forge, Disassemble };

struct CraftCondition {
    std::string kind{};
    std::string param{};
    float value{0.0F};
};

struct CraftSideEffect {
    std::string kind{};
    std::string param{};
    float value{0.0F};
};

/// Unified transformation definition (`kind` selects matcher/applier behavior).
struct Recipe {
    std::string id{};
    RecipeKind kind{RecipeKind::Forge};
    std::vector<CraftIngredient> inputs{};
    std::vector<CraftCondition> conditions{};
    std::vector<CraftIngredient> outputs{};
    std::vector<CraftSideEffect> sideEffects{};
};

/// Loads `assets/data/items.json` and `assets/data/abilities.json` (also tries next to executable).
/// Safe to call more than once; last successful load wins. Logs on failure and keeps prior data.
[[nodiscard]] bool loadGameData();

[[nodiscard]] bool gameDataLoaded();

/// Map `ITEM` / editor kind string → item instance. Empty item if unknown id.
[[nodiscard]] ItemData makeItemFromMapKind(const std::string &kind);

/// Rolls one casket's loot for the given tier: returns up to three catalog ids (trailing entries
/// empty when the casket rolls fewer than three items). Honors the tier's item-count odds, per-slot
/// rarity odds, and minimum-rarity guarantee. Bridged rarity names (Tarnished/Clouded,
/// Cursed/Lucid, Abyssal/Absolute) share one pool. **Special**-rarity items are never included.
[[nodiscard]] std::array<std::string, 3> rollCasketLoot(CasketTier tier);

/// All items from the loaded catalog (order not guaranteed). Refreshed by `loadGameData()`.
[[nodiscard]] const std::vector<ItemData> &allCatalogItems();

/// Undead Hunter ability bar (must call `loadGameData()` before gameplay).
[[nodiscard]] const CharacterAbilities &undeadHunterAbilities();

/// Loaded characters (`assets/data/characters.json`). Always at least one after `loadGameData()`.
[[nodiscard]] int characterCount();

[[nodiscard]] const CharacterClass &characterAt(int index);

[[nodiscard]] const std::vector<Recipe> &allRecipes();

[[nodiscard]] const std::vector<Recipe> &forgeRecipes();

[[nodiscard]] const std::vector<Recipe> &disassembleRecipes();

[[nodiscard]] const Recipe *findRecipeById(const std::string &id);

/// First disassemble recipe whose primary input `itemId` matches `sourceCatalogId`.
[[nodiscard]] const Recipe *findDisassembleRecipeBySourceId(const std::string &sourceCatalogId);

[[nodiscard]] bool catalogIdIsForgeBenchInput(const std::string &catalogId);

[[nodiscard]] bool catalogIdIsDisassembleBenchInput(const std::string &catalogId);

/// Loaded enemy archetypes (`assets/data/enemies.json`). Required for `loadGameData()` success.
[[nodiscard]] const EnemyArchetype *enemyArchetypeById(const std::string &id);

[[nodiscard]] const std::vector<EnemyArchetype> &allEnemyArchetypes();

[[nodiscard]] const EnemyAiGlobals &enemyAiGlobals();

} // namespace dreadcast
