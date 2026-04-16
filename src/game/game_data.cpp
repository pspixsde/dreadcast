#include "game/game_data.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

#include <raylib.h>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "game/item_rarity.hpp"

namespace dreadcast {

namespace {

std::string joinPath(const std::string &base, const std::string &relative) {
    if (base.empty()) {
        return relative;
    }
    const char last = base.back();
    const bool hasSep = (last == '/' || last == '\\');
    return hasSep ? (base + relative) : (base + "/" + relative);
}

/// Prefer cwd-relative path, then next to the executable (matches `ResourceManager` asset resolution).
[[nodiscard]] std::string resolveDataFilePath(const std::string &relativePath) {
    if (FileExists(relativePath.c_str())) {
        return relativePath;
    }
    const std::string nearExe = joinPath(GetApplicationDirectory(), relativePath);
    if (FileExists(nearExe.c_str())) {
        return nearExe;
    }
    return relativePath;
}

[[nodiscard]] bool readJsonFile(const std::string &path, nlohmann::json &out) {
    const std::string resolved = resolveDataFilePath(path);
    std::ifstream in(resolved, std::ios::binary);
    if (!in.is_open()) {
        TraceLog(LOG_WARNING, "Dreadcast: could not open JSON \"%s\"", resolved.c_str());
        return false;
    }
    try {
        in >> out;
    } catch (const std::exception &e) {
        TraceLog(LOG_WARNING, "Dreadcast: JSON parse error in \"%s\": %s", resolved.c_str(), e.what());
        return false;
    }
    return true;
}

[[nodiscard]] ItemRarity rarityFromString(const std::string &s) {
    if (s == "Tarnished") {
        return ItemRarity::Tarnished;
    }
    if (s == "Blighted") {
        return ItemRarity::Blighted;
    }
    if (s == "Cursed") {
        return ItemRarity::Cursed;
    }
    if (s == "Dread") {
        return ItemRarity::Dread;
    }
    if (s == "Abyssal") {
        return ItemRarity::Abyssal;
    }
    if (s == "Clouded") {
        return ItemRarity::Clouded;
    }
    if (s == "Lucid") {
        return ItemRarity::Lucid;
    }
    if (s == "Absolute") {
        return ItemRarity::Absolute;
    }
    if (s == "Special") {
        return ItemRarity::Special;
    }
    return ItemRarity::Tarnished;
}

[[nodiscard]] EquipSlot equipSlotFromString(const std::string &s) {
    if (s == "Amulet") {
        return EquipSlot::Amulet;
    }
    if (s == "Ring") {
        return EquipSlot::Ring;
    }
    return EquipSlot::Armor;
}

[[nodiscard]] ItemData itemFromJsonObject(const nlohmann::json &j) {
    ItemData it{};
    if (j.contains("id") && j["id"].is_string()) {
        it.catalogId = j["id"].get<std::string>();
    }
    if (j.contains("name") && j["name"].is_string()) {
        it.name = j["name"].get<std::string>();
    }
    if (j.contains("iconPath") && j["iconPath"].is_string()) {
        it.iconPath = j["iconPath"].get<std::string>();
    }
    if (j.contains("equipSlot") && j["equipSlot"].is_string()) {
        it.slot = equipSlotFromString(j["equipSlot"].get<std::string>());
    }
    if (j.contains("rarity") && j["rarity"].is_string()) {
        it.rarity = rarityFromString(j["rarity"].get<std::string>());
    }
    if (j.contains("isConsumable") && j["isConsumable"].is_boolean()) {
        it.isConsumable = j["isConsumable"].get<bool>();
    }
    if (j.contains("isStackable") && j["isStackable"].is_boolean()) {
        it.isStackable = j["isStackable"].get<bool>();
    }
    if (j.contains("maxStack") && j["maxStack"].is_number_integer()) {
        it.maxStack = j["maxStack"].get<int>();
    }
    if (j.contains("stackCount") && j["stackCount"].is_number_integer()) {
        it.stackCount = j["stackCount"].get<int>();
    }
    if (j.contains("maxHpBonus") && j["maxHpBonus"].is_number()) {
        it.maxHpBonus = j["maxHpBonus"].get<float>();
    }
    if (j.contains("maxManaBonus") && j["maxManaBonus"].is_number()) {
        it.maxManaBonus = j["maxManaBonus"].get<float>();
    }
    if (j.contains("hpRegenBonus") && j["hpRegenBonus"].is_number()) {
        it.hpRegenBonus = j["hpRegenBonus"].get<float>();
    }
    if (j.contains("damageReflectPercent") && j["damageReflectPercent"].is_number()) {
        it.damageReflectPercent = j["damageReflectPercent"].get<float>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        it.description = j["description"].get<std::string>();
    }
    if (it.isConsumable && it.isStackable && it.maxStack <= 0) {
        it.maxStack = maxStackForConsumableRarity(it.rarity);
    }
    if (it.stackCount < 1) {
        it.stackCount = 1;
    }
    return it;
}

[[nodiscard]] AbilityDef abilityFromJsonObject(const nlohmann::json &j) {
    AbilityDef a{};
    if (j.contains("name") && j["name"].is_string()) {
        a.name = j["name"].get<std::string>();
    }
    // Legacy defaults by ability name, so older/partial JSON keeps prior behavior.
    if (a.name == "Lead Fever") {
        a.effectDuration = 6.0F;
        a.pelletCount = 4;
        a.scatterAngle = 0.35F;
        a.scatterRandom = 0.18F;
        a.knockback = 250.0F;
    } else if (a.name == "Deadlight Snare") {
        a.projectileSpeed = 500.0F;
        a.projectileRange = 350.0F;
        a.pullRadius = 100.0F;
        a.stunDuration = 2.0F;
        a.dashDistance = 150.0F;
        a.dashSpeed = 800.0F;
    } else if (a.name == "Calamity Slug") {
        a.aimDuration = 1.0F;
        a.damage = 50.0F;
        a.projectileSpeed = 1200.0F;
        a.projectileRange = 1200.0F;
        a.projectileSize = 20.0F;
        a.knockbackSide = 400.0F;
    }
    if (j.contains("description") && j["description"].is_string()) {
        a.description = j["description"].get<std::string>();
    }
    if (j.contains("iconPath") && j["iconPath"].is_string()) {
        a.iconPath = j["iconPath"].get<std::string>();
    }
    if (j.contains("manaCost") && j["manaCost"].is_number()) {
        a.manaCost = j["manaCost"].get<float>();
    }
    if (j.contains("cooldown") && j["cooldown"].is_number()) {
        a.cooldown = j["cooldown"].get<float>();
    }
    auto numF = [&](const char *key, float &outV) {
        if (j.contains(key) && j[key].is_number()) {
            outV = j[key].get<float>();
        }
    };
    numF("effectDuration", a.effectDuration);
    if (j.contains("pelletCount") && j["pelletCount"].is_number_integer()) {
        a.pelletCount = std::max(1, j["pelletCount"].get<int>());
    }
    numF("scatterAngle", a.scatterAngle);
    numF("scatterRandom", a.scatterRandom);
    numF("knockback", a.knockback);
    numF("projectileSpeed", a.projectileSpeed);
    numF("projectileRange", a.projectileRange);
    numF("pullRadius", a.pullRadius);
    numF("stunDuration", a.stunDuration);
    numF("dashDistance", a.dashDistance);
    numF("dashSpeed", a.dashSpeed);
    numF("aimDuration", a.aimDuration);
    numF("damage", a.damage);
    numF("projectileSize", a.projectileSize);
    numF("knockbackSide", a.knockbackSide);
    return a;
}

void loadItemsFromJson(std::unordered_map<std::string, ItemData> &catalog) {
    nlohmann::json j;
    if (!readJsonFile("assets/data/items.json", j) || !j.contains("items") || !j["items"].is_array()) {
        return;
    }
    for (const auto &el : j["items"]) {
        if (!el.is_object() || !el.contains("id") || !el["id"].is_string()) {
            continue;
        }
        const std::string id = el["id"].get<std::string>();
        catalog[id] = itemFromJsonObject(el);
    }
}

[[nodiscard]] CharacterClass characterFromJsonObject(const nlohmann::json &j) {
    CharacterClass c{};
    if (j.contains("id") && j["id"].is_string()) {
        c.id = j["id"].get<std::string>();
    }
    if (j.contains("name") && j["name"].is_string()) {
        c.name = j["name"].get<std::string>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        c.description = j["description"].get<std::string>();
    }
    if (j.contains("bio") && j["bio"].is_string()) {
        c.bio = j["bio"].get<std::string>();
    }
    if (j.contains("detailAbilities") && j["detailAbilities"].is_string()) {
        c.detailAbilities = j["detailAbilities"].get<std::string>();
    }
    auto numF = [&](const char *key, float &outV) {
        if (j.contains(key) && j[key].is_number()) {
            outV = j[key].get<float>();
        }
    };
    numF("hpRegen", c.hpRegen);
    numF("manaRegen", c.manaRegen);
    numF("baseMaxHp", c.baseMaxHp);
    numF("baseMaxMana", c.baseMaxMana);
    numF("meleeDamage", c.meleeDamage);
    numF("meleeRange", c.meleeRange);
    numF("rangedDamage", c.rangedDamage);
    numF("rangedRange", c.rangedRange);
    numF("moveSpeed", c.moveSpeed);
    numF("visionRange", c.visionRange);
    numF("levelMaxHpGain", c.levelMaxHpGain);
    numF("levelMaxManaGain", c.levelMaxManaGain);
    numF("levelProjectileDamageGain", c.levelProjectileDamageGain);
    numF("levelMeleeDamageGain", c.levelMeleeDamageGain);
    return c;
}

void loadCharactersFromJson(std::vector<CharacterClass> &out) {
    out.clear();
    nlohmann::json j;
    if (!readJsonFile("assets/data/characters.json", j) || !j.contains("characters") ||
        !j["characters"].is_array()) {
        return;
    }
    for (const auto &el : j["characters"]) {
        if (!el.is_object()) {
            continue;
        }
        CharacterClass c = characterFromJsonObject(el);
        if (!c.name.empty()) {
            out.push_back(std::move(c));
        }
    }
}

void seedCharactersFallback(std::vector<CharacterClass> &out) {
    static constexpr const char *kEmbedded = R"JSON({
  "characters": [
    {
      "id": "undead_hunter",
      "name": "Undead Hunter",
      "description": "Masters of cursed weaponry and relentless aggression.",
      "bio": "Raised among the ash-choked barrows, they trade sanity for certainty: every curse has a price, and they pay in blood so the dead stay buried.",
      "detailAbilities": "- Ranged curse bolt (LMB) — mana cost, hits at range.\n- Three-hit melee combo (RMB) — frontal cone, hold to loop, per-hit knockback.\n- [1] Lead Fever — 6s: shots become 4 randomly scattered pellets (full damage) with knockback; 25 mana, 20s cooldown.\n- [2] Deadlight Snare — throw a net forward, dash back; on hit, pull nearby foes together and stun 2s; 20 mana, 20s cooldown.\n- [3] Calamity Slug — channel 1s, then a huge piercing shot for 50 damage with sideways knockback; 30 mana, 25s cooldown.\n- Built for aggression: close the gap, manage mana, punish mistakes.",
      "hpRegen": 0.5,
      "manaRegen": 0.5,
      "baseMaxHp": 100,
      "baseMaxMana": 100,
      "meleeDamage": 20,
      "meleeRange": 60,
      "rangedDamage": 10,
      "rangedRange": 800,
      "moveSpeed": 280,
      "visionRange": 500,
      "levelMaxHpGain": 10,
      "levelMaxManaGain": 10,
      "levelProjectileDamageGain": 5,
      "levelMeleeDamageGain": 5
    }
  ]
})JSON";
    try {
        const nlohmann::json root = nlohmann::json::parse(kEmbedded);
        for (const auto &el : root["characters"]) {
            if (el.is_object()) {
                out.push_back(characterFromJsonObject(el));
            }
        }
    } catch (const std::exception &e) {
        TraceLog(LOG_ERROR, "Dreadcast: embedded characters fallback parse failed: %s", e.what());
    }
}

