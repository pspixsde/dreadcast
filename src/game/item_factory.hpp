#pragma once

#include <string>

#include "game/item_rarity.hpp"
#include "game/items.hpp"

namespace dreadcast {

/// Builds `ItemData` for editor map spawns and gameplay. `kind` matches `.map` `ITEM` lines.
[[nodiscard]] inline ItemData makeItemFromMapKind(const std::string &kind) {
    if (kind == "iron_armor") {
        ItemData armor{};
        armor.name = "Iron Armor";
        armor.iconPath = "assets/textures/items/iron_armor_icon.png";
        armor.slot = EquipSlot::Armor;
        armor.rarity = ItemRarity::Tarnished;
        armor.maxHpBonus = 10.0F;
        armor.description = "+10 Max HP";
        return armor;
    }
    if (kind == "barbed_tunic") {
        ItemData armor{};
        armor.name = "Barbed Tunic";
        armor.iconPath = "assets/textures/items/barbed_tunic_icon.png";
        armor.slot = EquipSlot::Armor;
        armor.rarity = ItemRarity::Blighted;
        armor.hpRegenBonus = 0.3F;
        armor.damageReflectPercent = 0.10F;
        armor.description = "+0.3 HP/s\nReflects 10% of incoming damage back to the source.";
        return armor;
    }
    if (kind == "vial_pure_blood") {
        ItemData vile{};
        vile.name = "Vial of Pure Blood";
        vile.iconPath = "assets/textures/items/vial_pure_blood_icon.png";
        vile.rarity = ItemRarity::Clouded;
        vile.isConsumable = true;
        vile.isStackable = true;
        vile.maxStack = maxStackForConsumableRarity(ItemRarity::Clouded);
        vile.stackCount = 1;
        vile.description = "Regenerates 40 HP over 8 seconds.";
        return vile;
    }
    if (kind == "runic_shell") {
        ItemData armor{};
        armor.name = "Runic Shell";
        armor.iconPath = "assets/textures/items/runic_shell_icon.png";
        armor.slot = EquipSlot::Armor;
        armor.rarity = ItemRarity::Cursed;
        armor.maxHpBonus = 25.0F;
        armor.description =
            "+25 Max HP\n"
            "Below 30% HP: releases an energy shockwave dealing 30 damage,\n"
            "knocking back enemies, and healing 30 HP. 30s cooldown.";
        return armor;
    }
    if (kind == "vial_cordial_manic") {
        ItemData v{};
        v.name = "Vial of Cordial Manic";
        v.iconPath = "assets/textures/items/vial_cordial_manic_icon.png";
        v.rarity = ItemRarity::Lucid;
        v.isConsumable = true;
        v.isStackable = true;
        v.maxStack = maxStackForConsumableRarity(ItemRarity::Lucid);
        v.stackCount = 1;
        v.description =
            "Doubles movement speed and grants invincibility for 7 seconds.\n"
            "No HP regen and slowly lose 40% of max HP during the effect.\n"
            "Cannot be used below 40% max HP.";
        return v;
    }
    return {};
}

} // namespace dreadcast
