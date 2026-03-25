#include "ui/inventory_ui.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "config.hpp"
#include "ui/theme.hpp"

namespace dreadcast::ui {

namespace {

Rectangle makeEquipSlotRect(int screenW, int screenH, int equipIndex) {
    (void)screenW;
    const float panelW = 680.0F;
    const float panelH = 420.0F;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    const float colW = 220.0F;
    const float slotH = 56.0F;
    const float gap = 10.0F;
    const float x = px + 24.0F;
    const float y0 = py + 80.0F;
    return {x, y0 + static_cast<float>(equipIndex) * (slotH + gap), colW - 16.0F, slotH};
}

Rectangle makeConsumableSlotRect(int screenW, int screenH, int consumableIndex) {
    const float panelW = 680.0F;
    const float panelH = 420.0F;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    const float slotH = 56.0F;
    const float gap = 10.0F;
    const float y0 = py + 80.0F;
    const float afterEquip = y0 + 3.0F * (slotH + gap) + 24.0F;
    const float cellW = 104.0F;
    const float cellH = 48.0F;
    const float x = px + 24.0F + static_cast<float>(consumableIndex) * (cellW + gap);
    return {x, afterEquip, cellW, cellH};
}

Rectangle makeBagSlotRect(int screenW, int screenH, int bagIndex) {
    const float panelW = 680.0F;
    const float panelH = 420.0F;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    const float col0 = px + 320.0F;
    const int row = bagIndex / 3;
    const int col = bagIndex % 3;
    const float cellW = 80.0F;
    const float cellH = 44.0F;
    const float gap = 10.0F;
    return {col0 + static_cast<float>(col) * (cellW + gap),
            py + 100.0F + static_cast<float>(row) * (cellH + gap), cellW, cellH};
}

int hitTestBag(int screenW, int screenH, Vector2 mouse) {
    for (int i = 0; i < dreadcast::BAG_SLOT_COUNT; ++i) {
        if (CheckCollisionPointRec(mouse, makeBagSlotRect(screenW, screenH, i))) {
            return i;
        }
    }
    return -1;
}

int hitTestEquip(int screenW, int screenH, Vector2 mouse) {
    for (int i = 0; i < static_cast<int>(EquipSlot::COUNT); ++i) {
        if (CheckCollisionPointRec(mouse, makeEquipSlotRect(screenW, screenH, i))) {
            return i;
        }
    }
    return -1;
}

int hitTestConsumable(int screenW, int screenH, Vector2 mouse) {
    for (int i = 0; i < dreadcast::CONSUMABLE_SLOT_COUNT; ++i) {
        if (CheckCollisionPointRec(mouse, makeConsumableSlotRect(screenW, screenH, i))) {
            return i;
        }
    }
    return -1;
}

std::string itemLabel(const dreadcast::ItemData &it) {
    std::string s = it.name;
    if (it.isStackable && it.stackCount > 1) {
        s += " x";
        s += std::to_string(it.stackCount);
    }
    return s;
}

void clampRectToScreen(Rectangle &r, int screenW, int screenH) {
    if (r.x < 4.0F) {
        r.x = 4.0F;
    }
    if (r.y < 4.0F) {
        r.y = 4.0F;
    }
    if (r.x + r.width > static_cast<float>(screenW) - 4.0F) {
        r.x = static_cast<float>(screenW) - r.width - 4.0F;
    }
    if (r.y + r.height > static_cast<float>(screenH) - 4.0F) {
        r.y = static_cast<float>(screenH) - r.height - 4.0F;
    }
}

} // namespace

void InventoryUI::tryEquipFromBag(InventoryState &inv, int bagIndex) {
    if (bagIndex < 0 || bagIndex >= dreadcast::BAG_SLOT_COUNT) {
        return;
    }
    const int itemIdx = inv.bagSlots[static_cast<size_t>(bagIndex)];
    if (itemIdx < 0 || itemIdx >= static_cast<int>(inv.items.size())) {
        return;
    }
    const EquipSlot targetSlot = inv.items[static_cast<size_t>(itemIdx)].slot;
    const int si = static_cast<int>(targetSlot);
    int &eq = inv.equipped[static_cast<size_t>(si)];
    if (eq >= 0) {
        std::swap(eq, inv.bagSlots[static_cast<size_t>(bagIndex)]);
    } else {
        eq = itemIdx;
        inv.bagSlots[static_cast<size_t>(bagIndex)] = -1;
    }
}

void InventoryUI::tryUnequip(InventoryState &inv, EquipSlot slot) {
    const int itemIdx = inv.equipped[static_cast<size_t>(slot)];
    if (itemIdx < 0) {
        return;
    }
    const int bag = inv.firstEmptyBagSlot();
    if (bag < 0) {
        return;
    }
    inv.bagSlots[static_cast<size_t>(bag)] = itemIdx;
    inv.equipped[static_cast<size_t>(slot)] = -1;
}

void InventoryUI::moveEquippedToBagSlot(InventoryState &inv, EquipSlot slot, int bagIdx) {
    if (bagIdx < 0 || bagIdx >= dreadcast::BAG_SLOT_COUNT) {
        return;
    }
    const int eIdx = inv.equipped[static_cast<size_t>(slot)];
    if (eIdx < 0) {
        return;
    }
    const int bIdx = inv.bagSlots[static_cast<size_t>(bagIdx)];
    if (bIdx < 0) {
        inv.bagSlots[static_cast<size_t>(bagIdx)] = eIdx;
        inv.equipped[static_cast<size_t>(slot)] = -1;
        return;
    }
    if (inv.items[static_cast<size_t>(bIdx)].slot == slot) {
        inv.equipped[static_cast<size_t>(slot)] = bIdx;
        inv.bagSlots[static_cast<size_t>(bagIdx)] = eIdx;
    }
}

void InventoryUI::swapBagSlots(InventoryState &inv, int a, int b) {
    if (a < 0 || b < 0 || a >= dreadcast::BAG_SLOT_COUNT || b >= dreadcast::BAG_SLOT_COUNT) {
        return;
    }
    std::swap(inv.bagSlots[static_cast<size_t>(a)], inv.bagSlots[static_cast<size_t>(b)]);
}

InventoryAction InventoryUI::update(InputManager &input, InventoryState &inv) {
    InventoryAction action{};
    if (!open_) {
        dragging_ = false;
        contextOpen_ = false;
        return action;
    }

    const int sw = config::WINDOW_WIDTH;
    const int sh = config::WINDOW_HEIGHT;
    const Vector2 mouse = input.mousePosition();
    const bool leftPress = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool leftRelease = input.isMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const bool rightPress = input.isMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (input.isKeyPressed(KEY_ESCAPE) && contextOpen_) {
        contextOpen_ = false;
        return action;
    }

    if (contextOpen_) {
        if (leftPress) {
            if (CheckCollisionPointRec(mouse, contextOpt0_)) {
                if (contextIsCarried_ && contextOpt0IsEquip_) {
                    tryEquipFromBag(inv, contextBagSlot_);
                } else if (!contextIsCarried_ && !contextOpt0IsEquip_) {
                    tryUnequip(inv, static_cast<EquipSlot>(contextEquipSlot_));
                }
                contextOpen_ = false;
                return action;
            }
            if (CheckCollisionPointRec(mouse, contextOpt1_)) {
                action.type = InventoryAction::Drop;
                action.itemIndex = contextItemIndex_;
                if (contextIsCarried_) {
                    action.dropFromEquipped = false;
                    action.bagSlot = contextBagSlot_;
                    inv.bagSlots[static_cast<size_t>(contextBagSlot_)] = -1;
                } else {
                    action.dropFromEquipped = true;
                    action.equipSlot = static_cast<EquipSlot>(contextEquipSlot_);
                    inv.equipped[static_cast<size_t>(contextEquipSlot_)] = -1;
                }
                contextOpen_ = false;
                return action;
            }
            if (!CheckCollisionPointRec(mouse, contextRect_)) {
                contextOpen_ = false;
            }
        }
        if (leftRelease) {
            dragging_ = false;
        }
        return action;
    }

    if (rightPress && !dragging_) {
        const int bagHit = hitTestBag(sw, sh, mouse);
        if (bagHit >= 0) {
            const int idx = inv.bagSlots[static_cast<size_t>(bagHit)];
            if (idx >= 0) {
                const float rowH = 32.0F;
                const float menuW = 168.0F;
                const float menuH = rowH * 2.0F + 10.0F;
                contextRect_ = {mouse.x, mouse.y, menuW, menuH};
                clampRectToScreen(contextRect_, sw, sh);
                contextOpt0_ = {contextRect_.x + 4.0F, contextRect_.y + 4.0F,
                                contextRect_.width - 8.0F, rowH};
                contextOpt1_ = {contextRect_.x + 4.0F, contextRect_.y + 5.0F + rowH,
                                contextRect_.width - 8.0F, rowH};
                contextItemIndex_ = idx;
                contextBagSlot_ = bagHit;
                contextEquipSlot_ = -1;
                contextIsCarried_ = true;
                contextOpt0IsEquip_ = true;
                contextOpen_ = true;
            }
            return action;
        }
        const int eqHit = hitTestEquip(sw, sh, mouse);
        if (eqHit >= 0) {
            const int idx = inv.equipped[static_cast<size_t>(eqHit)];
            if (idx >= 0) {
                const float rowH = 32.0F;
                const float menuW = 168.0F;
                const float menuH = rowH * 2.0F + 10.0F;
                contextRect_ = {mouse.x, mouse.y, menuW, menuH};
                clampRectToScreen(contextRect_, sw, sh);
                contextOpt0_ = {contextRect_.x + 4.0F, contextRect_.y + 4.0F,
                                contextRect_.width - 8.0F, rowH};
                contextOpt1_ = {contextRect_.x + 4.0F, contextRect_.y + 5.0F + rowH,
                                contextRect_.width - 8.0F, rowH};
                contextItemIndex_ = idx;
                contextBagSlot_ = -1;
                contextEquipSlot_ = eqHit;
                contextIsCarried_ = false;
                contextOpt0IsEquip_ = false;
                contextOpen_ = true;
            }
        }
        return action;
    }

    if (leftPress && !dragging_) {
        const int b = hitTestBag(sw, sh, mouse);
        if (b >= 0) {
            const int idx = inv.bagSlots[static_cast<size_t>(b)];
            if (idx >= 0) {
                dragging_ = true;
                dragItemIndex_ = idx;
                dragSourceBag_ = b;
                dragSourceEquip_ = -1;
            }
        } else {
            const int e = hitTestEquip(sw, sh, mouse);
            if (e >= 0) {
                const int idx = inv.equipped[static_cast<size_t>(e)];
                if (idx >= 0) {
                    dragging_ = true;
                    dragItemIndex_ = idx;
                    dragSourceBag_ = -1;
                    dragSourceEquip_ = e;
                }
            }
        }
        return action;
    }

    if (leftRelease && dragging_) {
        dragging_ = false;
        const int bagHit = hitTestBag(sw, sh, mouse);
        const int eqHit = hitTestEquip(sw, sh, mouse);

        if (dragSourceBag_ >= 0) {
            if (eqHit >= 0) {
                const auto targetEq = static_cast<EquipSlot>(eqHit);
                if (inv.items[static_cast<size_t>(dragItemIndex_)].slot == targetEq) {
                    tryEquipFromBag(inv, dragSourceBag_);
                }
            } else if (bagHit >= 0 && bagHit != dragSourceBag_) {
                swapBagSlots(inv, dragSourceBag_, bagHit);
            }
        } else if (dragSourceEquip_ >= 0) {
            if (bagHit >= 0) {
                moveEquippedToBagSlot(inv, static_cast<EquipSlot>(dragSourceEquip_), bagHit);
            }
        }
        dragSourceBag_ = -1;
        dragSourceEquip_ = -1;
        dragItemIndex_ = -1;
    }

    return action;
}

void InventoryUI::draw(const Font &font, int screenW, int screenH,
                       const InventoryState &inv) {
    if (!open_) {
        return;
    }

    const float panelW = 680.0F;
    const float panelH = 420.0F;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    DrawRectangle(0, 0, screenW, screenH, ui::theme::INVENTORY_OVERLAY);
    Color panelFill = ui::theme::PANEL_FILL;
    panelFill.a = 250;
    DrawRectangleRec({px, py, panelW, panelH}, panelFill);
    DrawRectangleLinesEx({px, py, panelW, panelH}, 3.0F, ui::theme::PANEL_BORDER);

    const char *title = "Inventory";
    const float titleSize = 32.0F;
    const Vector2 td = MeasureTextEx(font, title, titleSize, 1.0F);
    DrawTextEx(font, title, {px + (panelW - td.x) * 0.5F, py + 16.0F}, titleSize, 1.0F,
               RAYWHITE);

    const float labelSize = 18.0F;
    DrawTextEx(font, "Equipped", {px + 24.0F, py + 56.0F}, labelSize, 1.0F,
               ui::theme::LABEL_TEXT);

    const float slotH = 56.0F;
    const float gap = 10.0F;
    const float y0 = py + 80.0F;
    const float consLabelY = y0 + 3.0F * (slotH + gap) + 4.0F;
    DrawTextEx(font, "Consumables", {px + 24.0F, consLabelY}, labelSize, 1.0F,
               ui::theme::LABEL_TEXT);
    DrawTextEx(font, "Carried", {px + 320.0F, py + 56.0F}, labelSize, 1.0F,
               ui::theme::LABEL_TEXT);

    const float small = 14.0F;
    for (int i = 0; i < static_cast<int>(EquipSlot::COUNT); ++i) {
        const Rectangle r = makeEquipSlotRect(screenW, screenH, i);
        DrawRectangleRec(r, ui::theme::SLOT_FILL);
        DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
        const char *ln = kSlotLabels[i];
        DrawTextEx(font, ln, {r.x + 6.0F, r.y + 6.0F}, small, 1.0F, ui::theme::MUTED_TEXT);
        const int idx = inv.equipped[static_cast<size_t>(i)];
        if (idx >= 0 && idx < static_cast<int>(inv.items.size())) {
            const bool ghost = dragging_ && dragSourceEquip_ == i;
            const Color c = ghost ? Fade(RAYWHITE, 0.35F) : RAYWHITE;
            const std::string lbl = itemLabel(inv.items[static_cast<size_t>(idx)]);
            DrawTextEx(font, lbl.c_str(), {r.x + 6.0F, r.y + 26.0F}, small, 1.0F, c);
        }
    }

    for (int i = 0; i < dreadcast::CONSUMABLE_SLOT_COUNT; ++i) {
        const Rectangle r = makeConsumableSlotRect(screenW, screenH, i);
        DrawRectangleRec(r, ui::theme::SLOT_FILL);
        DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
        const int idx = inv.consumableSlots[static_cast<size_t>(i)];
        if (idx >= 0 && idx < static_cast<int>(inv.items.size())) {
            const std::string lbl = itemLabel(inv.items[static_cast<size_t>(idx)]);
            DrawTextEx(font, lbl.c_str(), {r.x + 6.0F, r.y + 8.0F}, small, 1.0F, RAYWHITE);
        }
    }

    for (int i = 0; i < dreadcast::BAG_SLOT_COUNT; ++i) {
        const Rectangle r = makeBagSlotRect(screenW, screenH, i);
        DrawRectangleRec(r, ui::theme::SLOT_FILL);
        DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
        const int idx = inv.bagSlots[static_cast<size_t>(i)];
        if (idx >= 0 && idx < static_cast<int>(inv.items.size())) {
            const bool ghost = dragging_ && dragSourceBag_ == i;
            const Color c = ghost ? Fade(RAYWHITE, 0.35F) : RAYWHITE;
            const std::string lbl = itemLabel(inv.items[static_cast<size_t>(idx)]);
            DrawTextEx(font, lbl.c_str(), {r.x + 4.0F, r.y + 12.0F}, small, 1.0F, c);
        }
    }

    if (contextOpen_) {
        DrawRectangleRec(contextRect_, ui::theme::PANEL_FILL);
        DrawRectangleLinesEx(contextRect_, 2.0F, ui::theme::PANEL_BORDER);
        const char *o0 = contextIsCarried_ ? "Equip" : "Unequip";
        const char *o1 = "Drop";
        const float optFont = 18.0F;
        const Color h0 = CheckCollisionPointRec(GetMousePosition(), contextOpt0_)
                             ? ui::theme::BTN_HOVER
                             : ui::theme::SLOT_FILL;
        const Color h1 = CheckCollisionPointRec(GetMousePosition(), contextOpt1_)
                             ? ui::theme::BTN_HOVER
                             : ui::theme::SLOT_FILL;
        DrawRectangleRec(contextOpt0_, h0);
        DrawRectangleRec(contextOpt1_, h1);
        DrawTextEx(font, o0, {contextOpt0_.x + 8.0F, contextOpt0_.y + 6.0F}, optFont, 1.0F,
                   RAYWHITE);
        DrawTextEx(font, o1, {contextOpt1_.x + 8.0F, contextOpt1_.y + 6.0F}, optFont, 1.0F,
                   RAYWHITE);
    }

    if (dragging_ && dragItemIndex_ >= 0 &&
        dragItemIndex_ < static_cast<int>(inv.items.size())) {
        const std::string lbl = itemLabel(inv.items[static_cast<size_t>(dragItemIndex_)]);
        const float sz = 16.0F;
        const Vector2 m = GetMousePosition();
        const Vector2 dim = MeasureTextEx(font, lbl.c_str(), sz, 1.0F);
        const float pad = 6.0F;
        const Rectangle bg{m.x + 8.0F, m.y + 8.0F, dim.x + pad * 2.0F, dim.y + pad * 2.0F};
        DrawRectangleRec(bg, Fade(ui::theme::PANEL_FILL, 220));
        DrawRectangleLinesEx(bg, 1.5F, ui::theme::BTN_BORDER);
        DrawTextEx(font, lbl.c_str(), {bg.x + pad, bg.y + pad}, sz, 1.0F, RAYWHITE);
    }

    const Vector2 mouse = GetMousePosition();
    int tipIdx = -1;
    const int eqHit = hitTestEquip(screenW, screenH, mouse);
    if (eqHit >= 0) {
        tipIdx = inv.equipped[static_cast<size_t>(eqHit)];
    } else {
        const int bagHit = hitTestBag(screenW, screenH, mouse);
        if (bagHit >= 0) {
            tipIdx = inv.bagSlots[static_cast<size_t>(bagHit)];
        } else {
            const int consHit = hitTestConsumable(screenW, screenH, mouse);
            if (consHit >= 0) {
                tipIdx = inv.consumableSlots[static_cast<size_t>(consHit)];
            }
        }
    }
    if (tipIdx >= 0 && tipIdx < static_cast<int>(inv.items.size())) {
        const auto &it = inv.items[static_cast<size_t>(tipIdx)];
        if (!it.description.empty()) {
            const float tipSz = 14.0F;
            const std::string title = itemLabel(it);
            const Vector2 titleDim = MeasureTextEx(font, title.c_str(), tipSz + 2.0F, 1.0F);
            const Vector2 descDim = MeasureTextEx(font, it.description.c_str(), tipSz, 1.0F);
            const float pad = 6.0F;
            const float tw = std::max(titleDim.x, descDim.x) + pad * 2.0F;
            const float th = titleDim.y + descDim.y + pad * 3.0F;
            Rectangle tip{mouse.x + 14.0F, mouse.y + 14.0F, tw, th};
            clampRectToScreen(tip, screenW, screenH);
            DrawRectangleRec(tip, Fade(ui::theme::PANEL_FILL, 235));
            DrawRectangleLinesEx(tip, 1.5F, ui::theme::BTN_BORDER);
            DrawTextEx(font, title.c_str(), {tip.x + pad, tip.y + pad}, tipSz + 2.0F, 1.0F,
                       RAYWHITE);
            DrawTextEx(font, it.description.c_str(),
                       {tip.x + pad, tip.y + pad + titleDim.y + 6.0F}, tipSz, 1.0F,
                       ui::theme::LABEL_TEXT);
        }
    }
}

} // namespace dreadcast::ui