void loadAbilitiesFromJson(CharacterAbilities &out) {
    nlohmann::json j;
    if (!readJsonFile("assets/data/abilities.json", j) || !j.contains("abilities") ||
        !j["abilities"].is_array()) {
        return;
    }
    const auto &arr = j["abilities"];
    const size_t n = std::min(static_cast<size_t>(3), arr.size());
    for (size_t i = 0; i < n; ++i) {
        if (arr[static_cast<nlohmann::json::size_type>(i)].is_object()) {
            out.abilities[i] =
                abilityFromJsonObject(arr[static_cast<nlohmann::json::size_type>(i)]);
        }
    }
}

/// If JSON is missing or corrupt, keep the game bootable (same content as shipped `assets/data/*.json`).
void seedItemCatalogFallback(std::unordered_map<std::string, ItemData> &catalog) {
    static constexpr const char *kEmbedded = R"JSON({
  "items": [
    {"id":"iron_armor","name":"Iron Armor","iconPath":"assets/textures/items/iron_armor_icon.png","equipSlot":"Armor","rarity":"Tarnished","isConsumable":false,"isStackable":false,"maxStack":1,"stackCount":1,"maxHpBonus":10,"description":"+10 Max HP"},
    {"id":"barbed_tunic","name":"Barbed Tunic","iconPath":"assets/textures/items/barbed_tunic_icon.png","equipSlot":"Armor","rarity":"Blighted","isConsumable":false,"isStackable":false,"maxStack":1,"stackCount":1,"hpRegenBonus":0.3,"damageReflectPercent":0.1,"description":"+0.3 HP/s\nReflects 10% of incoming damage back to the source."},
    {"id":"vial_pure_blood","name":"Vial of Pure Blood","iconPath":"assets/textures/items/vial_pure_blood_icon.png","equipSlot":"Armor","rarity":"Clouded","isConsumable":true,"isStackable":true,"maxStack":5,"stackCount":1,"description":"Regenerates 40 HP over 8 seconds."},
    {"id":"runic_shell","name":"Runic Shell","iconPath":"assets/textures/items/runic_shell_icon.png","equipSlot":"Armor","rarity":"Cursed","isConsumable":false,"isStackable":false,"maxStack":1,"stackCount":1,"maxHpBonus":25,"description":"+25 Max HP\nBelow 30% HP: releases an energy shockwave dealing 30 damage,\nknocking back enemies, and healing 30 HP. 30s cooldown."},
    {"id":"vial_cordial_manic","name":"Vial of Cordial Manic","iconPath":"assets/textures/items/vial_cordial_manic_icon.png","equipSlot":"Armor","rarity":"Lucid","isConsumable":true,"isStackable":true,"maxStack":3,"stackCount":1,"description":"Doubles movement speed and grants invincibility for 7 seconds.\nNo HP regen and slowly lose 40% of max HP during the effect.\nCannot be used below 40% max HP."},
    {"id":"vial_raw_spirit","name":"Vial of Raw Spirit","iconPath":"assets/textures/items/vial_raw_spirit_icon.png","equipSlot":"Armor","rarity":"Clouded","isConsumable":true,"isStackable":true,"maxStack":5,"stackCount":1,"description":"Regenerates 50 mana over 6 seconds."},
    {"id":"hollow_ring","name":"Hollow Ring","iconPath":"assets/textures/items/hollow_ring_icon.png","equipSlot":"Ring","rarity":"Tarnished","isConsumable":false,"isStackable":false,"maxStack":1,"stackCount":1,"maxManaBonus":15,"description":"+15 Max Mana"}
  ]
})JSON";
    try {
        const nlohmann::json j = nlohmann::json::parse(kEmbedded);
        for (const auto &el : j["items"]) {
            if (!el.is_object() || !el.contains("id") || !el["id"].is_string()) {
                continue;
            }
            const std::string id = el["id"].get<std::string>();
            catalog[id] = itemFromJsonObject(el);
        }
    } catch (const std::exception &e) {
        TraceLog(LOG_ERROR, "Dreadcast: embedded items fallback parse failed: %s", e.what());
    }
}

