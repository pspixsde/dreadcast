#include "ui/slot_widget.hpp"

#include "core/resource_manager.hpp"
#include "game/items.hpp"
#include "ui/inventory_ui.hpp"

namespace dreadcast::ui {

void SlotWidget::drawSurface(Rectangle slotRect, const dreadcast::ItemData *item, bool ghost) {
    InventoryUI::drawItemSlotSurface(slotRect, item, ghost);
}

void SlotWidget::drawIcon(const dreadcast::ItemData &item, dreadcast::ResourceManager &resources,
                          Rectangle slotRect, Color tint) {
    InventoryUI::drawItemIcon(item, resources, slotRect, tint);
}

} // namespace dreadcast::ui
