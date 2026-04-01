#pragma once

#include <raylib.h>
#include <string>

#include "game/items.hpp"

namespace dreadcast {

[[nodiscard]] inline const char *rarityTierName(ItemRarity r) {
    switch (r) {
    case ItemRarity::Common:
        return "Common";
    case ItemRarity::Uncommon:
        return "Uncommon";
    case ItemRarity::Rare:
        return "Rare";
    case ItemRarity::Epic:
        return "Epic";
    case ItemRarity::Legendary:
        return "Legendary";
    case ItemRarity::Special:
        return "Special";
    default:
        return "Common";
    }
}

/// Display color for rarity (borders, tooltip accent).
[[nodiscard]] inline Color rarityColor(ItemRarity r) {
    switch (r) {
    case ItemRarity::Common:
        return {165, 155, 140, 255};
    case ItemRarity::Uncommon:
        return {110, 200, 125, 255};
    case ItemRarity::Rare:
        return {110, 165, 235, 255};
    case ItemRarity::Epic:
        return {200, 115, 255, 255};
    case ItemRarity::Legendary:
        return {255, 200, 95, 255};
    case ItemRarity::Special:
        return {255, 130, 190, 255};
    default:
        return {180, 180, 180, 255};
    }
}

/// Style name for gear (non-consumable): Tarnished, Blighted, …
[[nodiscard]] inline std::string gearRarityStyleName(ItemRarity r) {
    switch (r) {
    case ItemRarity::Common:
        return "Tarnished";
    case ItemRarity::Uncommon:
        return "Blighted";
    case ItemRarity::Rare:
        return "Cursed";
    case ItemRarity::Epic:
        return "Dread";
    case ItemRarity::Legendary:
        return "Abyssal";
    case ItemRarity::Special:
        return "Anomalous";
    default:
        return "Tarnished";
    }
}

/// Style name for vial-style consumables by tier.
[[nodiscard]] inline std::string vialRarityStyleName(ItemRarity r) {
    switch (r) {
    case ItemRarity::Common:
        return "Clouded [Vial]";
    case ItemRarity::Uncommon:
        return "Lucid [Vial]";
    case ItemRarity::Rare:
        return "Absolute [Vial]";
    case ItemRarity::Epic:
        return "Dread [Vial]";
    case ItemRarity::Legendary:
        return "Abyssal [Vial]";
    case ItemRarity::Special:
        return "Anomalous [Vial]";
    default:
        return "Clouded [Vial]";
    }
}

[[nodiscard]] inline std::string rarityStyleName(const ItemData &it) {
    return it.isConsumable ? vialRarityStyleName(it.rarity) : gearRarityStyleName(it.rarity);
}

/// Single line for tooltips, e.g. "Tarnished (Common)".
[[nodiscard]] inline std::string rarityLine(const ItemData &it) {
    return rarityStyleName(it) + " (" + rarityTierName(it.rarity) + ")";
}

} // namespace dreadcast
