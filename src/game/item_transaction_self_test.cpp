#include "game/item_transaction.hpp"

#include <array>
#include <vector>

#include "game/crafting.hpp"
#include "game/game_data.hpp"

#ifndef NDEBUG
#include <cassert>
#endif

namespace dreadcast {

#ifndef NDEBUG

void runItemTransactionSelfTest() {
    if (!gameDataLoaded()) {
        return;
    }

    {
        InventoryState inv;
        ItemData v1 = makeItemFromMapKind("vial_raw_spirit");
        if (v1.name.empty()) {
            return;
        }
        v1.stackCount = 5;
        const int p1 = inv.addItem(std::move(v1));
        inv.bagSlots[0] = p1;

        ItemData v2 = makeItemFromMapKind("vial_raw_spirit");
        v2.stackCount = 3;
        const int p2 = inv.addItem(std::move(v2));
        inv.bagSlots[1] = p2;

        InvCtx ctx{};
        ctx.inventory = &inv;
        const Endpoint src{EndpointKind::Bag, 1};
        const Endpoint dst{EndpointKind::Bag, 0};
        assert(move(ctx, src, dst, MovePolicy::TryMerge | MovePolicy::AllowSwap) == TxResult::Ok);
        debugValidateInventoryInvariants(ctx);
    }

    {
        InventoryState inv;
        ItemData blood = makeItemFromMapKind("vial_pure_blood");
        ItemData armor = makeItemFromMapKind("iron_armor");
        if (blood.name.empty() || armor.name.empty()) {
            return;
        }
        const int piBlood = inv.addItem(std::move(blood));
        const int piArmor = inv.addItem(std::move(armor));

        std::array<int, 6> forgeSlots{};
        forgeSlots.fill(-1);
        forgeSlots[0] = piBlood;
        forgeSlots[1] = piArmor;
        std::vector<int> bench;
        for (const int s : forgeSlots) {
            if (s >= 0) {
                bench.push_back(s);
            }
        }

        const auto match = tryMatchForgeBench(bench, inv, forgeRecipes(), {});
        assert(match.has_value());

        InvCtx ctx{};
        ctx.inventory = &inv;
        ctx.forgeSlots = &forgeSlots;
        int disIn = -1;
        std::array<int, 6> disOut{};
        disOut.fill(-1);
        int disCnt = 0;
        ctx.disassembleInputIndex = &disIn;
        ctx.disassembleOutputPool = &disOut;
        ctx.disassembleOutputCount = &disCnt;

        TransformPlan plan{};
        plan.match = *match;
        assert(executeTransform(ctx, plan, MovePolicy::TryMerge | MovePolicy::AllowSwap, {}) ==
               TxResult::Ok);
        debugValidateInventoryInvariants(ctx);
    }

    {
        InventoryState inv;
        ItemData vial = makeItemFromMapKind("vial_raw_spirit");
        if (vial.name.empty()) {
            return;
        }
        vial.stackCount = 2;
        const int pool = inv.addItem(std::move(vial));
        InvCtx ctx{};
        ctx.inventory = &inv;
        const int idxPickup = allocatePickupPoolItem(ctx, makeItemFromMapKind("vial_raw_spirit"));
        assert(idxPickup >= 0);
        assert(pool != idxPickup);
    }
}

#else

void runItemTransactionSelfTest() {}

#endif

} // namespace dreadcast
