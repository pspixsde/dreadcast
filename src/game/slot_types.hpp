#pragma once

#include <cstdint>

#include "game/items.hpp"

namespace dreadcast {

/// Where an item may be placed in the player inventory UI (bag / gear / quick-use).
enum class PlayerSlotKind : uint8_t { Bag = 0, Equip = 1, Consumable = 2 };

/// Stable reference to one player inventory cell (pool index lives in `InventoryState` arrays).
struct PlayerSlotRef {
    PlayerSlotKind kind{PlayerSlotKind::Bag};
    /// Bag: 0..BAG_SLOT_COUNT-1. Equip: 0=Armor,1=Amulet,2=Ring. Consumable: 0..CONSUMABLE_SLOT_COUNT-1.
    int index{0};

    [[nodiscard]] static PlayerSlotRef bag(int i) { return {PlayerSlotKind::Bag, i}; }
    [[nodiscard]] static PlayerSlotRef equip(EquipSlot s) {
        return {PlayerSlotKind::Equip, static_cast<int>(s)};
    }
    [[nodiscard]] static PlayerSlotRef consumable(int i) { return {PlayerSlotKind::Consumable, i}; }
};

/// Extended slot kinds for workbench / world (used by transaction helpers and UI descriptors).
enum class SlotKind : uint8_t {
    Bag,
    Armor,
    Amulet,
    Ring,
    Consumable,
    AnvilForgeInput,
    AnvilDisassembleInput,
    AnvilOutput,
};

[[nodiscard]] bool slotAcceptsItem(SlotKind slot, const ItemData &item);

[[nodiscard]] int *playerSlotPoolIndexMut(InventoryState &inv, PlayerSlotRef r);
[[nodiscard]] const int *playerSlotPoolIndexConst(const InventoryState &inv, PlayerSlotRef r);

} // namespace dreadcast