void seedAbilitiesFallback(CharacterAbilities &out) {
    static constexpr const char *kEmbedded = R"JSON({
  "abilities": [
    {"name":"Lead Fever","description":"Overload your gun for 6s: ranged attacks fire 4 pellets in a loose, random scatter that knock enemies back (full damage per pellet).\nMana: 25  Cooldown: 20s","iconPath":"assets/textures/abilities/lead_fever.png","manaCost":25,"cooldown":20,"effectDuration":6,"pelletCount":4,"scatterAngle":0.35,"scatterRandom":0.18,"knockback":250},
    {"name":"Deadlight Snare","description":"Throw a trap net forward and dash backward. On hit, pulls nearby enemies together and stuns them for 2s.\nMana: 20  Cooldown: 20s","iconPath":"assets/textures/abilities/deadlight_snare.png","manaCost":20,"cooldown":20,"projectileSpeed":500,"projectileRange":350,"pullRadius":100,"stunDuration":2,"dashDistance":150,"dashSpeed":800},
    {"name":"Calamity Slug","description":"Channel 1s, then fire a huge piercing slug (50 damage) that knocks survivors sideways.\nMana: 30  Cooldown: 25s","iconPath":"assets/textures/abilities/calamity_slug.png","manaCost":30,"cooldown":25,"aimDuration":1,"damage":50,"projectileSpeed":1200,"projectileRange":1200,"projectileSize":20,"knockbackSide":400}
  ]
})JSON";
    try {
        const nlohmann::json j = nlohmann::json::parse(kEmbedded);
        const auto &arr = j["abilities"];
        const size_t n = std::min(static_cast<size_t>(3), arr.size());
        for (size_t i = 0; i < n; ++i) {
            if (arr[static_cast<nlohmann::json::size_type>(i)].is_object()) {
                out.abilities[i] =
                    abilityFromJsonObject(arr[static_cast<nlohmann::json::size_type>(i)]);
            }
        }
    } catch (const std::exception &e) {
        TraceLog(LOG_ERROR, "Dreadcast: embedded abilities fallback parse failed: %s", e.what());
    }
}

