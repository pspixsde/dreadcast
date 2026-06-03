#pragma once

namespace dreadcast {

struct InventoryState;

/// Aggregated combat/survival modifiers from equipped gear (rebuilt when inventory changes).
struct PlayerEquipmentSnapshot {
    float maxHpBonus{0.0F};
    float maxManaBonus{0.0F};
    float hpRegenBonus{0.0F};
    float moveSpeedBonus{0.0F};
    float visionRangeBonus{0.0F};
    float damageReflect{0.0F};
    float abilityManaCostMultiplier{1.0F};
    float abilityCooldownManaRefund{0.0F};
};

[[nodiscard]] PlayerEquipmentSnapshot buildEquipmentSnapshot(const InventoryState &inv);

} // namespace dreadcast
