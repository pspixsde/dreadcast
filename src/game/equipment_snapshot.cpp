#include "game/equipment_snapshot.hpp"

#include "game/item_effects.hpp"
#include "game/items.hpp"

namespace dreadcast {

PlayerEquipmentSnapshot buildEquipmentSnapshot(const InventoryState &inv) {
    PlayerEquipmentSnapshot s{};
    s.abilityManaCostMultiplier = 1.0F;
    for (size_t i = 0; i < inv.equipped.size(); ++i) {
        const int idx = inv.equipped[i];
        if (idx < 0 || idx >= static_cast<int>(inv.items.size())) {
            continue;
        }
        const ItemData &it = inv.items[static_cast<size_t>(idx)];
        if (!it.effects.empty()) {
            aggregateEquippedItemPassives(it, s);
        } else {
            s.maxHpBonus += it.maxHpBonus;
            s.maxManaBonus += it.maxManaBonus;
            s.hpRegenBonus += it.hpRegenBonus;
            s.moveSpeedBonus += it.moveSpeedBonus;
            s.visionRangeBonus += it.visionRangeBonus;
            s.damageReflect += it.damageReflectPercent;
            const float m = it.abilityManaCostMultiplier;
            if (m > 0.001F) {
                s.abilityManaCostMultiplier *= m;
            }
            s.abilityCooldownManaRefund += it.abilityCooldownManaRefund;
        }
    }
    return s;
}

} // namespace dreadcast
