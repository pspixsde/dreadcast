#pragma once

#include <array>
#include <string>
#include <vector>

namespace dreadcast {

enum class EquipSlot { Armor, Amulet, Ring, COUNT };

struct ItemData {
    std::string name;
    EquipSlot slot{EquipSlot::Armor};
    bool isConsumable{false};
    bool isStackable{false};
    int maxStack{1};
    int stackCount{1};
    float maxHpBonus{0.0F};
    std::string description{};

    [[nodiscard]] bool canStackWith(const ItemData &other) const {
        return isStackable && other.isStackable && isConsumable && other.isConsumable &&
               name == other.name && maxStack == other.maxStack;
    }
};

inline constexpr int BAG_SLOT_COUNT = 9;
inline constexpr int CONSUMABLE_SLOT_COUNT = 2;

/// Run inventory: equipment + bag + consumable row; items stored in `items` pool, slots reference
/// indices (-1 = empty).
struct InventoryState {
    std::vector<ItemData> items{};
    std::array<int, static_cast<size_t>(EquipSlot::COUNT)> equipped{};
    std::array<int, BAG_SLOT_COUNT> bagSlots{};
    std::array<int, CONSUMABLE_SLOT_COUNT> consumableSlots{};

    InventoryState() {
        equipped.fill(-1);
        bagSlots.fill(-1);
        consumableSlots.fill(-1);
    }

    [[nodiscard]] int addItem(ItemData item) {
        items.push_back(std::move(item));
        return static_cast<int>(items.size()) - 1;
    }

    [[nodiscard]] int firstEmptyBagSlot() const {
        for (size_t i = 0; i < bagSlots.size(); ++i) {
            if (bagSlots[i] < 0) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    [[nodiscard]] int firstEmptyConsumableSlot() const {
        for (size_t i = 0; i < consumableSlots.size(); ++i) {
            if (consumableSlots[i] < 0) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /// Sum of `maxHpBonus` from all equipped items.
    [[nodiscard]] float totalEquippedMaxHpBonus() const {
        float sum = 0.0F;
        for (size_t i = 0; i < equipped.size(); ++i) {
            const int idx = equipped[i];
            if (idx >= 0 && idx < static_cast<int>(items.size())) {
                sum += items[static_cast<size_t>(idx)].maxHpBonus;
            }
        }
        return sum;
    }

    /// Rewrites slot indices that equal `from` to `to` (-1 ignored).
    void rewriteItemIndex(int from, int to) {
        for (size_t i = 0; i < equipped.size(); ++i) {
            if (equipped[i] == from) {
                equipped[i] = to;
            }
        }
        for (size_t i = 0; i < bagSlots.size(); ++i) {
            if (bagSlots[i] == from) {
                bagSlots[i] = to;
            }
        }
        for (size_t i = 0; i < consumableSlots.size(); ++i) {
            if (consumableSlots[i] == from) {
                consumableSlots[i] = to;
            }
        }
    }

    /// Removes item at index by swap-with-last; updates all slot references.
    void removeItemAtIndex(int idx) {
        if (idx < 0 || idx >= static_cast<int>(items.size())) {
            return;
        }
        const int last = static_cast<int>(items.size()) - 1;
        if (idx != last) {
            rewriteItemIndex(last, idx);
            items[static_cast<size_t>(idx)] = std::move(items[static_cast<size_t>(last)]);
        }
        items.pop_back();
    }
};

} // namespace dreadcast
