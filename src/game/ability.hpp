#pragma once

namespace dreadcast {

struct AbilityDef {
    const char *name;
    const char *description;
    const char *iconPath;
    float manaCost;
    float cooldown;
};

struct CharacterAbilities {
    AbilityDef abilities[3];
};

inline constexpr CharacterAbilities UNDEAD_HUNTER_ABILITIES{
    AbilityDef{"Lead Fever",
               "Overload your gun for 6s: ranged attacks fire 4 pellets in a loose, random scatter "
               "that knock enemies back (full damage per pellet).\nMana: 25  Cooldown: 20s",
               "assets/textures/abilities/lead_fever.png", 25.0F, 20.0F},
    AbilityDef{"Deadlight Snare",
               "Throw a trap net forward and dash backward. On hit, pulls nearby enemies together and "
               "stuns them for 2s.\nMana: 20  Cooldown: 20s",
               "assets/textures/abilities/deadlight_snare.png", 20.0F, 20.0F},
    AbilityDef{"Calamity Slug",
               "Channel 1s, then fire a huge piercing slug (50 damage) that knocks survivors sideways.\n"
               "Mana: 30  Cooldown: 25s",
               "assets/textures/abilities/calamity_slug.png", 30.0F, 25.0F},
};

} // namespace dreadcast
