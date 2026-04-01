#pragma once

#include <raylib.h>

#include "core/input.hpp"
#include "game/items.hpp"

namespace dreadcast {
class ResourceManager;
}

namespace dreadcast::ui {

/// Result of inventory interaction (e.g. drop item to ground).
struct InventoryAction {
    enum Type { None, Drop, Use };
    Type type{Type::None};
    int itemIndex{-1};
    bool dropFromEquipped{false};
    int bagSlot{-1};
    EquipSlot equipSlot{EquipSlot::Armor};

    // Consumable usage (vials etc.)
    int useBagSlot{-1};          // which bag slot to consume from (carried)
    int useConsumableSlot{-1};  // which consumable slot to consume from (equipped)
};

/// Tab-toggle inventory: equipment, consumables, carried grid; drag-and-drop and context menu.
class InventoryUI {
  public:
    void toggle() {
        open_ = !open_;
        if (open_) {
            contextOpen_ = false;
            rarityInfoOpen_ = false;
        } else {
            dragging_ = false;
            dragItemIndex_ = -1;
            dragSourceBag_ = -1;
            dragSourceEquip_ = -1;
            dragSourceConsumable_ = -1;
        }
    }
    [[nodiscard]] bool isOpen() const { return open_; }
    void setOpen(bool v) {
        open_ = v;
        if (open_) {
            contextOpen_ = false;
            rarityInfoOpen_ = false;
        } else {
            dragging_ = false;
            dragItemIndex_ = -1;
            dragSourceBag_ = -1;
            dragSourceEquip_ = -1;
            dragSourceConsumable_ = -1;
        }
    }

    InventoryAction update(InputManager &input, InventoryState &inv);

    void draw(const Font &font, dreadcast::ResourceManager &resources, int screenW, int screenH,
              const InventoryState &inv);

  private:
    bool open_{false};

    static constexpr const char *kSlotLabels[] = {"Armor", "Amulet", "Ring"};

    void tryEquipFromBag(InventoryState &inv, int bagIndex);
    void tryUnequip(InventoryState &inv, EquipSlot slot);
    void moveEquippedToBagSlot(InventoryState &inv, EquipSlot slot, int bagIdx);
    void swapBagSlots(InventoryState &inv, int a, int b);

    bool dragging_{false};
    int dragItemIndex_{-1};
    int dragSourceBag_{-1};
    int dragSourceEquip_{-1};
    int dragSourceConsumable_{-1};

    bool contextOpen_{false};
    Rectangle contextRect_{};
    Rectangle contextOpt0_{};
    Rectangle contextOpt1_{};
    Rectangle contextOpt2_{};
    int contextItemIndex_{-1};
    int contextBagSlot_{-1};
    int contextEquipSlot_{-1};
    int contextConsumableSlot_{-1};
    bool contextIsCarried_{true};
    bool contextOpt0IsEquip_{true};

    bool rarityInfoOpen_{false};
};

} // namespace dreadcast::ui
