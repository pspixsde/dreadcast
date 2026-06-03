#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "game/items.hpp"

namespace dreadcast {

enum class TxResult : std::uint8_t { Ok, Failed, Partial };

enum class StackMergeOrder : std::uint8_t { BagFirstThenConsumable, ConsumableFirstThenBag };

/// Bit flags for `move` / placement behavior.
enum class MovePolicy : std::uint32_t {
    None = 0,
    /// Prefer merging into bag before consumable row when pouring stackables.
    BagPriorityShift = 1u << 0,
    /// Try `mergeItemStacks` into destination before swap.
    TryMerge = 1u << 1,
    /// Allow swap when merge impossible and both slots hold items.
    AllowSwap = 1u << 2,
};

[[nodiscard]] inline MovePolicy operator|(MovePolicy a, MovePolicy b) {
    return static_cast<MovePolicy>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] inline bool any(MovePolicy a, MovePolicy mask) {
    return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(mask)) != 0u;
}

/// Policy for `returnPoolItem` (anvil return / overflow).
enum class ReturnPolicy : std::uint32_t {
    None = 0,
    /// Match gameplay **bag priority shift** setting: bag pour order first, skip consumable-row
    /// empty-slot preference before bag.
    BagPriorityShift = 1u << 0,
};

[[nodiscard]] inline ReturnPolicy operator|(ReturnPolicy a, ReturnPolicy b) {
    return static_cast<ReturnPolicy>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] inline bool any(ReturnPolicy a, ReturnPolicy mask) {
    return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(mask)) != 0u;
}

/// After `InventoryState::removeItemAtIndex(removedIdx)` (swap-with-last), fix external pool
/// index references (`removedIdx` receives what was at `oldLastIdx`).
class IndexHolderRegistry {
  public:
    using Rewriter = std::function<void(int removedIdx, int oldLastIdx)>;

    void clear() { rewriters_.clear(); }
    void add(Rewriter fn) { rewriters_.push_back(std::move(fn)); }

    void notifyPoolSwapRemove(int removedIdx, int oldLastIdx) const {
        for (const Rewriter &fn : rewriters_) {
            if (fn) {
                fn(removedIdx, oldLastIdx);
            }
        }
    }

  private:
    std::vector<Rewriter> rewriters_{};
};

enum class EndpointKind : std::uint8_t {
    Bag,
    Equip,
    Consumable,
    AnvilForgeInput,
    AnvilDisassembleInput,
    AnvilDisassembleOutputPool,
    WorldGround,
    PoolOnly,
};

struct Endpoint {
    EndpointKind kind{EndpointKind::Bag};
    int index{-1};
    EquipSlot equip{EquipSlot::Armor};
    entt::entity worldEntity{entt::null};
};

struct InvCtx {
    InventoryState *inventory{nullptr};
    entt::registry *registry{nullptr};
    entt::entity player{entt::null};
    float *inventoryFullFlashTimer{nullptr};
    IndexHolderRegistry *holders{nullptr};

    std::array<int, 6> *forgeSlots{nullptr};
    int *disassembleInputIndex{nullptr};
    std::array<int, 6> *disassembleOutputPool{nullptr};
    int *disassembleOutputCount{nullptr};
};

[[nodiscard]] int mergeItemStacks(ItemData &src, ItemData &dst);

[[nodiscard]] int pourPickupIntoInventorySlots(InventoryState &inventory, int pickupPoolIndex,
                                               StackMergeOrder order);

[[nodiscard]] int pourStackableIntoBagConsumableSlots(InventoryState &inventory, int poolIdx,
                                                      StackMergeOrder order);

[[nodiscard]] int poolIndexAt(const InvCtx &ctx, const Endpoint &ep);

void clearEndpoint(InvCtx &ctx, const Endpoint &ep);
void setEndpointPool(InvCtx &ctx, const Endpoint &ep, int poolIdx);

[[nodiscard]] TxResult move(InvCtx &ctx, Endpoint src, Endpoint dst,
                            MovePolicy policy = MovePolicy::TryMerge | MovePolicy::AllowSwap);

[[nodiscard]] TxResult swapEndpoints(InvCtx &ctx, Endpoint a, Endpoint b);

[[nodiscard]] TxResult removePoolItem(InvCtx &ctx, int poolIdx);

[[nodiscard]] TxResult returnPoolItem(InvCtx &ctx, int poolIdx, ReturnPolicy policy);

[[nodiscard]] TxResult dropToWorld(InvCtx &ctx, int poolIdx, Vector2 worldPos);

[[nodiscard]] TxResult dropToWorldAtPlayer(InvCtx &ctx, int poolIdx);

[[nodiscard]] TxResult pickupFromWorld(InvCtx &ctx, entt::entity pickupEntity);

[[nodiscard]] TxResult equipGearFromBagSlot(InvCtx &ctx, int bagIndex);

[[nodiscard]] TxResult tryEquipConsumableFromBagSlot(InvCtx &ctx, int bagIndex);

[[nodiscard]] TxResult tryUnequipConsumableToBagSlot(InvCtx &ctx, int consumableSlotIndex);

[[nodiscard]] TxResult tryUnequipGearToBag(InvCtx &ctx, EquipSlot slot);

[[nodiscard]] TxResult moveEquippedGearToBagSlot(InvCtx &ctx, EquipSlot slot, int bagIdx);

[[nodiscard]] TxResult swapBagSlotIndices(InvCtx &ctx, int bagA, int bagB);

/// Allocate a new pool row and merge it into inventory (bag/consumables/equip) or overflow like
/// `returnPoolItem`.
[[nodiscard]] TxResult grantItemData(InvCtx &ctx, ItemData item, ReturnPolicy policy);

/// Pool-only allocation for spawning world pickups (`ItemPickup` owns the pool index afterwards).
[[nodiscard]] int allocatePickupPoolItem(InvCtx &ctx, ItemData item);

/** Split **one** unit from a stack into a **new pool row**. If bag has room, decrement source and
 * place split row in bag. Otherwise returns `Partial` with `outNewPoolIndex` filled — caller drops
 * or removes that row itself.
 */
[[nodiscard]] TxResult splitStackableOneUnitToNewPool(InvCtx &ctx, const Endpoint &srcSlot,
                                                       int *outNewPoolIndex);

#ifndef NDEBUG
void debugValidateInventoryInvariants(const InvCtx &ctx);
#else
inline void debugValidateInventoryInvariants(const InvCtx &) {}
#endif

/// Debug: full checks; release: no-op. Invoked once after `loadGameData()`.
void runItemTransactionSelfTest();

} // namespace dreadcast
