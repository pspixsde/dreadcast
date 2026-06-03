#include "game/game_data.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <random>
#include <unordered_set>
#include <utility>
#include <vector>

#include <raylib.h>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "game/enemy_archetype.hpp"
#include "game/item_effects.hpp"
#include "game/item_rarity.hpp"
#include "game/item_transaction.hpp"
#include "game/map_data.hpp"

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

[[nodiscard]] ItemEffectTrigger itemEffectTriggerFromString(const std::string &s) {
    if (s == "onEquipped" || s == "OnEquipped") {
        return ItemEffectTrigger::OnEquipped;
    }
    if (s == "onUnequipped" || s == "OnUnequipped") {
        return ItemEffectTrigger::OnUnequipped;
    }
    if (s == "onUse" || s == "OnUse") {
        return ItemEffectTrigger::OnUse;
    }
    if (s == "onTick" || s == "OnTick") {
        return ItemEffectTrigger::OnTick;
    }
    if (s == "onTakeDamage" || s == "OnTakeDamage") {
        return ItemEffectTrigger::OnTakeDamage;
    }
    if (s == "onAbilityCooldownEnd" || s == "OnAbilityCooldownEnd") {
        return ItemEffectTrigger::OnAbilityCooldownEnd;
    }
    if (s == "onHpThreshold" || s == "OnHpThreshold") {
        return ItemEffectTrigger::OnHpThreshold;
    }
    if (s == "onStandingStill" || s == "OnStandingStill") {
        return ItemEffectTrigger::OnStandingStill;
    }
    return ItemEffectTrigger::None;
}

[[nodiscard]] ItemEffectActionKind itemEffectActionKindFromString(const std::string &s) {
    if (s == "statModifier" || s == "StatModifier") {
        return ItemEffectActionKind::StatModifier;
    }
    if (s == "healOverTime" || s == "HealOverTime") {
        return ItemEffectActionKind::HealOverTime;
    }
    if (s == "manaOverTime" || s == "ManaOverTime") {
        return ItemEffectActionKind::ManaOverTime;
    }
    if (s == "speedWindow" || s == "SpeedWindow") {
        return ItemEffectActionKind::SpeedWindow;
    }
    if (s == "aoeShockwave" || s == "AoeShockwave") {
        return ItemEffectActionKind::AoeShockwave;
    }
    if (s == "manaRefund" || s == "ManaRefund") {
        return ItemEffectActionKind::ManaRefund;
    }
    if (s == "visionStandingBonus" || s == "VisionStandingBonus") {
        return ItemEffectActionKind::VisionStandingBonus;
    }
    return ItemEffectActionKind::None;
}

[[nodiscard]] StatModifierKind statModifierKindFromString(const std::string &s) {
    if (s == "mul" || s == "Mul" || s == "multiply") {
        return StatModifierKind::Mul;
    }
    return StatModifierKind::Add;
}

void parseItemEffectAction(const nlohmann::json &aj, ItemEffectAction &out) {
    if (!aj.contains("kind") || !aj["kind"].is_string()) {
        return;
    }
    out.kind = itemEffectActionKindFromString(aj["kind"].get<std::string>());
    if (aj.contains("stat") && aj["stat"].is_string()) {
        out.statName = aj["stat"].get<std::string>();
    }
    if (aj.contains("value") && aj["value"].is_number()) {
        out.statValue = aj["value"].get<float>();
    }
    if (aj.contains("modifier") && aj["modifier"].is_string()) {
        out.statKind = statModifierKindFromString(aj["modifier"].get<std::string>());
    }
    if (aj.contains("total") && aj["total"].is_number()) {
        out.overTimeTotal = aj["total"].get<float>();
    }
    if (aj.contains("duration") && aj["duration"].is_number()) {
        out.overTimeDuration = aj["duration"].get<float>();
    }
    if (aj.contains("multiplier") && aj["multiplier"].is_number()) {
        out.speedMultiplier = aj["multiplier"].get<float>();
    }
    if (aj.contains("minHpFraction") && aj["minHpFraction"].is_number()) {
        out.speedWindowMinHpFraction = aj["minHpFraction"].get<float>();
    }
    if (aj.contains("drainHpFraction") && aj["drainHpFraction"].is_number()) {
        out.speedWindowDrainHpFraction = aj["drainHpFraction"].get<float>();
    }
    if (aj.contains("invulnerable") && aj["invulnerable"].is_boolean()) {
        out.speedWindowInvulnerable = aj["invulnerable"].get<bool>();
    }
    if (aj.contains("radius") && aj["radius"].is_number()) {
        out.shockwaveRadius = aj["radius"].get<float>();
    }
    if (aj.contains("damage") && aj["damage"].is_number()) {
        out.shockwaveDamage = aj["damage"].get<float>();
    }
    if (aj.contains("knockback") && aj["knockback"].is_number()) {
        out.shockwaveKnockback = aj["knockback"].get<float>();
    }
    if (aj.contains("selfHeal") && aj["selfHeal"].is_number()) {
        out.shockwaveSelfHeal = aj["selfHeal"].get<float>();
    }
    if (aj.contains("cooldownSeconds") && aj["cooldownSeconds"].is_number()) {
        out.shockwaveCooldown = aj["cooldownSeconds"].get<float>();
    }
    if (aj.contains("refundMana") && aj["refundMana"].is_number()) {
        out.manaRefundAmount = aj["refundMana"].get<float>();
    }
    if (aj.contains("stillSeconds") && aj["stillSeconds"].is_number()) {
        out.standingStillSeconds = aj["stillSeconds"].get<float>();
    }
    if (aj.contains("visionBonus") && aj["visionBonus"].is_number()) {
        out.standingVisionBonus = aj["visionBonus"].get<float>();
    }
}

