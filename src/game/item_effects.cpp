#include "game/item_effects.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "config.hpp"
#include "core/resource_manager.hpp"
#include "ecs/components.hpp"

namespace dreadcast {

namespace {

ecs::ActiveItemEffects &ensureFx(entt::registry &registry, entt::entity player) {
    return registry.get_or_emplace<ecs::ActiveItemEffects>(player);
}

template <typename T>
void removeEntriesOfKind(ecs::ActiveItemEffects &fx) {
    fx.entries.erase(std::remove_if(fx.entries.begin(), fx.entries.end(),
                                   [](const ecs::ActiveItemEffectEntry &e) {
                                       return std::holds_alternative<T>(e);
                                   }),
                     fx.entries.end());
}

template <typename T>
[[nodiscard]] T *findEntryMut(ecs::ActiveItemEffects &fx) {
    for (auto &e : fx.entries) {
        if (auto *p = std::get_if<T>(&e)) {
            return p;
        }
    }
    return nullptr;
}

template <typename T>
[[nodiscard]] const T *findEntryConst(const ecs::ActiveItemEffects &fx) {
    for (const auto &e : fx.entries) {
        if (const auto *p = std::get_if<T>(&e)) {
            return p;
        }
    }
    return nullptr;
}

void applyStatModifierToSnapshot(const ItemEffectAction &a, PlayerEquipmentSnapshot &out) {
    if (a.statName == "maxHp" && a.statKind == StatModifierKind::Add) {
        out.maxHpBonus += a.statValue;
    } else if (a.statName == "maxMana" && a.statKind == StatModifierKind::Add) {
        out.maxManaBonus += a.statValue;
    } else if (a.statName == "hpRegen" && a.statKind == StatModifierKind::Add) {
        out.hpRegenBonus += a.statValue;
    } else if (a.statName == "moveSpeed" && a.statKind == StatModifierKind::Add) {
        out.moveSpeedBonus += a.statValue;
    } else if (a.statName == "visionRange" && a.statKind == StatModifierKind::Add) {
        out.visionRangeBonus += a.statValue;
    } else if (a.statName == "damageReflect" && a.statKind == StatModifierKind::Add) {
        out.damageReflect += a.statValue;
    } else if (a.statName == "abilityManaCost") {
        if (a.statKind == StatModifierKind::Mul) {
            if (a.statValue > 0.001F) {
                out.abilityManaCostMultiplier *= a.statValue;
            }
        }
    } else if (a.statName == "abilityCooldownManaRefund" && a.statKind == StatModifierKind::Add) {
        out.abilityCooldownManaRefund += a.statValue;
    }
}

} // namespace

void aggregateEquippedItemPassives(const ItemData &item, PlayerEquipmentSnapshot &out) {
    for (const ItemEffectDef &eff : item.effects) {
        if (eff.trigger != ItemEffectTrigger::OnEquipped) {
            continue;
        }
        if (eff.action.kind == ItemEffectActionKind::StatModifier) {
            applyStatModifierToSnapshot(eff.action, out);
        }
    }
}

void synthesizeLegacyItemEffects(ItemData &it) {
    if (!it.effects.empty()) {
        return;
    }
    auto pushStat = [&](const char *stat, float v, StatModifierKind k) {
        if (std::fabs(v) < 1.0e-6F) {
            return;
        }
        ItemEffectDef d{};
        d.trigger = ItemEffectTrigger::OnEquipped;
        d.action.kind = ItemEffectActionKind::StatModifier;
        d.action.statName = stat;
        d.action.statValue = v;
        d.action.statKind = k;
        it.effects.push_back(d);
    };
    pushStat("maxHp", it.maxHpBonus, StatModifierKind::Add);
    pushStat("maxMana", it.maxManaBonus, StatModifierKind::Add);
    pushStat("hpRegen", it.hpRegenBonus, StatModifierKind::Add);
    pushStat("moveSpeed", it.moveSpeedBonus, StatModifierKind::Add);
    pushStat("visionRange", it.visionRangeBonus, StatModifierKind::Add);
    pushStat("damageReflect", it.damageReflectPercent, StatModifierKind::Add);
    if (std::fabs(it.abilityManaCostMultiplier - 1.0F) > 1.0e-4F) {
        pushStat("abilityManaCost", it.abilityManaCostMultiplier, StatModifierKind::Mul);
    }
    pushStat("abilityCooldownManaRefund", it.abilityCooldownManaRefund, StatModifierKind::Add);

    if (it.isConsumable && it.catalogId == "vial_pure_blood") {
        ItemEffectDef u{};
        u.trigger = ItemEffectTrigger::OnUse;
        u.action.kind = ItemEffectActionKind::HealOverTime;
        u.action.overTimeTotal = config::HOT_TOTAL_HEAL;
        u.action.overTimeDuration = config::HOT_DURATION;
        it.effects.push_back(u);
    } else if (it.isConsumable && it.catalogId == "vial_raw_spirit") {
        ItemEffectDef u{};
        u.trigger = ItemEffectTrigger::OnUse;
        u.action.kind = ItemEffectActionKind::ManaOverTime;
        u.action.overTimeTotal = config::RAW_SPIRIT_MANA_TOTAL;
        u.action.overTimeDuration = config::RAW_SPIRIT_DURATION;
        it.effects.push_back(u);
    } else if (it.isConsumable && it.catalogId == "vial_cordial_manic") {
        ItemEffectDef u{};
        u.trigger = ItemEffectTrigger::OnUse;
        u.action.kind = ItemEffectActionKind::SpeedWindow;
        u.action.speedMultiplier = config::MANIC_SPEED_MULTIPLIER;
        u.action.speedWindowDuration = config::MANIC_DURATION;
        u.action.speedWindowMinHpFraction = config::MANIC_MIN_HP_FRACTION;
        u.action.speedWindowDrainHpFraction = config::MANIC_HP_DRAIN_PERCENT;
        u.action.speedWindowInvulnerable = true;
        it.effects.push_back(u);
    } else if (it.catalogId == "runic_shell" && !it.isConsumable) {
        ItemEffectDef th{};
        th.trigger = ItemEffectTrigger::OnHpThreshold;
        th.condition.hpFractionAtMost = 0.30F;
        th.condition.hpEdge = HpThresholdEdge::Downward;
        th.condition.cooldownSeconds = config::RUNIC_SHELL_COOLDOWN;
        th.action.kind = ItemEffectActionKind::AoeShockwave;
        th.action.shockwaveRadius = config::RUNIC_SHELL_RADIUS;
        th.action.shockwaveDamage = config::RUNIC_SHELL_DAMAGE;
        th.action.shockwaveKnockback = config::RUNIC_SHELL_KNOCKBACK;
        th.action.shockwaveSelfHeal = config::RUNIC_SHELL_HEAL;
        th.action.shockwaveCooldown = config::RUNIC_SHELL_COOLDOWN;
        it.effects.push_back(th);
    } else if (it.catalogId == "vigilant_eye" && !it.isConsumable) {
        ItemEffectDef v1{};
        v1.trigger = ItemEffectTrigger::OnEquipped;
        v1.action.kind = ItemEffectActionKind::StatModifier;
        v1.action.statName = "visionRange";
        v1.action.statValue = it.visionRangeBonus > 0.001F ? it.visionRangeBonus
                                                           : config::VIGILANT_EYE_BASE_BONUS;
        v1.action.statKind = StatModifierKind::Add;
        it.effects.push_back(v1);

        ItemEffectDef v2{};
        v2.trigger = ItemEffectTrigger::OnStandingStill;
        v2.action.kind = ItemEffectActionKind::VisionStandingBonus;
        v2.action.standingStillSeconds = config::VIGILANT_EYE_STILL_SECONDS;
        v2.action.standingVisionBonus = config::VIGILANT_EYE_STILL_BONUS;
        it.effects.push_back(v2);
    }
}

ItemOnUseOutcome applyItemOnUseEffectsOutcome(entt::registry &registry, entt::entity player,
                                              const ItemData &item, ResourceManager &resources) {
    ItemOnUseOutcome out{};
    if (!registry.valid(player)) {
        return out;
    }
    for (const ItemEffectDef &eff : item.effects) {
        if (eff.trigger != ItemEffectTrigger::OnUse) {
            continue;
        }
        const ItemEffectAction &ac = eff.action;
        if (ac.kind == ItemEffectActionKind::HealOverTime) {
            auto &fx = ensureFx(registry, player);
            removeEntriesOfKind<ecs::ActiveItemEffectHot>(fx);
            ecs::ActiveItemEffectHot hot{};
            hot.totalHeal = ac.overTimeTotal;
            hot.duration = ac.overTimeDuration;
            hot.sourceCatalogId = item.catalogId;
            fx.entries.push_back(hot);
            out.applied = true;
            out.hudKind = 1;
            out.hudIcon = item.iconPath;
            const SoundHandle pb = resources.getSound("assets/sounds/items/vial_consume.wav");
            if (pb >= 0) {
                resources.audio().playOneShot(pb, 1.0F, 1.0F);
            }
            const SoundHandle loop = resources.getSound("assets/sounds/items/vial_pure_blood_loop.wav");
            if (loop >= 0) {
                resources.audio().playExclusiveLoop(loop, 0.55F, 1.0F);
            }
            return out;
        }
        if (ac.kind == ItemEffectActionKind::ManaOverTime) {
            auto &fx = ensureFx(registry, player);
            removeEntriesOfKind<ecs::ActiveItemEffectManaRot>(fx);
            ecs::ActiveItemEffectManaRot mr{};
            mr.totalMana = ac.overTimeTotal;
            mr.duration = ac.overTimeDuration;
            mr.sourceCatalogId = item.catalogId;
            fx.entries.push_back(mr);
            out.applied = true;
            out.hudKind = 3;
            out.hudIcon = item.iconPath;
            const SoundHandle pb = resources.getSound("assets/sounds/items/vial_consume.wav");
            if (pb >= 0) {
                resources.audio().playOneShot(pb, 1.0F, 1.0F);
            }
            return out;
        }
        if (ac.kind == ItemEffectActionKind::SpeedWindow) {
            if (!registry.all_of<ecs::Health>(player)) {
                return out;
            }
            const auto &hp = registry.get<ecs::Health>(player);
            const float minFrac = ac.speedWindowMinHpFraction > 0.001F ? ac.speedWindowMinHpFraction
                                                                       : 0.40F;
            if (hp.max > 0.001F && hp.current / hp.max < minFrac + 1.0e-4F) {
                out.cordialBlockedLowHp = true;
                return out;
            }
            auto &fx = ensureFx(registry, player);
            removeEntriesOfKind<ecs::ActiveItemEffectManic>(fx);
            ecs::ActiveItemEffectManic me{};
            me.duration = ac.speedWindowDuration;
            me.speedMultiplier = ac.speedMultiplier > 0.001F ? ac.speedMultiplier
                                                             : config::MANIC_SPEED_MULTIPLIER;
            const float drainFrac = ac.speedWindowDrainHpFraction > 0.001F
                                        ? ac.speedWindowDrainHpFraction
                                        : 0.40F;
            me.hpDrainTotal = hp.max * drainFrac;
            me.sourceCatalogId = item.catalogId;
            fx.entries.push_back(me);
            out.applied = true;
            out.hudKind = 2;
            out.hudIcon = item.iconPath;
            const SoundHandle pb = resources.getSound("assets/sounds/items/vial_consume.wav");
            if (pb >= 0) {
                resources.audio().playOneShot(pb, 1.0F, 1.0F);
            }
            const SoundHandle hb = resources.getSound("assets/sounds/items/vial_cordial_manic_heartbeat.wav");
            if (hb >= 0) {
                const float pitch = 9.0F / std::max(0.001F, me.duration);
                resources.audio().playExclusive(hb, pitch, 0.0F);
            }
            return out;
        }
    }
    return out;
}

void tickActiveItemEffects(entt::registry &registry, entt::entity player, float fixedDt,
                           ResourceManager &resources) {
    if (!registry.valid(player) || !registry.all_of<ecs::ActiveItemEffects>(player)) {
        return;
    }
    auto &fx = registry.get<ecs::ActiveItemEffects>(player);

    // --- HoT ---
    if (auto *hot = findEntryMut<ecs::ActiveItemEffectHot>(fx)) {
        if (registry.all_of<ecs::Health>(player)) {
            auto &hp = registry.get<ecs::Health>(player);
            if (hp.current <= 0.001F) {
                const SoundHandle pb = resources.getSound("assets/sounds/items/vial_pure_blood_loop.wav");
                if (pb >= 0) {
                    resources.audio().stopExclusive(pb);
                }
                removeEntriesOfKind<ecs::ActiveItemEffectHot>(fx);
            } else if (!findEntryConst<ecs::ActiveItemEffectManic>(fx)) {
                hot->elapsed += fixedDt;
                const float rate = hot->totalHeal / std::max(1.0e-4F, hot->duration);
                const float wantHeal = rate * fixedDt;
                const float room = hot->totalHeal - hot->healedSoFar;
                const float add = std::min(wantHeal, std::max(0.0F, room));
                hp.current = std::min(hp.max, hp.current + add);
                hot->healedSoFar += add;
                if (hot->elapsed >= hot->duration - 1.0e-4F ||
                    hot->healedSoFar >= hot->totalHeal - 1.0e-3F) {
                    const SoundHandle pb = resources.getSound("assets/sounds/items/vial_pure_blood_loop.wav");
                    if (pb >= 0) {
                        resources.audio().stopExclusive(pb);
                    }
                    removeEntriesOfKind<ecs::ActiveItemEffectHot>(fx);
                }
            } else {
                const SoundHandle pb = resources.getSound("assets/sounds/items/vial_pure_blood_loop.wav");
                if (pb >= 0) {
                    resources.audio().stopExclusive(pb);
                }
                removeEntriesOfKind<ecs::ActiveItemEffectHot>(fx);
            }
        }
    }

    // --- Mana over time ---
    if (auto *mr = findEntryMut<ecs::ActiveItemEffectManaRot>(fx)) {
        if (registry.all_of<ecs::Mana>(player)) {
            auto &mana = registry.get<ecs::Mana>(player);
            mr->elapsed += fixedDt;
            const float rate = mr->totalMana / std::max(1.0e-4F, mr->duration);
            const float want = rate * fixedDt;
            const float room = mr->totalMana - mr->regenedSoFar;
            const float add = std::min(want, std::max(0.0F, room));
            mana.current = std::min(mana.max, mana.current + add);
            mr->regenedSoFar += add;
            if (mr->elapsed >= mr->duration - 1.0e-4F ||
                mr->regenedSoFar >= mr->totalMana - 1.0e-3F) {
                removeEntriesOfKind<ecs::ActiveItemEffectManaRot>(fx);
            }
        }
    }

    // --- Manic ---
    if (auto *me = findEntryMut<ecs::ActiveItemEffectManic>(fx)) {
        if (!registry.all_of<ecs::Health>(player)) {
            removeEntriesOfKind<ecs::ActiveItemEffectManic>(fx);
        } else {
            auto &hp = registry.get<ecs::Health>(player);
            me->elapsed += fixedDt;
            const float t = me->elapsed / std::max(1.0e-4F, me->duration);
            if (t < 1.0F && hp.max > 0.001F) {
                const float add = (me->hpDrainTotal / std::max(1.0e-4F, me->duration)) * fixedDt;
                me->hpDrained += add;
                hp.current = std::max(0.0F, hp.current - add);
            }
            if (me->elapsed >= me->duration - 1.0e-4F ||
                me->hpDrained >= me->hpDrainTotal - 1.0e-3F) {
                const SoundHandle hb =
                    resources.getSound("assets/sounds/items/vial_cordial_manic_heartbeat.wav");
                if (hb >= 0) {
                    resources.audio().stopExclusive(hb);
                }
                removeEntriesOfKind<ecs::ActiveItemEffectManic>(fx);
            }
        }
    }

    // --- Runic Shell cooldown ---
    if (auto *cd = findEntryMut<ecs::ActiveItemEffectRunicCd>(fx)) {
        cd->remaining -= fixedDt;
        if (cd->remaining <= 0.0F) {
            removeEntriesOfKind<ecs::ActiveItemEffectRunicCd>(fx);
        }
    }
}

bool tryProcHpThresholdEffects(entt::registry &registry, entt::entity player,
                               const InventoryState &inventory, float hpAtTickStart, float hpNow,
                               float hpMax, ResourceManager &resources) {
    (void)resources;
    if (!registry.valid(player) || hpMax < 0.001F) {
        return false;
    }
    const int armorIdx = inventory.equipped[static_cast<size_t>(EquipSlot::Armor)];
    if (armorIdx < 0 || armorIdx >= static_cast<int>(inventory.items.size())) {
        return false;
    }
    const ItemData &armor = inventory.items[static_cast<size_t>(armorIdx)];
    for (const ItemEffectDef &eff : armor.effects) {
        if (eff.trigger != ItemEffectTrigger::OnHpThreshold) {
            continue;
        }
        if (eff.action.kind != ItemEffectActionKind::AoeShockwave) {
            continue;
        }
        const float threshFrac = eff.condition.hpFractionAtMost > 0.001F ? eff.condition.hpFractionAtMost
                                                                         : 0.30F;
        const float thresh = threshFrac * hpMax;
        if (!(hpAtTickStart > thresh + 1.0e-4F && hpNow <= thresh + 1.0e-4F)) {
            continue;
        }
        auto &fx = ensureFx(registry, player);
        if (findEntryConst<ecs::ActiveItemEffectRunicCd>(fx)) {
            continue;
        }
        const float radius = eff.action.shockwaveRadius > 1.0F ? eff.action.shockwaveRadius
                                                                 : config::RUNIC_SHELL_RADIUS;
        const float radiusSq = radius * radius;
        const float dmg = eff.action.shockwaveDamage > 0.001F ? eff.action.shockwaveDamage
                                                              : config::RUNIC_SHELL_DAMAGE;
        const float kb = eff.action.shockwaveKnockback > 0.001F ? eff.action.shockwaveKnockback
                                                                : config::RUNIC_SHELL_KNOCKBACK;
        const float heal = eff.action.shockwaveSelfHeal > 0.001F ? eff.action.shockwaveSelfHeal
                                                                 : config::RUNIC_SHELL_HEAL;
        const float cdDur = eff.action.shockwaveCooldown > 0.001F ? eff.action.shockwaveCooldown
                                                                  : config::RUNIC_SHELL_COOLDOWN;

        if (!registry.all_of<ecs::Transform, ecs::Health>(player)) {
            return false;
        }
        const auto &pt = registry.get<ecs::Transform>(player);
        const auto enemies = registry.view<ecs::Enemy, ecs::Transform, ecs::Health>();
        for (const auto e : enemies) {
            const auto &et = registry.get<ecs::Transform>(e);
            const float dx = et.position.x - pt.position.x;
            const float dy = et.position.y - pt.position.y;
            if (dx * dx + dy * dy > radiusSq) {
                continue;
            }
            auto &eh = registry.get<ecs::Health>(e);
            eh.current -= dmg;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.001F) {
                auto &vel = registry.get_or_emplace<ecs::Velocity>(e);
                vel.value.x = (dx / dist) * kb;
                vel.value.y = (dy / dist) * kb;
                registry.emplace_or_replace<ecs::KnockbackState>(
                    e, ecs::KnockbackState{config::KNOCKBACK_DURATION * 1.5F, 0.0F});
            }
        }
        if (registry.all_of<ecs::Health>(player)) {
            auto &hpMut = registry.get<ecs::Health>(player);
            if (!findEntryConst<ecs::ActiveItemEffectManic>(fx)) {
                hpMut.current = std::min(hpMut.max, hpMut.current + heal);
            }
        }
        ecs::ActiveItemEffectRunicCd cd{};
        cd.remaining = cdDur;
        cd.total = cdDur;
        cd.sourceCatalogId = armor.catalogId;
        fx.entries.push_back(cd);
        return true;
    }
    return false;
}

