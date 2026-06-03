#include "game/crafting.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <raylib.h>

#include "game/game_data.hpp"

namespace dreadcast {

namespace {

void applyCraftSideEffects(const Recipe *recipe) {
    if (recipe == nullptr) {
        return;
    }
    for (const CraftSideEffect &se : recipe->sideEffects) {
        if (se.kind.empty()) {
            continue;
        }
        TraceLog(LOG_WARNING, "Dreadcast: recipe \"%s\" sideEffect kind \"%s\" not implemented.",
                 recipe->id.c_str(), se.kind.c_str());
    }
}

} // namespace

bool recipeConditionsMet(const Recipe &recipe, const CraftingContext &ctx) {
    for (const CraftCondition &cond : recipe.conditions) {
        const std::string &k = cond.kind;
        if (k.empty() || k == "always") {
            continue;
        }
        if (k == "playerLevelAtLeast") {
            const int need = static_cast<int>(std::floor(cond.value + 0.5f));
            if (ctx.playerLevel == nullptr || *ctx.playerLevel < need) {
                return false;
            }
            continue;
        }
        TraceLog(LOG_WARNING, "Dreadcast: recipe \"%s\" unknown condition kind \"%s\".",
                 recipe.id.c_str(), k.c_str());
    }
    return true;
}

namespace {

[[nodiscard]] bool mapStringIntEqual(const std::unordered_map<std::string, int> &a,
                                    const std::unordered_map<std::string, int> &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const auto &kv : a) {
        const auto it = b.find(kv.first);
        if (it == b.end() || it->second != kv.second) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::unordered_map<std::string, int> aggregateBench(
    const InventoryState &inv, const std::vector<int> &benchPoolIndices) {
    std::unordered_map<std::string, int> totals;
    for (const int poolIdx : benchPoolIndices) {
        if (poolIdx < 0 || poolIdx >= static_cast<int>(inv.items.size())) {
            continue;
        }
        const ItemData &it = inv.items[static_cast<size_t>(poolIdx)];
        if (it.catalogId.empty()) {
            continue;
        }
        totals[it.catalogId] += it.stackCount;
    }
    return totals;
}

[[nodiscard]] std::unordered_map<std::string, int> aggregateRecipeInputs(const Recipe &recipe) {
    std::unordered_map<std::string, int> totals;
    for (const CraftIngredient &in : recipe.inputs) {
        if (!in.itemId.empty()) {
            totals[in.itemId] += in.count;
        }
    }
    return totals;
}

[[nodiscard]] std::vector<std::pair<int, int>> buildTakesForForgeMatch(
    const InventoryState &inv, const std::vector<int> &benchPoolIndices, const Recipe &recipe) {
    struct Cell {
        int poolIdx{-1};
        int remain{0};
        std::string id{};
    };
    std::vector<Cell> cells;
    cells.reserve(benchPoolIndices.size());
    for (const int poolIdx : benchPoolIndices) {
        if (poolIdx < 0 || poolIdx >= static_cast<int>(inv.items.size())) {
            continue;
        }
        const ItemData &it = inv.items[static_cast<size_t>(poolIdx)];
        if (it.catalogId.empty() || it.stackCount <= 0) {
            continue;
        }
        cells.push_back(Cell{poolIdx, it.stackCount, it.catalogId});
    }

    std::vector<std::pair<int, int>> takes;
    for (const CraftIngredient &ing : recipe.inputs) {
        int need = ing.count;
        while (need > 0) {
            Cell *pick = nullptr;
            for (Cell &c : cells) {
                if (c.remain > 0 && c.id == ing.itemId) {
                    pick = &c;
                    break;
                }
            }
            if (pick == nullptr) {
                return {};
            }
            const int grab = std::min(need, pick->remain);
            takes.push_back({pick->poolIdx, grab});
            pick->remain -= grab;
            need -= grab;
        }
    }
    return takes;
}

void clearForgeSlotsHoldingPool(InvCtx &ctx, int poolIdx) {
    if (ctx.forgeSlots == nullptr) {
        return;
    }
    for (int &fs : *ctx.forgeSlots) {
        if (fs == poolIdx) {
            fs = -1;
        }
    }
}

} // namespace

std::optional<RecipeMatch> tryMatchForgeBench(const std::vector<int> &benchPoolIndices,
                                             const InventoryState &inventory,
                                             const std::vector<Recipe> &forgeRecipes,
                                             const CraftingContext &) {
    const auto benchTotals = aggregateBench(inventory, benchPoolIndices);
    if (benchTotals.empty()) {
        return std::nullopt;
    }

    for (const Recipe &recipe : forgeRecipes) {
        if (recipe.kind != RecipeKind::Forge) {
            continue;
        }
        const auto needTotals = aggregateRecipeInputs(recipe);
        if (!mapStringIntEqual(benchTotals, needTotals)) {
            continue;
        }
        std::vector<std::pair<int, int>> takes =
            buildTakesForForgeMatch(inventory, benchPoolIndices, recipe);
        if (takes.empty() && !recipe.inputs.empty()) {
            continue;
        }
        return RecipeMatch{&recipe, std::move(takes)};
    }
    return std::nullopt;
}

std::optional<RecipeMatch> tryMatchDisassembleInput(int inputPoolIndex,
                                                   const InventoryState &inventory,
                                                   const std::vector<Recipe> &disRecipes,
                                                   const CraftingContext &) {
    if (inputPoolIndex < 0 || inputPoolIndex >= static_cast<int>(inventory.items.size())) {
        return std::nullopt;
    }
    const ItemData &it = inventory.items[static_cast<size_t>(inputPoolIndex)];
    if (it.catalogId.empty()) {
        return std::nullopt;
    }
    for (const Recipe &recipe : disRecipes) {
        if (recipe.kind != RecipeKind::Disassemble || recipe.inputs.empty()) {
            continue;
        }
        if (recipe.inputs[0].itemId != it.catalogId) {
            continue;
        }
        const int need = std::max(1, recipe.inputs[0].count);
        if (it.stackCount < need) {
            continue;
        }
        return RecipeMatch{&recipe, std::vector<std::pair<int, int>>{{inputPoolIndex, it.stackCount}}};
    }
    return std::nullopt;
}

TxResult applyRecipeMatch(InvCtx &ctx, const RecipeMatch &match, MovePolicy placePolicy) {
    if (ctx.inventory == nullptr || match.recipe == nullptr) {
        return TxResult::Failed;
    }
    InventoryState &inv = *ctx.inventory;

    if (match.recipe->kind == RecipeKind::Forge && ctx.forgeSlots != nullptr) {
        ctx.forgeSlots->fill(-1);
    }

    if (match.recipe->kind == RecipeKind::Disassemble && ctx.disassembleOutputPool != nullptr &&
        ctx.disassembleOutputCount != nullptr) {
        for (int i = 0; i < *ctx.disassembleOutputCount; ++i) {
            const int idx = (*ctx.disassembleOutputPool)[static_cast<size_t>(i)];
            if (idx >= 0) {
                (void)returnPoolItem(ctx, idx, ReturnPolicy::None);
            }
            (*ctx.disassembleOutputPool)[static_cast<size_t>(i)] = -1;
        }
        *ctx.disassembleOutputCount = 0;
    }

    std::unordered_map<int, int> takeAgg;
    for (const auto &tk : match.takes) {
        takeAgg[tk.first] += tk.second;
    }
    std::vector<std::pair<int, int>> takes;
    takes.reserve(takeAgg.size());
    for (const auto &kv : takeAgg) {
        takes.push_back(kv);
    }
    std::sort(takes.begin(), takes.end(),
              [](const std::pair<int, int> &a, const std::pair<int, int> &b) { return a.first > b.first; });

    for (const auto &tk : takes) {
        const int poolIdx = tk.first;
        const int amount = tk.second;
        if (poolIdx < 0 || poolIdx >= static_cast<int>(inv.items.size()) || amount <= 0) {
            continue;
        }
        ItemData &it = inv.items[static_cast<size_t>(poolIdx)];
        if (amount >= it.stackCount) {
            clearForgeSlotsHoldingPool(ctx, poolIdx);
            if (ctx.disassembleInputIndex != nullptr && *ctx.disassembleInputIndex == poolIdx) {
                *ctx.disassembleInputIndex = -1;
            }
            if (ctx.disassembleOutputPool != nullptr && ctx.disassembleOutputCount != nullptr) {
                for (int i = 0; i < *ctx.disassembleOutputCount; ++i) {
                    int &s = (*ctx.disassembleOutputPool)[static_cast<size_t>(i)];
                    if (s == poolIdx) {
                        s = -1;
                    }
                }
            }
            (void)removePoolItem(ctx, poolIdx);
        } else {
            it.stackCount -= amount;
        }
    }

    if (match.recipe->kind == RecipeKind::Forge) {
        if (match.recipe->outputs.empty()) {
            return TxResult::Failed;
        }
        const CraftIngredient &out0 = match.recipe->outputs[0];
        ItemData out = makeItemFromMapKind(out0.itemId);
        if (out.name.empty()) {
            return TxResult::Failed;
        }
        out.stackCount = std::max(1, out0.count);
        const int newIdx = inv.addItem(std::move(out));
        ReturnPolicy rp = any(placePolicy, MovePolicy::BagPriorityShift)
                              ? ReturnPolicy::BagPriorityShift
                              : ReturnPolicy::None;
        return returnPoolItem(ctx, newIdx, rp);
    }

    if (match.recipe->kind == RecipeKind::Disassemble) {
        int mult = 1;
        if (!match.recipe->inputs.empty()) {
            const int denom = std::max(1, match.recipe->inputs[0].count);
            if (!match.takes.empty()) {
                mult = std::max(1, match.takes[0].second / denom);
            }
        }
        for (const CraftIngredient &outg : match.recipe->outputs) {
            int total = outg.count * mult;
            while (total > 0) {
                ItemData piece = makeItemFromMapKind(outg.itemId);
                if (piece.name.empty()) {
                    break;
                }
                const int chunk =
                    piece.isStackable ? std::min(total, std::max(1, piece.maxStack)) : 1;
                piece.stackCount = chunk;
                total -= chunk;
                const int ni = inv.addItem(std::move(piece));
                if (ctx.disassembleOutputPool != nullptr && ctx.disassembleOutputCount != nullptr &&
                    *ctx.disassembleOutputCount < static_cast<int>(ctx.disassembleOutputPool->size())) {
                    (*ctx.disassembleOutputPool)[static_cast<size_t>(*ctx.disassembleOutputCount)] = ni;
                    ++(*ctx.disassembleOutputCount);
                } else {
                    ReturnPolicy rp = any(placePolicy, MovePolicy::BagPriorityShift)
                                          ? ReturnPolicy::BagPriorityShift
                                          : ReturnPolicy::None;
                    (void)returnPoolItem(ctx, ni, rp);
                }
            }
        }
        return TxResult::Ok;
    }

    return TxResult::Failed;
}

TxResult executeTransform(InvCtx &ctx, TransformPlan &plan, MovePolicy placePolicy, const CraftingContext &craftCtx) {
    if (plan.match.recipe == nullptr) {
        return TxResult::Failed;
    }
    if (!recipeConditionsMet(*plan.match.recipe, craftCtx)) {
        return TxResult::Failed;
    }
    const TxResult t = applyRecipeMatch(ctx, plan.match, placePolicy);
    if (t == TxResult::Ok) {
        applyCraftSideEffects(plan.match.recipe);
    }
    return t;
}

} // namespace dreadcast
