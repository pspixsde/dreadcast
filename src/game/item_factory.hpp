#pragma once

#include <string>

#include "game/items.hpp"

namespace dreadcast {

/// Builds `ItemData` for editor map spawns and gameplay. `kind` matches `.map` `ITEM` lines.
[[nodiscard]] inline ItemData makeItemFromMapKind(const std::string &kind) {
    if (kind == "iron_armor") {
        ItemData armor{};
        armor.name = "Iron Armor";
        armor.iconPath = "assets/textures/items/iron_armor_icon.png";
        armor.slot = EquipSlot::Armor;
        armor.rarity = ItemRarity::Common;
        armor.maxHpBonus = 10.0F;
        armor.description =
            "Tarnished arms — used, rusted, and discarded by the masses.\n\n+10 Max HP";
        return armor;
    }
    if (kind == "vial_pure_blood") {
        ItemData vile{};
        vile.name = "Vial of Pure Blood";
        vile.iconPath = "assets/textures/items/vial_pure_blood_icon.png";
        vile.rarity = ItemRarity::Common;
        vile.isConsumable = true;
        vile.isStackable = true;
        vile.maxStack = 5;
        vile.stackCount = 1;
        vile.description = "Clouded essence — the liquid is murky and full of sediment. It heals, "
                           "but the impurities leave a bitter aftertaste of madness.\n\n"
                           "Regenerates 40 HP over 8 seconds.";
        return vile;
    }
    return {};
}

} // namespace dreadcast