std::optional<ItemEffectAction> findVigilantStandingBonusAction(const ItemData *amuletItem) {
    if (amuletItem == nullptr) {
        return std::nullopt;
    }
    for (const ItemEffectDef &eff : amuletItem->effects) {
        if (eff.trigger == ItemEffectTrigger::OnStandingStill &&
            eff.action.kind == ItemEffectActionKind::VisionStandingBonus) {
            return eff.action;
        }
    }
    return std::nullopt;
}

bool playerHasManicEffect(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    return fx != nullptr && findEntryConst<ecs::ActiveItemEffectManic>(*fx) != nullptr;
}

void dispatchOnTakeDamageItemEffects(entt::registry &, entt::entity,
                                     const InventoryState *invOpt, const float damageDealt) {
    if (invOpt == nullptr || damageDealt <= 0.001F) {
        return;
    }
    for (const int poolIdx : invOpt->equipped) {
        if (poolIdx < 0 || poolIdx >= static_cast<int>(invOpt->items.size())) {
            continue;
        }
        const ItemData &gear = invOpt->items[static_cast<size_t>(poolIdx)];
        for (const ItemEffectDef &eff : gear.effects) {
            if (eff.trigger != ItemEffectTrigger::OnTakeDamage) {
                continue;
            }
            // Data-driven OnTakeDamage hooks can be executed here (currently none in catalog).
            (void)eff;
        }
    }
}

