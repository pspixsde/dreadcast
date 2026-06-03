#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "game/game_data.hpp"
#include "game/item_transaction.hpp"

namespace dreadcast {

struct RecipeMatch {
    const Recipe *recipe{nullptr};
    /// Pool index and amount to subtract from that stack (full removal when amount == stack).
    std::vector<std::pair<int, int>> takes{};
};

struct CraftingContext {
    const int *playerLevel{nullptr};
};

struct TransformPlan {
    RecipeMatch match;
};

[[nodiscard]] bool recipeConditionsMet(const Recipe &recipe, const CraftingContext &ctx);

[[nodiscard]] std::optional<RecipeMatch> tryMatchForgeBench(const std::vector<int> &benchPoolIndices,
                                                            const InventoryState &inventory,
                                                            const std::vector<Recipe> &forgeRecipes,
                                                            const CraftingContext &ctx = {});

[[nodiscard]] std::optional<RecipeMatch> tryMatchDisassembleInput(int inputPoolIndex,
                                                                  const InventoryState &inventory,
                                                                  const std::vector<Recipe> &disRecipes,
                                                                  const CraftingContext &ctx = {});

[[nodiscard]] TxResult applyRecipeMatch(InvCtx &ctx, const RecipeMatch &match, MovePolicy placePolicy);

/// Validates `recipe` conditions then runs `applyRecipeMatch` plus registered `sideEffects`.
[[nodiscard]] TxResult executeTransform(InvCtx &ctx, TransformPlan &plan, MovePolicy placePolicy,
                                      const CraftingContext &craftCtx = {});

} // namespace dreadcast
