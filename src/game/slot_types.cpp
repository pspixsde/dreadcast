#include "game/slot_types.hpp"

#include "game/game_data.hpp"

namespace dreadcast {

bool slotAcceptsItem(SlotKind slot, const ItemData &item) {
    switch (slot) {
    case SlotKind::Bag:
        return true;
    case SlotKind::Armor:
        return !item.isConsumable && item.slot == EquipSlot::Armor;
    case SlotKind::Amulet:
        return !item.isConsumable && item.slot == EquipSlot::Amulet;
    case SlotKind::Ring:
        return !item.isConsumable && item.slot == EquipSlot::Ring;
    case SlotKind::Consumable:
        return item.isConsumable;
    case SlotKind::AnvilForgeInput:
        return catalogIdIsForgeBenchInput(item.catalogId);
    case SlotKind::AnvilDisassembleInput:
        return catalogIdIsDisassembleBenchInput(item.catalogId);
    case SlotKind::AnvilOutput:
        return false;
    }
    return false;
}

int *playerSlotPoolIndexMut(InventoryState &inv, PlayerSlotRef r) {
    switch (r.kind) {
    case PlayerSlotKind::Bag:
        if (r.index < 0 || r.index >= BAG_SLOT_COUNT) {
            return nullptr;
        }
        return &inv.bagSlots[static_cast<size_t>(r.index)];
    case PlayerSlotKind::Equip:
        if (r.index < 0 || r.index >= static_cast<int>(EquipSlot::COUNT)) {
            return nullptr;
        }
        return &inv.equipped[static_cast<size_t>(r.index)];
    case PlayerSlotKind::Consumable:
        if (r.index < 0 || r.index >= CONSUMABLE_SLOT_COUNT) {
            return nullptr;
        }
        return &inv.consumableSlots[static_cast<size_t>(r.index)];
    }
    return nullptr;
}

const int *playerSlotPoolIndexConst(const InventoryState &inv, PlayerSlotRef r) {
    return playerSlotPoolIndexMut(const_cast<InventoryState &>(inv), r);
}

} // namespace dreadcast
