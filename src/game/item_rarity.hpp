#pragma once

#include <raylib.h>
#include <string>

#include "game/items.hpp"

namespace dreadcast {

[[nodiscard]] inline const char *rarityName(ItemRarity r) {
    switch (r) {
    case ItemRarity::Tarnished:
        return "Tarnished";
    case ItemRarity::Blighted:
        return "Blighted";
    case ItemRarity::Cursed:
        return "Cursed";
    case ItemRarity::Dread:
        return "Dread";
    case ItemRarity::Abyssal:
        return "Abyssal";
    case ItemRarity::Clouded:
        return "Clouded";
    case ItemRarity::Lucid:
        return "Lucid";
    case ItemRarity::Absolute:
        return "Absolute";
    case ItemRarity::Special:
        return "Special";
    default:
        return "Tarnished";
    }
}

/// Display color for rarity (borders, tooltip accent, slot tint).
[[nodiscard]] inline Color rarityColor(ItemRarity r) {
    switch (r) {
    case ItemRarity::Tarnished:
    case ItemRarity::Clouded:
        return {165, 155, 140, 255};
    case ItemRarity::Blighted:
        return {110, 200, 125, 255};
    case ItemRarity::Cursed:
    case ItemRarity::Lucid:
        return {110, 165, 235, 255};
    case ItemRarity::Dread:
        return {200, 115, 255, 255};
    case ItemRarity::Abyssal:
    case ItemRarity::Absolute:
        return {255, 200, 95, 255};
    case ItemRarity::Special:
        return {255, 75, 75, 255};
    default:
        return {180, 180, 180, 255};
    }
}

/// Max stack per slot for consumable rarities (Clouded … Special).
[[nodiscard]] inline int maxStackForConsumableRarity(ItemRarity r) {
    switch (r) {
    case ItemRarity::Clouded:
        return 5;
    case ItemRarity::Lucid:
        return 3;
    case ItemRarity::Absolute:
    case ItemRarity::Special:
        return 1;
    default:
        return 1;
    }
}

/// Tooltip / UI single rarity line (name only).
[[nodiscard]] inline std::string rarityLine(const ItemData &it) { return std::string(rarityName(it.rarity)); }

} // namespace dreadcast
