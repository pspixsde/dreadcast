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
               "Overload your gun for 6s: ranged attacks fire 4 scattered pellets "
               "(half damage each) that knock enemies back.\nMana: 25  Cooldown: 20s",
               "assets/textures/drafts/1.png", 25.0F, 20.0F},
    AbilityDef{"Deadlight Snare",
               "Throw a trap net forward and dash backward. On hit, pulls nearby enemies together and "
               "stuns them for 2s.\nMana: 20  Cooldown: 20s",
               "assets/textures/drafts/2.png", 20.0F, 20.0F},
    AbilityDef{"Calamity Slug",
               "Channel 1s, then fire a huge piercing slug (50 damage) that knocks survivors sideways.\n"
               "Mana: 30  Cooldown: 25s",
               "assets/textures/drafts/3.png", 30.0F, 25.0F},
};

} // namespace dreadcast
