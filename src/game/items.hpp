#pragma once

#include <array>
#include <string>
#include <vector>

#include "game/item_effects_types.hpp"

namespace dreadcast {

enum class EquipSlot { Armor, Amulet, Ring, COUNT };

/// Rarity is one enum for UI and saves. Gear: Tarnished … Abyssal, or **Special** (odd rules).
/// Vials: Clouded … Special (see `isConsumableRarityTier` in `item_rarity.hpp`).
enum class ItemRarity : uint8_t {
    Tarnished = 0,
    Blighted,
    Cursed,
    Dread,
    Abyssal,
    Clouded,
    Lucid,
    Absolute,
    Special
};

struct ItemData {
    /// Matches `assets/data/items.json` `id` / map `ITEM` kind (empty for legacy items).
    std::string catalogId{};
    std::string name;
    std::string iconPath{};
    EquipSlot slot{EquipSlot::Armor};
    ItemRarity rarity{ItemRarity::Tarnished};
    bool isConsumable{false};
    bool isStackable{false};
    int maxStack{1};
    int stackCount{1};
    float maxHpBonus{0.0F};
    /// Bonus to max mana while equipped (rings etc.).
    float maxManaBonus{0.0F};
    /// Passive HP/s while equipped (armor etc.).
    float hpRegenBonus{0.0F};
    /// Flat move speed bonus while equipped.
    float moveSpeedBonus{0.0F};
    /// Flat fog visibility radius bonus while equipped.
    float visionRangeBonus{0.0F};
    /// Fraction of incoming damage reflected to attacker (instant, not projectile).
    float damageReflectPercent{0.0F};
    /// Multiplier on ability mana cost while equipped (1.0 = no change). Stacking is multiplicative
    /// across equipped items (product).
    float abilityManaCostMultiplier{1.0F};
    /// Mana restored when any ability cooldown finishes (per equipped item; sums).
    float abilityCooldownManaRefund{0.0F};
    std::string description{};
    /// Data-driven triggers/actions (see `assets/data/items.json`). Filled at load; may be
    /// synthesized from legacy numeric fields when JSON has no `effects` array.
    std::vector<ItemEffectDef> effects{};

    [[nodiscard]] bool canStackWith(const ItemData &other) const {
        if (!isStackable || !other.isStackable || !isConsumable || !other.isConsumable ||
            maxStack != other.maxStack) {
            return false;
        }
        if (!catalogId.empty() && !other.catalogId.empty()) {
            return catalogId == other.catalogId;
        }
        return name == other.name;
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