bool playerHasHealOverTime(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    return fx != nullptr && findEntryConst<ecs::ActiveItemEffectHot>(*fx) != nullptr;
}

bool playerHasManaRegenOverTime(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    return fx != nullptr && findEntryConst<ecs::ActiveItemEffectManaRot>(*fx) != nullptr;
}

bool playerHasRunicShellCooldown(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    return fx != nullptr && findEntryConst<ecs::ActiveItemEffectRunicCd>(*fx) != nullptr;
}

float runicShellCooldownRatio(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    if (fx == nullptr) {
        return 0.0F;
    }
    const auto *cd = findEntryConst<ecs::ActiveItemEffectRunicCd>(*fx);
    if (cd == nullptr || cd->total < 0.001F) {
        return 0.0F;
    }
    return std::clamp(cd->remaining / cd->total, 0.0F, 1.0F);
}

float runicShellCooldownSecondsRemaining(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    if (fx == nullptr) {
        return 0.0F;
    }
    const auto *cd = findEntryConst<ecs::ActiveItemEffectRunicCd>(*fx);
    return cd != nullptr ? std::max(0.0F, cd->remaining) : 0.0F;
}

float cordialManicMinHpFractionFromItem(const ItemData &item) {
    for (const ItemEffectDef &eff : item.effects) {
        if (eff.trigger != ItemEffectTrigger::OnUse) {
            continue;
        }
        if (eff.action.kind != ItemEffectActionKind::SpeedWindow) {
            continue;
        }
        if (eff.action.speedWindowMinHpFraction > 0.001F) {
            return eff.action.speedWindowMinHpFraction;
        }
        return 0.40F;
    }
    return 0.40F;
}

float playerManicSpeedMultiplierOrOne(const entt::registry &registry, entt::entity player) {
    const auto *fx = registry.try_get<ecs::ActiveItemEffects>(player);
    if (fx == nullptr) {
        return 1.0F;
    }
    const auto *me = findEntryConst<ecs::ActiveItemEffectManic>(*fx);
    if (me == nullptr || me->speedMultiplier < 0.001F) {
        return 1.0F;
    }
    return me->speedMultiplier;
}

} // namespace dreadcast
