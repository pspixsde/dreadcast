#include "game/item_transaction.hpp"

#include <algorithm>

#include "ecs/components.hpp"
#include "ecs/systems/collision_system.hpp"

#include "game/slot_types.hpp"

namespace dreadcast {

namespace {

void rewriteForgeAndBenchSlots(InvCtx &ctx, int removedIdx, int oldLastIdx) {
    if (removedIdx == oldLastIdx) {
        return;
    }
    auto rewriteSlot = [&](int &slot) {
        if (slot == removedIdx) {
            slot = -1;
        } else if (slot == oldLastIdx) {
            slot = removedIdx;
        }
    };
    if (ctx.forgeSlots != nullptr) {
        for (int &fs : *ctx.forgeSlots) {
            rewriteSlot(fs);
        }
    }
    if (ctx.disassembleInputIndex != nullptr) {
        rewriteSlot(*ctx.disassembleInputIndex);
    }
    if (ctx.disassembleOutputPool != nullptr && ctx.disassembleOutputCount != nullptr) {
        for (int i = 0; i < *ctx.disassembleOutputCount; ++i) {
            rewriteSlot((*ctx.disassembleOutputPool)[static_cast<size_t>(i)]);
        }
    }
}

[[nodiscard]] bool endpointRequiresSlotValidation(EndpointKind k) {
    switch (k) {
    case EndpointKind::Bag:
    case EndpointKind::Equip:
    case EndpointKind::Consumable:
    case EndpointKind::AnvilForgeInput:
    case EndpointKind::AnvilDisassembleInput:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] SlotKind slotKindForPlacement(const Endpoint &ep) {
    switch (ep.kind) {
    case EndpointKind::Bag:
        return SlotKind::Bag;
    case EndpointKind::Equip:
        switch (ep.equip) {
        case EquipSlot::Armor:
            return SlotKind::Armor;
        case EquipSlot::Amulet:
            return SlotKind::Amulet;
        case EquipSlot::Ring:
            return SlotKind::Ring;
        default:
            return SlotKind::Bag;
        }
    case EndpointKind::Consumable:
        return SlotKind::Consumable;
    case EndpointKind::AnvilForgeInput:
        return SlotKind::AnvilForgeInput;
    case EndpointKind::AnvilDisassembleInput:
        return SlotKind::AnvilDisassembleInput;
    default:
        return SlotKind::Bag;
    }
}

} // namespace

int mergeItemStacks(ItemData &src, ItemData &dst) {
    if (!src.canStackWith(dst)) {
        return 0;
    }
    const int space = dst.maxStack - dst.stackCount;
    if (space <= 0 || src.stackCount <= 0) {
        return 0;
    }
    const int move = std::min(space, src.stackCount);
    dst.stackCount += move;
    src.stackCount -= move;
    return move;
}

namespace {

void pourIntoPoolIndex(InventoryState &inventory, int pickupPoolIndex, int dstPoolIdx) {
    if (dstPoolIdx < 0 || dstPoolIdx >= static_cast<int>(inventory.items.size()) ||
        pickupPoolIndex < 0 || pickupPoolIndex >= static_cast<int>(inventory.items.size())) {
        return;
    }
    ItemData &dst = inventory.items[static_cast<size_t>(dstPoolIdx)];
    ItemData &src = inventory.items[static_cast<size_t>(pickupPoolIndex)];
    (void)mergeItemStacks(src, dst);
}

void pourBagThenConsumable(InventoryState &inventory, int pickupPoolIndex) {
    for (size_t i = 0; i < inventory.bagSlots.size(); ++i) {
        if (inventory.items[static_cast<size_t>(pickupPoolIndex)].stackCount <= 0) {
            break;
        }
        pourIntoPoolIndex(inventory, pickupPoolIndex, inventory.bagSlots[i]);
    }
    for (size_t i = 0; i < inventory.consumableSlots.size(); ++i) {
        if (inventory.items[static_cast<size_t>(pickupPoolIndex)].stackCount <= 0) {
            break;
        }
        pourIntoPoolIndex(inventory, pickupPoolIndex, inventory.consumableSlots[i]);
    }
}

void pourConsumableThenBag(InventoryState &inventory, int pickupPoolIndex) {
    for (size_t i = 0; i < inventory.consumableSlots.size(); ++i) {
        if (inventory.items[static_cast<size_t>(pickupPoolIndex)].stackCount <= 0) {
            break;
        }
        pourIntoPoolIndex(inventory, pickupPoolIndex, inventory.consumableSlots[i]);
    }
    for (size_t i = 0; i < inventory.bagSlots.size(); ++i) {
        if (inventory.items[static_cast<size_t>(pickupPoolIndex)].stackCount <= 0) {
            break;
        }
        pourIntoPoolIndex(inventory, pickupPoolIndex, inventory.bagSlots[i]);
    }
}

} // namespace

int pourStackableIntoBagConsumableSlots(InventoryState &inventory, int poolIdx,
                                        StackMergeOrder order) {
    if (poolIdx < 0 || poolIdx >= static_cast<int>(inventory.items.size())) {
        return 0;
    }
    const auto &ref = inventory.items[static_cast<size_t>(poolIdx)];
    if (!ref.isStackable) {
        return ref.stackCount;
    }
    if (order == StackMergeOrder::BagFirstThenConsumable) {
        pourBagThenConsumable(inventory, poolIdx);
    } else {
        pourConsumableThenBag(inventory, poolIdx);
    }
    return inventory.items[static_cast<size_t>(poolIdx)].stackCount;
}

int pourPickupIntoInventorySlots(InventoryState &inventory, int pickupPoolIndex,
                                 StackMergeOrder order) {
    if (pickupPoolIndex < 0 || pickupPoolIndex >= static_cast<int>(inventory.items.size())) {
        return 0;
    }
    const auto &pickupItemRef = inventory.items[static_cast<size_t>(pickupPoolIndex)];
    if (!pickupItemRef.isStackable || !pickupItemRef.isConsumable) {
        return pickupItemRef.stackCount;
    }
    return pourStackableIntoBagConsumableSlots(inventory, pickupPoolIndex, order);
}

int poolIndexAt(const InvCtx &ctx, const Endpoint &ep) {
    if (ctx.inventory == nullptr) {
        return -1;
    }
    const InventoryState &inv = *ctx.inventory;
    switch (ep.kind) {
    case EndpointKind::Bag:
        if (ep.index < 0 || ep.index >= BAG_SLOT_COUNT) {
            return -1;
        }
        return inv.bagSlots[static_cast<size_t>(ep.index)];
    case EndpointKind::Equip: {
        const int si = static_cast<int>(ep.equip);
        if (si < 0 || si >= static_cast<int>(EquipSlot::COUNT)) {
            return -1;
        }
        return inv.equipped[static_cast<size_t>(si)];
    }
    case EndpointKind::Consumable:
        if (ep.index < 0 || ep.index >= CONSUMABLE_SLOT_COUNT) {
            return -1;
        }
        return inv.consumableSlots[static_cast<size_t>(ep.index)];
    case EndpointKind::AnvilForgeInput:
        if (ctx.forgeSlots == nullptr || ep.index < 0 ||
            ep.index >= static_cast<int>(ctx.forgeSlots->size())) {
            return -1;
        }
        return (*ctx.forgeSlots)[static_cast<size_t>(ep.index)];
    case EndpointKind::AnvilDisassembleInput:
        if (ctx.disassembleInputIndex == nullptr) {
            return -1;
        }
        (void)ep.index;
        return *ctx.disassembleInputIndex;
    case EndpointKind::AnvilDisassembleOutputPool:
        if (ctx.disassembleOutputPool == nullptr || ep.index < 0 ||
            ep.index >= static_cast<int>(ctx.disassembleOutputPool->size())) {
            return -1;
        }
        return (*ctx.disassembleOutputPool)[static_cast<size_t>(ep.index)];
    case EndpointKind::WorldGround:
        if (ctx.registry == nullptr || !ctx.registry->valid(ep.worldEntity) ||
            !ctx.registry->all_of<ecs::ItemPickup>(ep.worldEntity)) {
            return -1;
        }
        return ctx.registry->get<ecs::ItemPickup>(ep.worldEntity).itemIndex;
    case EndpointKind::PoolOnly:
        if (ep.index < 0 || ep.index >= static_cast<int>(inv.items.size())) {
            return -1;
        }
        return ep.index;
    }
    return -1;
}

void clearEndpoint(InvCtx &ctx, const Endpoint &ep) {
    if (ctx.inventory == nullptr) {
        return;
    }
    InventoryState &inv = *ctx.inventory;
    switch (ep.kind) {
    case EndpointKind::Bag:
        if (ep.index >= 0 && ep.index < BAG_SLOT_COUNT) {
            inv.bagSlots[static_cast<size_t>(ep.index)] = -1;
        }
        break;
    case EndpointKind::Equip: {
        const int si = static_cast<int>(ep.equip);
        if (si >= 0 && si < static_cast<int>(EquipSlot::COUNT)) {
            inv.equipped[static_cast<size_t>(si)] = -1;
        }
        break;
    }
    case EndpointKind::Consumable:
        if (ep.index >= 0 && ep.index < CONSUMABLE_SLOT_COUNT) {
            inv.consumableSlots[static_cast<size_t>(ep.index)] = -1;
        }
        break;
    case EndpointKind::AnvilForgeInput:
        if (ctx.forgeSlots != nullptr && ep.index >= 0 &&
            ep.index < static_cast<int>(ctx.forgeSlots->size())) {
            (*ctx.forgeSlots)[static_cast<size_t>(ep.index)] = -1;
        }
        break;
    case EndpointKind::AnvilDisassembleInput:
        if (ctx.disassembleInputIndex != nullptr) {
            *ctx.disassembleInputIndex = -1;
        }
        break;
    case EndpointKind::AnvilDisassembleOutputPool:
        if (ctx.disassembleOutputPool != nullptr && ep.index >= 0 &&
            ep.index < static_cast<int>(ctx.disassembleOutputPool->size())) {
            (*ctx.disassembleOutputPool)[static_cast<size_t>(ep.index)] = -1;
        }
        break;
    case EndpointKind::WorldGround:
        // Pickup entity destroyed elsewhere
        break;
    case EndpointKind::PoolOnly:
        break;
    }
}

void setEndpointPool(InvCtx &ctx, const Endpoint &ep, int poolIdx) {
    if (ctx.inventory == nullptr) {
        return;
    }
    InventoryState &inv = *ctx.inventory;
    switch (ep.kind) {
    case EndpointKind::Bag:
        if (ep.index >= 0 && ep.index < BAG_SLOT_COUNT) {
            inv.bagSlots[static_cast<size_t>(ep.index)] = poolIdx;
        }
        break;
    case EndpointKind::Equip: {
        const int si = static_cast<int>(ep.equip);
        if (si >= 0 && si < static_cast<int>(EquipSlot::COUNT)) {
            inv.equipped[static_cast<size_t>(si)] = poolIdx;
        }
        break;
    }
    case EndpointKind::Consumable:
        if (ep.index >= 0 && ep.index < CONSUMABLE_SLOT_COUNT) {
            inv.consumableSlots[static_cast<size_t>(ep.index)] = poolIdx;
        }
        break;
    case EndpointKind::AnvilForgeInput:
        if (ctx.forgeSlots != nullptr && ep.index >= 0 &&
            ep.index < static_cast<int>(ctx.forgeSlots->size())) {
            (*ctx.forgeSlots)[static_cast<size_t>(ep.index)] = poolIdx;
        }
        break;
    case EndpointKind::AnvilDisassembleInput:
        if (ctx.disassembleInputIndex != nullptr) {
            *ctx.disassembleInputIndex = poolIdx;
        }
        break;
    case EndpointKind::AnvilDisassembleOutputPool:
        if (ctx.disassembleOutputPool != nullptr && ep.index >= 0 &&
            ep.index < static_cast<int>(ctx.disassembleOutputPool->size())) {
            (*ctx.disassembleOutputPool)[static_cast<size_t>(ep.index)] = poolIdx;
        }
        break;
    case EndpointKind::WorldGround:
        if (ctx.registry != nullptr && ctx.registry->valid(ep.worldEntity) &&
            ctx.registry->all_of<ecs::ItemPickup>(ep.worldEntity)) {
            ctx.registry->get<ecs::ItemPickup>(ep.worldEntity).itemIndex = poolIdx;
        }
        break;
    case EndpointKind::PoolOnly:
        (void)poolIdx;
        break;
    }
}

TxResult removePoolItem(InvCtx &ctx, int poolIdx) {
    if (ctx.inventory == nullptr || poolIdx < 0 ||
        poolIdx >= static_cast<int>(ctx.inventory->items.size())) {
        return TxResult::Failed;
    }
    const int oldLast = static_cast<int>(ctx.inventory->items.size()) - 1;
    ctx.inventory->removeItemAtIndex(poolIdx);
    if (ctx.registry != nullptr) {
        dreadcast::ecs::collision::rewrite_ground_pickup_indices_after_remove(*ctx.registry, poolIdx,
                                                                             oldLast);
    }
    rewriteForgeAndBenchSlots(ctx, poolIdx, oldLast);
    if (ctx.holders != nullptr) {
        ctx.holders->notifyPoolSwapRemove(poolIdx, oldLast);
    }
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult returnPoolItem(InvCtx &ctx, int poolIdx, ReturnPolicy policy) {
    if (ctx.inventory == nullptr || poolIdx < 0 ||
        poolIdx >= static_cast<int>(ctx.inventory->items.size())) {
        return TxResult::Failed;
    }
    InventoryState &inv = *ctx.inventory;
    const bool bagPriority = any(policy, ReturnPolicy::BagPriorityShift);
    auto &it = inv.items[static_cast<size_t>(poolIdx)];

    if (it.isStackable && it.stackCount > 0) {
        const StackMergeOrder order = bagPriority ? StackMergeOrder::BagFirstThenConsumable
                                                  : StackMergeOrder::ConsumableFirstThenBag;
        (void)pourStackableIntoBagConsumableSlots(inv, poolIdx, order);

        if (inv.items[static_cast<size_t>(poolIdx)].stackCount <= 0) {
            (void)removePoolItem(ctx, poolIdx);
            return TxResult::Ok;
        }
    }

    if (!bagPriority) {
        if (it.isConsumable) {
            const int c = inv.firstEmptyConsumableSlot();
            if (c >= 0) {
                inv.consumableSlots[static_cast<size_t>(c)] = poolIdx;
                debugValidateInventoryInvariants(ctx);
                return TxResult::Ok;
            }
        } else {
            const int eq = static_cast<int>(it.slot);
            if (eq >= 0 && eq < static_cast<int>(inv.equipped.size()) &&
                inv.equipped[static_cast<size_t>(eq)] < 0) {
                inv.equipped[static_cast<size_t>(eq)] = poolIdx;
                debugValidateInventoryInvariants(ctx);
                return TxResult::Ok;
            }
        }
    }
    const int bag = inv.firstEmptyBagSlot();
    if (bag >= 0) {
        inv.bagSlots[static_cast<size_t>(bag)] = poolIdx;
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    if (ctx.registry != nullptr && ctx.registry->valid(ctx.player) &&
        ctx.registry->all_of<ecs::Transform>(ctx.player)) {
        const Vector2 pos = ctx.registry->get<ecs::Transform>(ctx.player).position;
        return dropToWorld(ctx, poolIdx, pos);
    }
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult dropToWorld(InvCtx &ctx, int poolIdx, Vector2 worldPos) {
    if (ctx.registry == nullptr || poolIdx < 0 ||
        poolIdx >= static_cast<int>(ctx.inventory->items.size())) {
        return TxResult::Failed;
    }
    dreadcast::ecs::collision::spawn_item_pickup_at_world(*ctx.registry, worldPos, poolIdx);
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult dropToWorldAtPlayer(InvCtx &ctx, int poolIdx) {
    if (ctx.registry == nullptr || !ctx.registry->valid(ctx.player) ||
        !ctx.registry->all_of<ecs::Transform>(ctx.player)) {
        return TxResult::Failed;
    }
    const Vector2 pos = ctx.registry->get<ecs::Transform>(ctx.player).position;
    return dropToWorld(ctx, poolIdx, pos);
}

TxResult pickupFromWorld(InvCtx &ctx, entt::entity p) {
    if (ctx.inventory == nullptr || ctx.registry == nullptr || !ctx.registry->valid(p) ||
        !ctx.registry->all_of<ecs::ItemPickup, ecs::Transform, ecs::Sprite>(p)) {
        return TxResult::Failed;
    }

    InventoryState &inventory = *ctx.inventory;
    entt::registry &registry = *ctx.registry;

    const int pickupIndex = registry.get<ecs::ItemPickup>(p).itemIndex;
    if (pickupIndex < 0 || pickupIndex >= static_cast<int>(inventory.items.size())) {
        registry.destroy(p);
        return TxResult::Ok;
    }

    const int originalStack = inventory.items[static_cast<size_t>(pickupIndex)].stackCount;

    {
        const auto &pickupItemRef = inventory.items[static_cast<size_t>(pickupIndex)];
        if (pickupItemRef.isStackable && pickupItemRef.isConsumable) {
            (void)pourPickupIntoInventorySlots(inventory, pickupIndex,
                                               StackMergeOrder::BagFirstThenConsumable);
        }
    }

    const int remaining = inventory.items[static_cast<size_t>(pickupIndex)].stackCount;
    if (remaining <= 0) {
        registry.destroy(p);
        (void)removePoolItem(ctx, pickupIndex);
        return TxResult::Ok;
    }

    const int bag = inventory.firstEmptyBagSlot();
    if (bag < 0) {
        if (remaining == originalStack && ctx.inventoryFullFlashTimer != nullptr) {
            *ctx.inventoryFullFlashTimer = 1.2F;
        }
        return TxResult::Failed;
    }

    inventory.bagSlots[static_cast<size_t>(bag)] = pickupIndex;
    registry.destroy(p);
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult move(InvCtx &ctx, Endpoint src, Endpoint dst, MovePolicy policy) {
    if (ctx.inventory == nullptr) {
        return TxResult::Failed;
    }
    const int srcPool = poolIndexAt(ctx, src);
    if (srcPool < 0) {
        return TxResult::Failed;
    }
    const int dstPool = poolIndexAt(ctx, dst);

    if (dstPool < 0) {
        if (endpointRequiresSlotValidation(dst.kind)) {
            const ItemData &moving = ctx.inventory->items[static_cast<size_t>(srcPool)];
            if (!slotAcceptsItem(slotKindForPlacement(dst), moving)) {
                return TxResult::Failed;
            }
        }
        clearEndpoint(ctx, src);
        setEndpointPool(ctx, dst, srcPool);
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }

    if (any(policy, MovePolicy::TryMerge)) {
        ItemData &s = ctx.inventory->items[static_cast<size_t>(srcPool)];
        ItemData &d = ctx.inventory->items[static_cast<size_t>(dstPool)];
        const int moved = mergeItemStacks(s, d);
        if (moved > 0 && s.stackCount <= 0) {
            clearEndpoint(ctx, src);
            (void)removePoolItem(ctx, srcPool);
            debugValidateInventoryInvariants(ctx);
            return TxResult::Ok;
        }
    }

    if (!any(policy, MovePolicy::AllowSwap)) {
        return TxResult::Failed;
    }

    clearEndpoint(ctx, src);
    clearEndpoint(ctx, dst);
    setEndpointPool(ctx, dst, srcPool);
    setEndpointPool(ctx, src, dstPool);
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult swapEndpoints(InvCtx &ctx, Endpoint a, Endpoint b) {
    const int pa = poolIndexAt(ctx, a);
    const int pb = poolIndexAt(ctx, b);
    if (pa < 0 || pb < 0) {
        return TxResult::Failed;
    }
    if (ctx.inventory != nullptr) {
        const ItemData &itemA = ctx.inventory->items[static_cast<size_t>(pa)];
        const ItemData &itemB = ctx.inventory->items[static_cast<size_t>(pb)];
        if (endpointRequiresSlotValidation(b.kind)) {
            if (!slotAcceptsItem(slotKindForPlacement(b), itemA)) {
                return TxResult::Failed;
            }
        }
        if (endpointRequiresSlotValidation(a.kind)) {
            if (!slotAcceptsItem(slotKindForPlacement(a), itemB)) {
                return TxResult::Failed;
            }
        }
    }
    clearEndpoint(ctx, a);
    clearEndpoint(ctx, b);
    setEndpointPool(ctx, a, pb);
    setEndpointPool(ctx, b, pa);
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult equipGearFromBagSlot(InvCtx &ctx, int bagIndex) {
    if (ctx.inventory == nullptr || bagIndex < 0 || bagIndex >= BAG_SLOT_COUNT) {
        return TxResult::Failed;
    }
    const Endpoint src{EndpointKind::Bag, bagIndex};
    const int itemIdx = poolIndexAt(ctx, src);
    if (itemIdx < 0) {
        return TxResult::Failed;
    }
    if (ctx.inventory->items[static_cast<size_t>(itemIdx)].isConsumable) {
        return TxResult::Failed;
    }
    const EquipSlot targetSlot = ctx.inventory->items[static_cast<size_t>(itemIdx)].slot;
    const Endpoint dst{EndpointKind::Equip, -1, targetSlot};
    const int eqIdx = poolIndexAt(ctx, dst);
    if (eqIdx >= 0) {
        return swapEndpoints(ctx, src, dst);
    }
    return move(ctx, src, dst, MovePolicy::TryMerge | MovePolicy::AllowSwap);
}

TxResult tryEquipConsumableFromBagSlot(InvCtx &ctx, int bagIndex) {
    if (ctx.inventory == nullptr || bagIndex < 0 || bagIndex >= BAG_SLOT_COUNT) {
        return TxResult::Failed;
    }
    InventoryState &inv = *ctx.inventory;
    const int itemIdx = inv.bagSlots[static_cast<size_t>(bagIndex)];
    if (itemIdx < 0 || itemIdx >= static_cast<int>(inv.items.size())) {
        return TxResult::Failed;
    }
    if (!inv.items[static_cast<size_t>(itemIdx)].isConsumable) {
        return TxResult::Failed;
    }
    ItemData &src = inv.items[static_cast<size_t>(itemIdx)];
    for (int i = 0; i < CONSUMABLE_SLOT_COUNT; ++i) {
        const int dstIdx = inv.consumableSlots[static_cast<size_t>(i)];
        if (dstIdx < 0 || dstIdx >= static_cast<int>(inv.items.size())) {
            continue;
        }
        ItemData &dst = inv.items[static_cast<size_t>(dstIdx)];
        if (!dst.canStackWith(src) || dst.stackCount >= dst.maxStack) {
            continue;
        }
        const int moved = mergeItemStacks(src, dst);
        (void)moved;
        if (src.stackCount <= 0) {
            inv.bagSlots[static_cast<size_t>(bagIndex)] = -1;
            return removePoolItem(ctx, itemIdx);
        }
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    const int empty = inv.firstEmptyConsumableSlot();
    if (empty >= 0) {
        inv.consumableSlots[static_cast<size_t>(empty)] = itemIdx;
        inv.bagSlots[static_cast<size_t>(bagIndex)] = -1;
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    return TxResult::Failed;
}

TxResult tryUnequipConsumableToBagSlot(InvCtx &ctx, int consumableSlotIndex) {
    if (ctx.inventory == nullptr || consumableSlotIndex < 0 ||
        consumableSlotIndex >= CONSUMABLE_SLOT_COUNT) {
        return TxResult::Failed;
    }
    InventoryState &inv = *ctx.inventory;
    const int itemIdx = inv.consumableSlots[static_cast<size_t>(consumableSlotIndex)];
    if (itemIdx < 0 || itemIdx >= static_cast<int>(inv.items.size())) {
        return TxResult::Failed;
    }
    ItemData &src = inv.items[static_cast<size_t>(itemIdx)];
    for (int i = 0; i < BAG_SLOT_COUNT; ++i) {
        const int dstIdx = inv.bagSlots[static_cast<size_t>(i)];
        if (dstIdx < 0 || dstIdx >= static_cast<int>(inv.items.size())) {
            continue;
        }
        ItemData &dst = inv.items[static_cast<size_t>(dstIdx)];
        if (!dst.canStackWith(src) || dst.stackCount >= dst.maxStack) {
            continue;
        }
        (void)mergeItemStacks(src, dst);
        if (src.stackCount <= 0) {
            inv.consumableSlots[static_cast<size_t>(consumableSlotIndex)] = -1;
            return removePoolItem(ctx, itemIdx);
        }
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    const int bag = inv.firstEmptyBagSlot();
    if (bag < 0) {
        return TxResult::Failed;
    }
    inv.bagSlots[static_cast<size_t>(bag)] = itemIdx;
    inv.consumableSlots[static_cast<size_t>(consumableSlotIndex)] = -1;
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult tryUnequipGearToBag(InvCtx &ctx, EquipSlot slot) {
    if (ctx.inventory == nullptr) {
        return TxResult::Failed;
    }
    InventoryState &inv = *ctx.inventory;
    const int itemIdx = inv.equipped[static_cast<size_t>(slot)];
    if (itemIdx < 0) {
        return TxResult::Failed;
    }
    const int bag = inv.firstEmptyBagSlot();
    if (bag < 0) {
        return TxResult::Failed;
    }
    inv.bagSlots[static_cast<size_t>(bag)] = itemIdx;
    inv.equipped[static_cast<size_t>(slot)] = -1;
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult moveEquippedGearToBagSlot(InvCtx &ctx, EquipSlot slot, int bagIdx) {
    if (ctx.inventory == nullptr || bagIdx < 0 || bagIdx >= BAG_SLOT_COUNT) {
        return TxResult::Failed;
    }
    InventoryState &inv = *ctx.inventory;
    const int eIdx = inv.equipped[static_cast<size_t>(slot)];
    if (eIdx < 0) {
        return TxResult::Failed;
    }
    const int bIdx = inv.bagSlots[static_cast<size_t>(bagIdx)];
    if (bIdx < 0) {
        inv.bagSlots[static_cast<size_t>(bagIdx)] = eIdx;
        inv.equipped[static_cast<size_t>(slot)] = -1;
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    if (inv.items[static_cast<size_t>(bIdx)].slot == slot) {
        inv.equipped[static_cast<size_t>(slot)] = bIdx;
        inv.bagSlots[static_cast<size_t>(bagIdx)] = eIdx;
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    return TxResult::Failed;
}

TxResult swapBagSlotIndices(InvCtx &ctx, int bagA, int bagB) {
    if (ctx.inventory == nullptr || bagA < 0 || bagB < 0 || bagA >= BAG_SLOT_COUNT ||
        bagB >= BAG_SLOT_COUNT) {
        return TxResult::Failed;
    }
    std::swap(ctx.inventory->bagSlots[static_cast<size_t>(bagA)],
              ctx.inventory->bagSlots[static_cast<size_t>(bagB)]);
    debugValidateInventoryInvariants(ctx);
    return TxResult::Ok;
}

TxResult grantItemData(InvCtx &ctx, ItemData item, ReturnPolicy policy) {
    if (ctx.inventory == nullptr) {
        return TxResult::Failed;
    }
    const int idx = ctx.inventory->addItem(std::move(item));
    return returnPoolItem(ctx, idx, policy);
}

int allocatePickupPoolItem(InvCtx &ctx, ItemData item) {
    if (ctx.inventory == nullptr) {
        return -1;
    }
    return ctx.inventory->addItem(std::move(item));
}

TxResult splitStackableOneUnitToNewPool(InvCtx &ctx, const Endpoint &srcSlot, int *outNewPoolIndex) {
    if (ctx.inventory == nullptr || outNewPoolIndex == nullptr) {
        return TxResult::Failed;
    }
    *outNewPoolIndex = -1;
    const int srcPool = poolIndexAt(ctx, srcSlot);
    if (srcPool < 0 || srcPool >= static_cast<int>(ctx.inventory->items.size())) {
        return TxResult::Failed;
    }
    ItemData &srcIt = ctx.inventory->items[static_cast<size_t>(srcPool)];
    if (!srcIt.isStackable || srcIt.stackCount < 2) {
        return TxResult::Failed;
    }
    ItemData one = srcIt;
    one.stackCount = 1;
    --srcIt.stackCount;
    const int newIdx = ctx.inventory->addItem(std::move(one));
    *outNewPoolIndex = newIdx;
    const int emptyBag = ctx.inventory->firstEmptyBagSlot();
    if (emptyBag >= 0) {
        ctx.inventory->bagSlots[static_cast<size_t>(emptyBag)] = newIdx;
        debugValidateInventoryInvariants(ctx);
        return TxResult::Ok;
    }
    return TxResult::Partial;
}

#ifndef NDEBUG
void debugValidateInventoryInvariants(const InvCtx &ctx) {
    if (ctx.inventory == nullptr) {
        return;
    }
    const InventoryState &inv = *ctx.inventory;
    const int n = static_cast<int>(inv.items.size());
    std::vector<int> refCount;
    refCount.assign(static_cast<size_t>(std::max(0, n)), 0);

    auto mark = [&](int idx) {
        if (idx >= 0 && idx < n) {
            refCount[static_cast<size_t>(idx)] += 1;
        }
    };

    for (int idx : inv.equipped) {
        mark(idx);
    }
    for (int idx : inv.bagSlots) {
        mark(idx);
    }
    for (int idx : inv.consumableSlots) {
        mark(idx);
    }
    if (ctx.forgeSlots != nullptr) {
        for (int idx : *ctx.forgeSlots) {
            mark(idx);
        }
    }
    if (ctx.disassembleInputIndex != nullptr && *ctx.disassembleInputIndex >= 0) {
        mark(*ctx.disassembleInputIndex);
    }
    if (ctx.disassembleOutputPool != nullptr && ctx.disassembleOutputCount != nullptr) {
        for (int i = 0; i < *ctx.disassembleOutputCount; ++i) {
            mark((*ctx.disassembleOutputPool)[static_cast<size_t>(i)]);
        }
    }

    if (ctx.registry != nullptr) {
        const auto view = ctx.registry->view<ecs::ItemPickup>();
        for (const auto e : view) {
            mark(view.get<ecs::ItemPickup>(e).itemIndex);
        }
    }

    for (int i = 0; i < n; ++i) {
        const ItemData &it = inv.items[static_cast<size_t>(i)];
        if (it.stackCount < 1) {
            TraceLog(LOG_ERROR, "Dreadcast: invariant — item pool %d has stackCount %d", i,
                     it.stackCount);
        }
    }
    for (int i = 0; i < n; ++i) {
        if (refCount[static_cast<size_t>(i)] > 1) {
            TraceLog(LOG_ERROR, "Dreadcast: invariant — pool index %d referenced %d times", i,
                     refCount[static_cast<size_t>(i)]);
        }
    }
}
#endif

} // namespace dreadcast
