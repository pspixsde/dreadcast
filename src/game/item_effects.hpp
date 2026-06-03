#pragma once

#include <entt/entt.hpp>
#include <optional>
#include <string>

#include "game/equipment_snapshot.hpp"
#include "game/items.hpp"

namespace dreadcast {

class ResourceManager;

struct ItemOnUseOutcome {
    bool applied{false};
    bool cordialBlockedLowHp{false};
    /// 0 = none, 1 = HoT HUD, 2 = Manic HUD, 3 = mana-over-time HUD (maps to `StatusHudKind`).
    int hudKind{0};
    std::string hudIcon{};
};

/// Sum passive `OnEquipped` / `StatModifier` contributions from one item into `out`.
void aggregateEquippedItemPassives(const ItemData &item, PlayerEquipmentSnapshot &out);

/// If JSON had no `effects`, build standard `OnEquipped` stat rows from legacy `ItemData` fields.
void synthesizeLegacyItemEffects(ItemData &item);

[[nodiscard]] ItemOnUseOutcome applyItemOnUseEffectsOutcome(entt::registry &registry,
                                                              entt::entity player,
                                                              const ItemData &item,
                                                              ResourceManager &resources);

/// Per-frame / fixed-step ticking for HoT, mana-over-time, manic drain, runic cooldown.
void tickActiveItemEffects(entt::registry &registry, entt::entity player, float fixedDt,
                           ResourceManager &resources);

/// When player HP crosses threshold downward while wearing matching armor effect.
[[nodiscard]] bool tryProcHpThresholdEffects(entt::registry &registry, entt::entity player,
                                             const InventoryState &inventory, float hpBeforeTick,
                                             float hpNow, float hpMax, ResourceManager &resources);

/// Read Vigilant Eye tuning from equipped amulet item (or nullopt if not worn).
std::optional<ItemEffectAction> findVigilantStandingBonusAction(const ItemData *amuletItem);

[[nodiscard]] bool playerHasManicEffect(const entt::registry &registry, entt::entity player);

/// Invoked after HP damage is applied (`OnTakeDamage` item hooks; inventory optional).
void dispatchOnTakeDamageItemEffects(entt::registry &registry, entt::entity victim,
                                       const InventoryState *inventoryOpt, float damageDealt);

[[nodiscard]] bool playerHasHealOverTime(const entt::registry &registry, entt::entity player);
[[nodiscard]] bool playerHasManaRegenOverTime(const entt::registry &registry,
                                              entt::entity player);
[[nodiscard]] bool playerHasRunicShellCooldown(const entt::registry &registry,
                                               entt::entity player);

[[nodiscard]] float runicShellCooldownRatio(const entt::registry &registry, entt::entity player);
[[nodiscard]] float runicShellCooldownSecondsRemaining(const entt::registry &registry,
                                                       entt::entity player);

/// Minimum HP fraction required to drink Cordial Manic (from item `effects`, else 0.40).
[[nodiscard]] float cordialManicMinHpFractionFromItem(const ItemData &item);

/// Active manic speed multiplier from `ActiveItemEffectManic`, or 1 if none.
[[nodiscard]] float playerManicSpeedMultiplierOrOne(const entt::registry &registry,
                                                      entt::entity player);

} // namespace dreadcast