std::unordered_map<std::string, ItemData> g_itemCatalog{};
CharacterAbilities g_undeadHunterAbilities{};
std::vector<CharacterClass> g_characters{};
bool g_gameDataLoaded{false};

std::vector<ForgeRecipe> g_forgeRecipes{};
std::vector<DisassembleRecipe> g_disassembleRecipes{};

void seedCraftingRecipes() {
    g_forgeRecipes.clear();
    ForgeRecipe fr{};
    fr.inputs.push_back(CraftIngredient{"vial_pure_blood", 1});
    fr.inputs.push_back(CraftIngredient{"iron_armor", 1});
    fr.outputId = "barbed_tunic";
    fr.outputCount = 1;
    g_forgeRecipes.push_back(std::move(fr));

    g_disassembleRecipes.clear();
    DisassembleRecipe dr{};
    dr.sourceId = "barbed_tunic";
    dr.outputs.push_back(CraftIngredient{"vial_pure_blood", 1});
    dr.outputs.push_back(CraftIngredient{"iron_armor", 1});
    g_disassembleRecipes.push_back(std::move(dr));
}

} // namespace

bool loadGameData() {
    std::unordered_map<std::string, ItemData> newItems;
    CharacterAbilities newAb{};
    std::vector<CharacterClass> newChars{};

    loadItemsFromJson(newItems);
    if (newItems.empty()) {
        TraceLog(LOG_WARNING,
                 "Dreadcast: using embedded item definitions (assets/data/items.json not loaded).");
        seedItemCatalogFallback(newItems);
    }

    loadAbilitiesFromJson(newAb);
    bool abilitiesOk = true;
    for (const auto &a : newAb.abilities) {
        if (a.name.empty()) {
            abilitiesOk = false;
            break;
        }
    }
    if (!abilitiesOk) {
        TraceLog(LOG_WARNING,
                 "Dreadcast: using embedded ability definitions (assets/data/abilities.json not "
                 "loaded).");
        seedAbilitiesFallback(newAb);
        abilitiesOk = true;
        for (const auto &a : newAb.abilities) {
            if (a.name.empty()) {
                abilitiesOk = false;
                break;
            }
        }
    }

    const bool itemsOk = !newItems.empty();
    if (itemsOk) {
        g_itemCatalog = std::move(newItems);
    } else {
        TraceLog(LOG_ERROR, "Dreadcast: item catalog empty after JSON and fallback.");
    }

    if (abilitiesOk) {
        g_undeadHunterAbilities = newAb;
    } else {
        TraceLog(LOG_ERROR, "Dreadcast: ability definitions empty after JSON and fallback.");
    }

    loadCharactersFromJson(newChars);
    if (newChars.empty()) {
        TraceLog(LOG_WARNING,
                 "Dreadcast: using embedded character definitions (assets/data/characters.json not "
                 "loaded).");
        seedCharactersFallback(newChars);
    }
    if (!newChars.empty()) {
        g_characters = std::move(newChars);
    } else {
        TraceLog(LOG_ERROR, "Dreadcast: character list empty after JSON and fallback.");
    }

    const bool charsOk = !g_characters.empty();
    g_gameDataLoaded = itemsOk && abilitiesOk && charsOk;
    seedCraftingRecipes();
    return g_gameDataLoaded;
}