void parseItemEffectsArray(const nlohmann::json &arr, std::vector<ItemEffectDef> &out) {
    out.clear();
    if (!arr.is_array()) {
        return;
    }
    static const std::unordered_set<std::string> allowedEffectKeys{"trigger", "condition", "action"};
    for (const auto &el : arr) {
        if (!el.is_object()) {
            continue;
        }
        for (auto jt = el.begin(); jt != el.end(); ++jt) {
            const std::string k = jt.key();
            if (allowedEffectKeys.count(k) == 0U) {
                TraceLog(LOG_WARNING, "Dreadcast: unknown key in item effect object: \"%s\".",
                         k.c_str());
            }
        }
        ItemEffectDef def{};
        if (el.contains("trigger") && el["trigger"].is_string()) {
            const std::string ts = el["trigger"].get<std::string>();
            def.trigger = itemEffectTriggerFromString(ts);
            if (def.trigger == ItemEffectTrigger::None && !ts.empty()) {
                TraceLog(LOG_WARNING, "Dreadcast: unknown item effect trigger \"%s\".", ts.c_str());
            }
        }
        if (el.contains("condition") && el["condition"].is_object()) {
            const auto &cj = el["condition"];
            if (cj.contains("hpFractionAtMost") && cj["hpFractionAtMost"].is_number()) {
                def.condition.hpFractionAtMost = cj["hpFractionAtMost"].get<float>();
            }
            if (cj.contains("edgeCrossing") && cj["edgeCrossing"].is_string()) {
                const std::string ed = cj["edgeCrossing"].get<std::string>();
                def.condition.hpEdge =
                    (ed == "downward" || ed == "Downward") ? HpThresholdEdge::Downward
                                                           : HpThresholdEdge::Any;
            }
            if (cj.contains("cooldownSeconds") && cj["cooldownSeconds"].is_number()) {
                def.condition.cooldownSeconds = cj["cooldownSeconds"].get<float>();
            }
            if (cj.contains("stillForSeconds") && cj["stillForSeconds"].is_number()) {
                def.condition.stillForSeconds = cj["stillForSeconds"].get<float>();
            }
        }
        if (el.contains("action") && el["action"].is_object()) {
            const auto &aj = el["action"];
            parseItemEffectAction(aj, def.action);
            if (aj.contains("kind") && aj["kind"].is_string()) {
                const std::string ks = aj["kind"].get<std::string>();
                if (def.action.kind == ItemEffectActionKind::None && !ks.empty()) {
                    TraceLog(LOG_WARNING, "Dreadcast: unknown item effect action kind \"%s\".",
                             ks.c_str());
                }
            }
        }
        if (def.trigger != ItemEffectTrigger::None && def.action.kind != ItemEffectActionKind::None) {
            out.push_back(std::move(def));
        }
    }
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
    if (j.contains("moveSpeedBonus") && j["moveSpeedBonus"].is_number()) {
        it.moveSpeedBonus = j["moveSpeedBonus"].get<float>();
    }
    if (j.contains("visionRangeBonus") && j["visionRangeBonus"].is_number()) {
        it.visionRangeBonus = j["visionRangeBonus"].get<float>();
    }
    if (j.contains("damageReflectPercent") && j["damageReflectPercent"].is_number()) {
        it.damageReflectPercent = j["damageReflectPercent"].get<float>();
    }
    if (j.contains("abilityManaCostMultiplier") && j["abilityManaCostMultiplier"].is_number()) {
        it.abilityManaCostMultiplier = j["abilityManaCostMultiplier"].get<float>();
    }
    if (j.contains("abilityCooldownManaRefund") && j["abilityCooldownManaRefund"].is_number()) {
        it.abilityCooldownManaRefund = j["abilityCooldownManaRefund"].get<float>();
    }
    if (it.abilityManaCostMultiplier <= 0.001F) {
        it.abilityManaCostMultiplier = 1.0F;
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
    if (j.contains("effects") && j["effects"].is_array()) {
        parseItemEffectsArray(j["effects"], it.effects);
    }
    if (it.effects.empty()) {
        synthesizeLegacyItemEffects(it);
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
        a.scatterAngle = 0.30F;
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
    int itemsSchemaVersion = 1;
    if (j.contains("schemaVersion") && j["schemaVersion"].is_number_integer()) {
        itemsSchemaVersion = j["schemaVersion"].get<int>();
    } else if (j.contains("version") && j["version"].is_number_integer()) {
        itemsSchemaVersion = j["version"].get<int>();
    } else {
        TraceLog(LOG_WARNING,
                 "Dreadcast: items.json missing schemaVersion (or legacy \"version\") — assuming 1.");
    }
    if (itemsSchemaVersion != 1) {
        TraceLog(LOG_WARNING,
                 "Dreadcast: items.json schemaVersion %d is not 1 — loader may reject fields later.",
                 itemsSchemaVersion);
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
    numF("rangedProjectileSpeed", c.rangedProjectileSpeed);
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
      "description": "",
      "bio": "A hunter cast out over rumors of dark blood. A cursed beast and cruel men tore his life apart; he died on his family's floor vowing vengeance — and woke in the Dread Pit, half-dead and hungry in ways that no longer feel human.",
      "detailAbilities": "- Ranged curse bolt (LMB) — hits at range; see Attack for damage, range, and bolt speed.\n- Three-hit melee combo (RMB) — frontal cone, hold to loop, per-hit knockback.\n- [1] Lead Fever — 6s: shots become 4 randomly scattered pellets (full damage) with knockback; 25 mana, 20s cooldown.\n- [2] Deadlight Snare — throw a net forward, dash back; on hit, pull nearby foes together and stun 2s; 20 mana, 20s cooldown.\n- [3] Calamity Slug — channel 1s, then a huge piercing shot for 50 damage with sideways knockback; 30 mana, 25s cooldown.\n- Built for aggression: close the gap, manage mana, punish mistakes.",
      "hpRegen": 0.5,
      "manaRegen": 0.5,
      "baseMaxHp": 100,
      "baseMaxMana": 100,
      "meleeDamage": 20,
      "meleeRange": 60,
      "rangedDamage": 10,
      "rangedRange": 800,
      "rangedProjectileSpeed": 600,
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

[[nodiscard]] bool behaviorStringValid(const std::string &behavior) {
    return behavior == "ranged_kiter" || behavior == "melee_chaser" || behavior == "mid_bruiser" ||
           behavior == "dreg_swarmer";
}

[[nodiscard]] EnemyArchetype enemyArchetypeFromJsonObject(const nlohmann::json &j) {
    EnemyArchetype a{};
    if (j.contains("id") && j["id"].is_string()) {
        a.id = j["id"].get<std::string>();
    }
    if (j.contains("displayName") && j["displayName"].is_string()) {
        a.displayName = j["displayName"].get<std::string>();
    } else if (!a.id.empty()) {
        a.displayName = a.id;
    }
    if (j.contains("behavior") && j["behavior"].is_string()) {
        a.behavior = j["behavior"].get<std::string>();
    }
    if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() >= 3) {
        const unsigned char r = static_cast<unsigned char>(j["tint"][0].get<int>());
        const unsigned char g = static_cast<unsigned char>(j["tint"][1].get<int>());
        const unsigned char b = static_cast<unsigned char>(j["tint"][2].get<int>());
        const unsigned char alpha =
            j["tint"].size() >= 4 ? static_cast<unsigned char>(j["tint"][3].get<int>()) : 255;
        a.tint = Color{r, g, b, alpha};
    }
    auto numF = [&](const char *key, float &outV) {
        if (j.contains(key) && j[key].is_number()) {
            outV = j[key].get<float>();
        }
    };
    numF("spriteSize", a.spriteSize);
    numF("hp", a.hp);
    numF("xp", a.xp);
    numF("agitationRange", a.agitationRange);
    numF("calmDownDelay", a.calmDownDelay);

    if (j.contains("ranged") && j["ranged"].is_object()) {
        const auto &r = j["ranged"];
        auto rNum = [&](const char *key, float &outV) {
            if (r.contains(key) && r[key].is_number()) {
                outV = r[key].get<float>();
            }
        };
        rNum("minShootRange", a.ranged.minShootRange);
        rNum("shootCooldown", a.ranged.shootCooldown);
        rNum("preferredRange", a.ranged.preferredRange);
        rNum("kiteSpeed", a.ranged.kiteSpeed);
        rNum("advanceSpeed", a.ranged.advanceSpeed);
        rNum("panicRange", a.ranged.panicRange);
        rNum("strafeBias", a.ranged.strafeBias);
        rNum("projectileDamage", a.ranged.projectileDamage);
        rNum("projectileSpeed", a.ranged.projectileSpeed);
        rNum("projectileRange", a.ranged.projectileRange);
    }
    if (j.contains("melee") && j["melee"].is_object()) {
        const auto &m = j["melee"];
        if (m.contains("chaseSpeed") && m["chaseSpeed"].is_number()) {
            a.melee.chaseSpeed = m["chaseSpeed"].get<float>();
        }
        if (m.contains("meleeDamage") && m["meleeDamage"].is_number()) {
            a.melee.meleeDamage = m["meleeDamage"].get<float>();
        }
        if (m.contains("meleeRange") && m["meleeRange"].is_number()) {
            a.melee.meleeRange = m["meleeRange"].get<float>();
        }
        if (m.contains("meleeCooldown") && m["meleeCooldown"].is_number()) {
            a.melee.meleeCooldown = m["meleeCooldown"].get<float>();
        }
    }
    if (j.contains("bruiser") && j["bruiser"].is_object()) {
        const auto &b = j["bruiser"];
        auto bNum = [&](const char *key, float &outV) {
            if (b.contains(key) && b[key].is_number()) {
                outV = b[key].get<float>();
            }
        };
        bNum("chaseSpeed", a.bruiser.chaseSpeed);
        bNum("preferredRange", a.bruiser.preferredRange);
        bNum("attackDamage", a.bruiser.attackDamage);
        bNum("attackCooldown", a.bruiser.attackCooldown);
        bNum("attackTelegraph", a.bruiser.attackTelegraph);
        bNum("attackRange", a.bruiser.attackRange);
        bNum("attackLineLength", a.bruiser.attackLineLength);
        bNum("attackLineHalfWidth", a.bruiser.attackLineHalfWidth);
        bNum("closeRange", a.bruiser.closeRange);
        bNum("abilityChargeTime", a.bruiser.abilityChargeTime);
        bNum("abilityCooldown", a.bruiser.abilityCooldown);
        bNum("abilityKnockback", a.bruiser.abilityKnockback);
        bNum("abilityKnockbackDuration", a.bruiser.abilityKnockbackDuration);
        bNum("slowMultiplier", a.bruiser.slowMultiplier);
        bNum("slowDuration", a.bruiser.slowDuration);
    }
    return a;
}

void loadEnemyGlobalsFromJson(const nlohmann::json &root, EnemyAiGlobals &out) {
    if (!root.contains("globals") || !root["globals"].is_object()) {
        return;
    }
    const auto &g = root["globals"];
    auto numF = [&](const char *key, float &outV) {
        if (g.contains(key) && g[key].is_number()) {
            outV = g[key].get<float>();
        }
    };
    numF("steerRate", out.steerRate);
    numF("seekArriveRadius", out.seekArriveRadius);
    numF("stuckThreshold", out.stuckThreshold);
    numF("stuckMinDisp", out.stuckMinDisp);
}

void loadEnemiesFromJson(std::unordered_map<std::string, EnemyArchetype> &out,
                         EnemyAiGlobals &globalsOut) {
    out.clear();
    nlohmann::json j;
    if (!readJsonFile("assets/data/enemies.json", j)) {
        return;
    }
    if (j.contains("schemaVersion") && j["schemaVersion"].is_number()) {
        const int ver = j["schemaVersion"].get<int>();
        if (ver != 1) {
            TraceLog(LOG_WARNING,
                     "Dreadcast: enemies.json schemaVersion %d is not 1 — loader may reject fields "
                     "later.",
                     ver);
        }
    } else {
        TraceLog(LOG_WARNING,
                 "Dreadcast: enemies.json missing schemaVersion — assuming 1 (legacy file).");
    }
    loadEnemyGlobalsFromJson(j, globalsOut);
    if (!j.contains("enemies") || !j["enemies"].is_array()) {
        return;
    }
    for (const auto &el : j["enemies"]) {
        if (!el.is_object()) {
            continue;
        }
        EnemyArchetype a = enemyArchetypeFromJsonObject(el);
        if (a.id.empty()) {
            TraceLog(LOG_WARNING, "Dreadcast: enemies.json entry missing id — skipped.");
            continue;
        }
        if (!behaviorStringValid(a.behavior)) {
            TraceLog(LOG_WARNING,
                     "Dreadcast: enemy \"%s\" has unknown behavior \"%s\" — defaulting to "
                     "ranged_kiter.",
                     a.id.c_str(), a.behavior.c_str());
            a.behavior = "ranged_kiter";
        }
        if (out.find(a.id) != out.end()) {
            TraceLog(LOG_WARNING, "Dreadcast: duplicate enemy id \"%s\" — later entry wins.",
                     a.id.c_str());
        }
        out[a.id] = std::move(a);
    }
}

void validateEnemyCatalog(const std::unordered_map<std::string, EnemyArchetype> &catalog) {
    for (const auto &[id, a] : catalog) {
        if (a.hp <= 0.0F) {
            TraceLog(LOG_WARNING, "Dreadcast: enemy \"%s\" has hp <= 0.", id.c_str());
        }
        if (a.spriteSize <= 0.0F) {
            TraceLog(LOG_WARNING, "Dreadcast: enemy \"%s\" has spriteSize <= 0.", id.c_str());
        }
    }
}

void rebuildEnemyArchetypeList(const std::unordered_map<std::string, EnemyArchetype> &catalog,
                               std::vector<EnemyArchetype> &listOut) {
    listOut.clear();
    listOut.reserve(catalog.size());
    for (const auto &[_, a] : catalog) {
        listOut.push_back(a);
    }
    std::sort(listOut.begin(), listOut.end(),
              [](const EnemyArchetype &lhs, const EnemyArchetype &rhs) { return lhs.id < rhs.id; });
}

std::unordered_map<std::string, ItemData> g_itemCatalog{};
std::vector<ItemData> g_catalogItemsList{};

/// Number of bridged rarity pools used by casket loot rolls (see `casketRarityPoolIndex`).
inline constexpr int kCasketPoolCount = 5;
/// Catalog ids grouped by bridged rarity pool (0 = Tarnished/Clouded … 4 = Abyssal/Absolute).
/// Rebuilt by `rebuildCatalogItemsList`; excludes `Special`-rarity items.
std::array<std::vector<std::string>, kCasketPoolCount> g_casketRarityPools{};

/// Bridged rarity pool index for casket loot, or -1 for `Special` (never rolled into caskets).
[[nodiscard]] int casketRarityPoolIndex(ItemRarity r) {
    switch (r) {
    case ItemRarity::Tarnished:
    case ItemRarity::Clouded:
        return 0;
    case ItemRarity::Blighted:
        return 1;
    case ItemRarity::Cursed:
    case ItemRarity::Lucid:
        return 2;
    case ItemRarity::Dread:
        return 3;
    case ItemRarity::Abyssal:
    case ItemRarity::Absolute:
        return 4;
    case ItemRarity::Special:
    default:
        return -1;
    }
}

/// Per-session loot RNG; seeded once from `random_device`.
[[nodiscard]] std::mt19937 &casketRng() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

/// Index into `weights` chosen proportionally to each weight (returns 0 if all weights are 0).
[[nodiscard]] int weightedPickIndex(const int *weights, int count) {
    int total = 0;
    for (int i = 0; i < count; ++i) {
        total += std::max(0, weights[i]);
    }
    if (total <= 0) {
        return 0;
    }
    std::uniform_int_distribution<int> dist(1, total);
    int roll = dist(casketRng());
    for (int i = 0; i < count; ++i) {
        roll -= std::max(0, weights[i]);
        if (roll <= 0) {
            return i;
        }
    }
    return count - 1;
}

/// Nearest non-empty bridged pool to `desired` (searches lower rarities first, then higher).
/// Returns -1 only when every pool is empty.
[[nodiscard]] int nearestStockedPool(int desired) {
    if (desired >= 0 && desired < kCasketPoolCount &&
        !g_casketRarityPools[static_cast<size_t>(desired)].empty()) {
        return desired;
    }
    for (int d = desired - 1; d >= 0; --d) {
        if (!g_casketRarityPools[static_cast<size_t>(d)].empty()) {
            return d;
        }
    }
    for (int u = desired + 1; u < kCasketPoolCount; ++u) {
        if (!g_casketRarityPools[static_cast<size_t>(u)].empty()) {
            return u;
        }
    }
    return -1;
}

/// Picks a random catalog id from pool `desired` (or the nearest stocked pool). Returns the chosen
/// id and the *effective* pool it came from, or {"", -1} when the whole catalog is empty.
[[nodiscard]] std::pair<std::string, int> pickCasketItem(int desired) {
    const int pool = nearestStockedPool(desired);
    if (pool < 0) {
        return {std::string{}, -1};
    }
    const auto &ids = g_casketRarityPools[static_cast<size_t>(pool)];
    std::uniform_int_distribution<size_t> dist(0, ids.size() - 1);
    return {ids[dist(casketRng())], pool};
}

/// Tuning for one casket tier: item-count odds (1/2/3 items), per-slot bridged-rarity odds, and the
/// minimum bridged pool that at least one item must reach (0 = no guarantee).
struct CasketTierRoll {
    std::array<int, 3> countWeights{};
    std::array<int, kCasketPoolCount> rarityWeights{};
    int minPoolFloor{0};
};

[[nodiscard]] CasketTierRoll casketTierRoll(CasketTier tier) {
    switch (tier) {
    case CasketTier::Sealed:
        return CasketTierRoll{{35, 45, 20}, {30, 30, 25, 12, 3}, 1};
    case CasketTier::Wrought:
        return CasketTierRoll{{10, 40, 50}, {15, 25, 30, 20, 10}, 2};
    case CasketTier::Old:
    default:
        return CasketTierRoll{{65, 25, 10}, {45, 30, 18, 6, 1}, 0};
    }
}
CharacterAbilities g_undeadHunterAbilities{};
std::vector<CharacterClass> g_characters{};
bool g_gameDataLoaded{false};

std::vector<Recipe> g_allRecipes{};
std::vector<Recipe> g_forgeRecipesCache{};
std::vector<Recipe> g_disassembleRecipesCache{};
std::unordered_set<std::string> g_forgeBenchInputIds{};
std::unordered_set<std::string> g_disassembleBenchInputIds{};

std::unordered_map<std::string, EnemyArchetype> g_enemyCatalog{};
std::vector<EnemyArchetype> g_enemyArchetypeList{};
EnemyAiGlobals g_enemyAiGlobals{};

void rebuildRecipeCaches() {
    g_forgeRecipesCache.clear();
    g_disassembleRecipesCache.clear();
    g_forgeBenchInputIds.clear();
    g_disassembleBenchInputIds.clear();
    for (const Recipe &r : g_allRecipes) {
        if (r.kind == RecipeKind::Forge) {
            g_forgeRecipesCache.push_back(r);
            for (const CraftIngredient &in : r.inputs) {
                if (!in.itemId.empty()) {
                    g_forgeBenchInputIds.insert(in.itemId);
                }
            }
        } else if (r.kind == RecipeKind::Disassemble) {
            g_disassembleRecipesCache.push_back(r);
            for (const CraftIngredient &in : r.inputs) {
                if (!in.itemId.empty()) {
                    g_disassembleBenchInputIds.insert(in.itemId);
                }
            }
        }
    }
}

void rebuildCatalogItemsList() {
    g_catalogItemsList.clear();
    g_catalogItemsList.reserve(g_itemCatalog.size());
    for (auto &pool : g_casketRarityPools) {
        pool.clear();
    }
    for (const auto &kv : g_itemCatalog) {
        g_catalogItemsList.push_back(kv.second);
        const int pool = casketRarityPoolIndex(kv.second.rarity);
        if (pool >= 0 && pool < kCasketPoolCount && !kv.first.empty()) {
            g_casketRarityPools[static_cast<size_t>(pool)].push_back(kv.first);
        }
    }
    // Stable, deterministic ordering within each pool (catalog map order is unspecified).
    for (auto &pool : g_casketRarityPools) {
        std::sort(pool.begin(), pool.end());
    }
}

[[nodiscard]] RecipeKind recipeKindFromString(const std::string &s) {
    if (s == "forge" || s == "Forge") {
        return RecipeKind::Forge;
    }
    if (s == "disassemble" || s == "Disassemble") {
        return RecipeKind::Disassemble;
    }
    return RecipeKind::Forge;
}

[[nodiscard]] CraftIngredient craftIngredientFromJson(const nlohmann::json &ing) {
    CraftIngredient ci{};
    if (ing.contains("itemId") && ing["itemId"].is_string()) {
        ci.itemId = ing["itemId"].get<std::string>();
    }
    if (ing.contains("count") && ing["count"].is_number_integer()) {
        ci.count = ing["count"].get<int>();
    } else if (ing.contains("count") && ing["count"].is_number_unsigned()) {
        ci.count = static_cast<int>(ing["count"].get<unsigned>());
    }
    if (ci.count < 1) {
        ci.count = 1;
    }
    return ci;
}

void seedCraftingRecipes() {
    g_allRecipes.clear();
    {
        Recipe forge{};
        forge.id = "forge_barbed_tunic";
        forge.kind = RecipeKind::Forge;
        forge.inputs.push_back(CraftIngredient{"vial_pure_blood", 1});
        forge.inputs.push_back(CraftIngredient{"iron_armor", 1});
        forge.outputs.push_back(CraftIngredient{"barbed_tunic", 1});
        g_allRecipes.push_back(std::move(forge));
    }
    {
        Recipe dis{};
        dis.id = "disassemble_barbed_tunic";
        dis.kind = RecipeKind::Disassemble;
        dis.inputs.push_back(CraftIngredient{"barbed_tunic", 1});
        dis.outputs.push_back(CraftIngredient{"vial_pure_blood", 1});
        dis.outputs.push_back(CraftIngredient{"iron_armor", 1});
        g_allRecipes.push_back(std::move(dis));
    }
    rebuildRecipeCaches();
}

void parseRecipeObjectToList(const nlohmann::json &el, std::vector<Recipe> &out) {
    if (!el.is_object()) {
        return;
    }
    Recipe r{};
    if (el.contains("id") && el["id"].is_string()) {
        r.id = el["id"].get<std::string>();
    }
    if (el.contains("kind") && el["kind"].is_string()) {
        r.kind = recipeKindFromString(el["kind"].get<std::string>());
    }
    if (el.contains("inputs") && el["inputs"].is_array()) {
        for (const auto &ing : el["inputs"]) {
            if (!ing.is_object()) {
                continue;
            }
            const CraftIngredient ci = craftIngredientFromJson(ing);
            if (!ci.itemId.empty()) {
                r.inputs.push_back(ci);
            }
        }
    }
    if (el.contains("outputs") && el["outputs"].is_array()) {
        for (const auto &outg : el["outputs"]) {
            if (!outg.is_object()) {
                continue;
            }
            const CraftIngredient ci = craftIngredientFromJson(outg);
            if (!ci.itemId.empty()) {
                r.outputs.push_back(ci);
            }
        }
    }
    if (el.contains("conditions") && el["conditions"].is_array()) {
        for (const auto &c : el["conditions"]) {
            if (!c.is_object()) {
                continue;
            }
            CraftCondition cc{};
            if (c.contains("kind") && c["kind"].is_string()) {
                cc.kind = c["kind"].get<std::string>();
            }
            if (c.contains("param") && c["param"].is_string()) {
                cc.param = c["param"].get<std::string>();
            }
            if (c.contains("value") && c["value"].is_number()) {
                cc.value = c["value"].get<float>();
            }
            r.conditions.push_back(std::move(cc));
        }
    }
    if (el.contains("sideEffects") && el["sideEffects"].is_array()) {
        for (const auto &s : el["sideEffects"]) {
            if (!s.is_object()) {
                continue;
            }
            CraftSideEffect se{};
            if (s.contains("kind") && s["kind"].is_string()) {
                se.kind = s["kind"].get<std::string>();
            }
            if (s.contains("param") && s["param"].is_string()) {
                se.param = s["param"].get<std::string>();
            }
            if (s.contains("value") && s["value"].is_number()) {
                se.value = s["value"].get<float>();
            }
            r.sideEffects.push_back(std::move(se));
        }
    }
    if (!r.outputs.empty() && !r.inputs.empty()) {
        if (r.id.empty()) {
            r.id = "recipe_" + std::to_string(out.size());
        }
        out.push_back(std::move(r));
    }
}

void loadRecipesFromJson() {
    g_allRecipes.clear();
    nlohmann::json j;
    if (!readJsonFile("assets/data/recipes.json", j)) {
        TraceLog(LOG_WARNING,
                 "Dreadcast: assets/data/recipes.json not found; using built-in crafting recipes.");
        seedCraftingRecipes();
        return;
    }
    try {
        int recipesSchemaVersion = 1;
        if (j.contains("schemaVersion") && j["schemaVersion"].is_number_integer()) {
            recipesSchemaVersion = j["schemaVersion"].get<int>();
        } else {
            TraceLog(LOG_WARNING,
                     "Dreadcast: recipes.json missing schemaVersion — assuming 1 (legacy file).");
        }
        if (recipesSchemaVersion != 1) {
            TraceLog(LOG_WARNING,
                     "Dreadcast: recipes.json schemaVersion %d is not 1 — loader may reject fields "
                     "later.",
                     recipesSchemaVersion);
        }
        if (j.contains("recipes") && j["recipes"].is_array()) {
            for (const auto &el : j["recipes"]) {
                parseRecipeObjectToList(el, g_allRecipes);
            }
        }
        if (g_allRecipes.empty() && j.contains("forgeRecipes") && j["forgeRecipes"].is_array()) {
            for (const auto &el : j["forgeRecipes"]) {
                if (!el.is_object()) {
                    continue;
                }
                Recipe r{};
                r.kind = RecipeKind::Forge;
                if (el.contains("outputId") && el["outputId"].is_string()) {
                    const std::string oid = el["outputId"].get<std::string>();
                    int oc = 1;
                    if (el.contains("outputCount") && el["outputCount"].is_number_integer()) {
                        oc = el["outputCount"].get<int>();
                    } else if (el.contains("outputCount") && el["outputCount"].is_number_unsigned()) {
                        oc = static_cast<int>(el["outputCount"].get<unsigned>());
                    }
                    if (oc < 1) {
                        oc = 1;
                    }
                    r.outputs.push_back(CraftIngredient{oid, oc});
                }
                if (el.contains("inputs") && el["inputs"].is_array()) {
                    for (const auto &ing : el["inputs"]) {
                        if (!ing.is_object()) {
                            continue;
                        }
                        const CraftIngredient ci = craftIngredientFromJson(ing);
                        if (!ci.itemId.empty()) {
                            r.inputs.push_back(ci);
                        }
                    }
                }
                r.id = "legacy_forge_" + std::to_string(g_allRecipes.size());
                if (!r.outputs.empty() && !r.inputs.empty()) {
                    g_allRecipes.push_back(std::move(r));
                }
            }
        }
        if (j.contains("disassembleRecipes") && j["disassembleRecipes"].is_array()) {
            for (const auto &el : j["disassembleRecipes"]) {
                if (!el.is_object()) {
                    continue;
                }
                Recipe r{};
                r.kind = RecipeKind::Disassemble;
                if (el.contains("sourceId") && el["sourceId"].is_string()) {
                    r.inputs.push_back(CraftIngredient{el["sourceId"].get<std::string>(), 1});
                }
                if (el.contains("outputs") && el["outputs"].is_array()) {
                    for (const auto &out : el["outputs"]) {
                        if (!out.is_object()) {
                            continue;
                        }
                        const CraftIngredient ci = craftIngredientFromJson(out);
                        if (!ci.itemId.empty()) {
                            r.outputs.push_back(ci);
                        }
                    }
                }
                r.id = "legacy_disassemble_" + std::to_string(g_allRecipes.size());
                if (!r.outputs.empty() && !r.inputs.empty()) {
                    g_allRecipes.push_back(std::move(r));
                }
            }
        }
    } catch (const std::exception &e) {
        TraceLog(LOG_WARNING, "Dreadcast: recipes.json parse error: %s — using built-in recipes.",
                 e.what());
        g_allRecipes.clear();
    }
    if (g_allRecipes.empty()) {
        seedCraftingRecipes();
    } else {
        rebuildRecipeCaches();
    }
}

void validateLoadedItemCatalog() {
    for (const auto &kv : g_itemCatalog) {
        const ItemData &it = kv.second;
        for (const ItemEffectDef &eff : it.effects) {
            if (eff.trigger == ItemEffectTrigger::None) {
                TraceLog(LOG_WARNING, "Dreadcast: item \"%s\" has an effect with unset trigger.",
                         it.catalogId.c_str());
            }
            if (eff.action.kind == ItemEffectActionKind::StatModifier &&
                eff.action.statName.empty()) {
                TraceLog(LOG_WARNING, "Dreadcast: item \"%s\" has statModifier with empty stat.",
                         it.catalogId.c_str());
            }
        }
    }
}

void validateCraftingRecipesAgainstCatalog() {
    std::unordered_set<std::string> seenIds;
    for (const Recipe &rec : g_allRecipes) {
        if (!rec.id.empty()) {
            if (seenIds.count(rec.id) != 0U) {
                TraceLog(LOG_WARNING, "Dreadcast: duplicate recipe id \"%s\".", rec.id.c_str());
            }
            seenIds.insert(rec.id);
        }
        if (rec.inputs.empty() || rec.outputs.empty()) {
            TraceLog(LOG_WARNING, "Dreadcast: recipe \"%s\" has empty inputs or outputs.",
                     rec.id.c_str());
        }
        for (const CraftIngredient &in : rec.inputs) {
            if (g_itemCatalog.find(in.itemId) == g_itemCatalog.end()) {
                TraceLog(LOG_WARNING, "Dreadcast: recipe \"%s\" references unknown input id \"%s\".",
                         rec.id.c_str(), in.itemId.c_str());
            }
        }
        for (const CraftIngredient &out : rec.outputs) {
            if (g_itemCatalog.find(out.itemId) == g_itemCatalog.end()) {
                TraceLog(LOG_WARNING, "Dreadcast: recipe \"%s\" references unknown output id \"%s\".",
                         rec.id.c_str(), out.itemId.c_str());
            }
        }
        for (const CraftCondition &c : rec.conditions) {
            if (!c.kind.empty() && c.kind != "always") {
                TraceLog(LOG_WARNING,
                         "Dreadcast: recipe \"%s\" has unknown condition kind \"%s\" (extensible).",
                         rec.id.c_str(), c.kind.c_str());
            }
        }
        for (const CraftSideEffect &s : rec.sideEffects) {
            if (!s.kind.empty()) {
                TraceLog(LOG_WARNING,
                         "Dreadcast: recipe \"%s\" has side-effect kind \"%s\" (no runtime handler yet).",
                         rec.id.c_str(), s.kind.c_str());
            }
        }
        if (rec.kind == RecipeKind::Forge && rec.outputs.size() > 1U) {
            TraceLog(LOG_WARNING,
                     "Dreadcast: forge recipe \"%s\" has multiple outputs; first is used as primary.",
                     rec.id.c_str());
        }
    }
}

} // namespace

bool loadGameData() {
    std::unordered_map<std::string, ItemData> newItems;
    CharacterAbilities newAb{};
    std::vector<CharacterClass> newChars{};
    std::unordered_map<std::string, EnemyArchetype> newEnemies;
    EnemyAiGlobals newEnemyGlobals{};

    loadItemsFromJson(newItems);
    if (newItems.empty()) {
        TraceLog(LOG_ERROR,
                 "Dreadcast: item catalog empty — ensure assets/data/items.json exists and is valid.");
        return false;
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
        rebuildCatalogItemsList();
        validateLoadedItemCatalog();
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

    loadEnemiesFromJson(newEnemies, newEnemyGlobals);
    const bool enemiesOk = !newEnemies.empty();
    if (!enemiesOk) {
        TraceLog(LOG_ERROR,
                 "Dreadcast: enemy catalog empty — ensure assets/data/enemies.json exists and is "
                 "valid.");
    } else {
        g_enemyCatalog = std::move(newEnemies);
        g_enemyAiGlobals = newEnemyGlobals;
        validateEnemyCatalog(g_enemyCatalog);
        rebuildEnemyArchetypeList(g_enemyCatalog, g_enemyArchetypeList);
    }

    const bool charsOk = !g_characters.empty();
    g_gameDataLoaded = itemsOk && abilitiesOk && charsOk && enemiesOk;
    if (itemsOk) {
        loadRecipesFromJson();
        validateCraftingRecipesAgainstCatalog();
    } else {
        seedCraftingRecipes();
    }
    if (g_gameDataLoaded) {
        runItemTransactionSelfTest();
    }
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

std::array<std::string, 3> rollCasketLoot(CasketTier tier) {
    const CasketTierRoll cfg = casketTierRoll(tier);
    const int itemCount = 1 + weightedPickIndex(cfg.countWeights.data(),
                                                static_cast<int>(cfg.countWeights.size()));

    // Reject-and-retry until the minimum-rarity guarantee is met. The floors are common enough
    // (Sealed: 70% Blighted+, Wrought: 60% Cursed+) that this converges in a handful of tries; the
    // cap then forces one slot up so we never loop forever (or when high-rarity stock is thin).
    constexpr int kMaxAttempts = 64;
    std::array<std::string, 3> slots{};
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        slots = {};
        int bestPool = -1;
        for (int i = 0; i < itemCount; ++i) {
            const int desired = weightedPickIndex(cfg.rarityWeights.data(), kCasketPoolCount);
            auto [id, pool] = pickCasketItem(desired);
            slots[static_cast<size_t>(i)] = std::move(id);
            bestPool = std::max(bestPool, pool);
        }
        if (bestPool >= cfg.minPoolFloor) {
            return slots;
        }
    }

    // Fallback: force the first slot to the lowest stocked pool that satisfies the floor.
    for (int p = cfg.minPoolFloor; p < kCasketPoolCount; ++p) {
        if (!g_casketRarityPools[static_cast<size_t>(p)].empty()) {
            slots[0] = pickCasketItem(p).first;
            break;
        }
    }
    return slots;
}

const std::vector<ItemData> &allCatalogItems() { return g_catalogItemsList; }

const CharacterAbilities &undeadHunterAbilities() { return g_undeadHunterAbilities; }

int characterCount() { return static_cast<int>(g_characters.size()); }

const CharacterClass &characterAt(int index) {
    static CharacterClass kFallback{};
    static bool fallbackInit = false;
    if (!fallbackInit) {
        fallbackInit = true;
        kFallback.id = "undead_hunter";
        kFallback.name = "Undead Hunter";
        kFallback.description = "";
        kFallback.bio =
            "A hunter cast out over rumors of dark blood. A cursed beast and cruel men tore his life "
            "apart; he died on his family's floor vowing vengeance — and woke in the Dread Pit, "
            "half-dead and hungry in ways that no longer feel human.";
        kFallback.detailAbilities =
            "- Ranged curse bolt (LMB) — hits at range; see Attack for damage, range, and bolt speed.\n"
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
        kFallback.rangedProjectileSpeed = 600.0F;
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

const std::vector<Recipe> &allRecipes() { return g_allRecipes; }

const std::vector<Recipe> &forgeRecipes() { return g_forgeRecipesCache; }

const std::vector<Recipe> &disassembleRecipes() { return g_disassembleRecipesCache; }

const Recipe *findRecipeById(const std::string &id) {
    for (const Recipe &r : g_allRecipes) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

const Recipe *findDisassembleRecipeBySourceId(const std::string &sourceCatalogId) {
    for (const Recipe &r : g_disassembleRecipesCache) {
        for (const CraftIngredient &in : r.inputs) {
            if (in.itemId == sourceCatalogId) {
                return &r;
            }
        }
    }
    return nullptr;
}

bool catalogIdIsForgeBenchInput(const std::string &catalogId) {
    return g_forgeBenchInputIds.find(catalogId) != g_forgeBenchInputIds.end();
}

bool catalogIdIsDisassembleBenchInput(const std::string &catalogId) {
    return g_disassembleBenchInputIds.find(catalogId) != g_disassembleBenchInputIds.end();
}

const EnemyArchetype *enemyArchetypeById(const std::string &id) {
    const auto it = g_enemyCatalog.find(id);
    if (it == g_enemyCatalog.end()) {
        return nullptr;
    }
    return &it->second;
}

const std::vector<EnemyArchetype> &allEnemyArchetypes() { return g_enemyArchetypeList; }

const EnemyAiGlobals &enemyAiGlobals() { return g_enemyAiGlobals; }

} // namespace dreadcast