bool gameDataLoaded() { return g_gameDataLoaded; }

ItemData makeItemFromMapKind(const std::string &kind) {
    const auto it = g_itemCatalog.find(kind);
    if (it == g_itemCatalog.end()) {
        return {};
    }
    return it->second;
}

const CharacterAbilities &undeadHunterAbilities() { return g_undeadHunterAbilities; }

int characterCount() { return static_cast<int>(g_characters.size()); }

const CharacterClass &characterAt(int index) {
    static CharacterClass kFallback{};
    static bool fallbackInit = false;
    if (!fallbackInit) {
        fallbackInit = true;
        kFallback.id = "undead_hunter";
        kFallback.name = "Undead Hunter";
        kFallback.description = "Masters of cursed weaponry and relentless aggression.";
        kFallback.bio =
            "Raised among the ash-choked barrows, they trade sanity for certainty: every curse has a "
            "price, and they pay in blood so the dead stay buried.";
        kFallback.detailAbilities =
            "- Ranged curse bolt (LMB) — mana cost, hits at range.\n"
            "- Three-hit melee combo (RMB) — frontal cone, hold to loop, per-hit knockback.\n"
            "- [1] Lead Fever — 6s: shots become 4 randomly scattered pellets (full damage) with "
            "knockback; 25 mana, 20s cooldown.\n"
            "- [2] Deadlight Snare — throw a net forward, dash back; on hit, pull nearby foes together and "
            "stun 2s; 20 mana, 20s cooldown.\n"
            "- [3] Calamity Slug — channel 1s, then a huge piercing shot for 50 damage with sideways "
            "knockback; 30 mana, 25s cooldown.\n"
            "- Built for aggression: close the gap, manage mana, punish mistakes.";
        kFallback.hpRegen = 0.5F;
        kFallback.manaRegen = 0.5F;
        kFallback.baseMaxHp = 100.0F;
        kFallback.baseMaxMana = 100.0F;
        kFallback.meleeDamage = 20.0F;
        kFallback.meleeRange = 60.0F;
        kFallback.rangedDamage = 10.0F;
        kFallback.rangedRange = 800.0F;
        kFallback.moveSpeed = 280.0F;
        kFallback.visionRange = 500.0F;
        kFallback.levelMaxHpGain = 10.0F;
        kFallback.levelMaxManaGain = 10.0F;
        kFallback.levelProjectileDamageGain = 5.0F;
        kFallback.levelMeleeDamageGain = 5.0F;
    }
    if (g_characters.empty()) {
        return kFallback;
    }
    const size_t i = static_cast<size_t>(std::max(0, index));
    return g_characters[std::min(i, g_characters.size() - 1)];
}

const std::vector<ForgeRecipe> &forgeRecipes() { return g_forgeRecipes; }

const std::vector<DisassembleRecipe> &disassembleRecipes() { return g_disassembleRecipes; }

} // namespace dreadcast
