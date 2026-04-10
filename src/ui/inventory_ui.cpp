#include "ui/inventory_ui.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <string>

#include "config.hpp"
#include "game/item_rarity.hpp"
#include "core/resource_manager.hpp"
#include "ui/theme.hpp"

namespace dreadcast::ui {

namespace {

// Slot dimensions (width:height = 7:5). Equip largest, consumable medium, bag smallest.
constexpr float kInvPanelW = 760.0F;
constexpr float kInvPanelH = 480.0F;
constexpr float kEquipSlotW = 120.0F;
constexpr float kEquipSlotH = 86.0F;
constexpr float kConsumableSlotW = 108.0F;
constexpr float kConsumableSlotH = 77.0F;
constexpr float kBagSlotW = 98.0F;
constexpr float kBagSlotH = 70.0F;
constexpr float kSlotGap = 10.0F;
/// Width used to center armor (row 1) and the amulet+ring pair (row 2).
constexpr float kEquipColumnInnerW = 280.0F;

Rectangle makeEquipSlotRect(int screenW, int screenH, int equipIndex) {
    (void)screenW;
    const float panelW = kInvPanelW;
    const float panelH = kInvPanelH;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    const float colLeft = px + 24.0F;
    const float y0 = py + 80.0F;
    const float row1Y = y0 + kEquipSlotH + kSlotGap;
    const float pairW = kEquipSlotW * 2.0F + kSlotGap;

    if (equipIndex == 0) {
        // Armor — centered on row 1.
        const float x = colLeft + (kEquipColumnInnerW - kEquipSlotW) * 0.5F;
        return {x, y0, kEquipSlotW, kEquipSlotH};
    }
    if (equipIndex == 1) {
        // Amulet — left slot on row 2.
        const float x = colLeft + (kEquipColumnInnerW - pairW) * 0.5F;
        return {x, row1Y, kEquipSlotW, kEquipSlotH};
    }
    if (equipIndex == 2) {
        // Ring — right slot on row 2.
        const float x0 = colLeft + (kEquipColumnInnerW - pairW) * 0.5F;
        return {x0 + kEquipSlotW + kSlotGap, row1Y, kEquipSlotW, kEquipSlotH};
    }
    return {0.0F, 0.0F, 0.0F, 0.0F};
}

Rectangle makeConsumableSlotRect(int screenW, int screenH, int consumableIndex) {
    const float panelW = kInvPanelW;
    const float panelH = kInvPanelH;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    const float y0 = py + 80.0F;
    const float equipBlockH = 2.0F * kEquipSlotH + kSlotGap;
    const float afterEquip = y0 + equipBlockH + 24.0F;
    const float x = px + 24.0F + static_cast<float>(consumableIndex) *
                                      (kConsumableSlotW + kSlotGap);
    return {x, afterEquip, kConsumableSlotW, kConsumableSlotH};
}

Rectangle makeBagSlotRect(int screenW, int screenH, int bagIndex) {
    const float panelW = kInvPanelW;
    const float panelH = kInvPanelH;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    const float col0 = px + 340.0F;
    const int row = bagIndex / 3;
    const int col = bagIndex % 3;
    return {col0 + static_cast<float>(col) * (kBagSlotW + kSlotGap),
            py + 100.0F + static_cast<float>(row) * (kBagSlotH + kSlotGap), kBagSlotW,
            kBagSlotH};
}

Rectangle makeInventoryPanelRect(int screenW, int screenH) {
    const float panelW = kInvPanelW;
    const float panelH = kInvPanelH;
    const float px = (static_cast<float>(screenW) - panelW) * 0.5F;
    const float py = (static_cast<float>(screenH) - panelH) * 0.5F;
    return {px, py, panelW, panelH};
}

Rectangle makeRarityPanelRect(int screenW, int screenH) {
    const Rectangle inv = makeInventoryPanelRect(screenW, screenH);
    const float w = 560.0F;
    const float h = 500.0F;
    return {inv.x + (inv.width - w) * 0.5F, inv.y + (inv.height - h) * 0.5F, w, h};
}

Vector2 infoIconCenter(int screenW, int screenH) {
    const Rectangle inv = makeInventoryPanelRect(screenW, screenH);
    return {inv.x + inv.width - 30.0F, inv.y + 28.0F};
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
    if (it.isStackable && it.stackCount >= 1) {
        s += " x";
        s += std::to_string(it.stackCount);
    }
    return s;
}

float measureMultilineHeight(const Font &font, const std::string &text, float fontSize) {
    if (text.empty()) {
        return 0.0F;
    }
    std::istringstream iss(text);
    std::string line;
    float h = 0.0F;
    while (std::getline(iss, line)) {
        const Vector2 d = MeasureTextEx(font, line.c_str(), fontSize, 1.0F);
        h += d.y + 4.0F;
    }
    return std::max(0.0F, h - 4.0F);
}

float maxLineWidthMultiline(const Font &font, const std::string &text, float fontSize) {
    float w = 0.0F;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        const Vector2 d = MeasureTextEx(font, line.c_str(), fontSize, 1.0F);
        w = std::max(w, d.x);
    }
    return w;
}

void drawMultilineText(const Font &font, const std::string &text, float x, float y, float fontSize,
                       Color col) {
    std::istringstream iss(text);
    std::string line;
    float yy = y;
    while (std::getline(iss, line)) {
        DrawTextEx(font, line.c_str(), {x, yy}, fontSize, 1.0F, col);
        const Vector2 d = MeasureTextEx(font, line.c_str(), fontSize, 1.0F);
        yy += d.y + 4.0F;
    }
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

/// Base slot fill + rarity tint (margin around centered icon reads as colored “padding”).
void drawItemSlotSurface(Rectangle r, const dreadcast::ItemData *it, bool ghost) {
    DrawRectangleRec(r, ui::theme::SLOT_FILL);
    if (it != nullptr) {
        const Color rc = dreadcast::rarityColor(it->rarity);
        DrawRectangleRec(r, Fade(rc, ghost ? 0.12F : 0.28F));
    }
}

void drawCordialManicBlockedOverlay(Rectangle slotRect) {
    const float ix = slotRect.x + (slotRect.width - ITEM_ICON_DRAW_W) * 0.5F;
    const float iy = slotRect.y + (slotRect.height - ITEM_ICON_DRAW_H) * 0.5F;
    const Vector2 a{ix + 6.0F, iy + 6.0F};
    const Vector2 b{ix + ITEM_ICON_DRAW_W - 6.0F, iy + ITEM_ICON_DRAW_H - 6.0F};
    DrawLineEx(a, b, 4.0F, Fade(RED, 220));
    DrawLineEx({a.x, b.y}, {b.x, a.y}, 4.0F, Fade(RED, 220));
}

} // namespace

void InventoryUI::drawItemIcon(const dreadcast::ItemData &it, dreadcast::ResourceManager &resources,
                                 Rectangle slotRect, Color tint) {
    if (it.iconPath.empty()) {
        return;
    }
    const Texture2D tex = resources.getTexture(it.iconPath);
    if (tex.id == 0 || tex.width <= 0 || tex.height <= 0) {
        return;
    }
    const float ix = slotRect.x + (slotRect.width - ITEM_ICON_DRAW_W) * 0.5F;
    const float iy = slotRect.y + (slotRect.height - ITEM_ICON_DRAW_H) * 0.5F;
    const Rectangle dst{ix, iy, ITEM_ICON_DRAW_W, ITEM_ICON_DRAW_H};
    const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, tint);
}

void InventoryUI::tryEquipFromBag(InventoryState &inv, int bagIndex) {
    if (bagIndex < 0 || bagIndex >= dreadcast::BAG_SLOT_COUNT) {
        return;
    }
    const int itemIdx = inv.bagSlots[static_cast<size_t>(bagIndex)];
    if (itemIdx < 0 || itemIdx >= static_cast<int>(inv.items.size())) {
        return;
    }
    // Consumables are not equippable into gear slots (Armor/Amulet/Ring).
    if (inv.items[static_cast<size_t>(itemIdx)].isConsumable) {
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

void InventoryUI::tryEquipConsumableFromBag(InventoryState &inv, int bagIndex) {
    if (bagIndex < 0 || bagIndex >= dreadcast::BAG_SLOT_COUNT) {
        return;
    }
    const int itemIdx = inv.bagSlots[static_cast<size_t>(bagIndex)];
    if (itemIdx < 0 || itemIdx >= static_cast<int>(inv.items.size())) {
        return;
    }
    if (!inv.items[static_cast<size_t>(itemIdx)].isConsumable) {
        return;
    }
    const int empty = inv.firstEmptyConsumableSlot();
    if (empty >= 0) {
        inv.consumableSlots[static_cast<size_t>(empty)] = itemIdx;
        inv.bagSlots[static_cast<size_t>(bagIndex)] = -1;
    }
}

void InventoryUI::tryUnequipConsumableToBag(InventoryState &inv, int consumableSlotIndex) {
    if (consumableSlotIndex < 0 || consumableSlotIndex >= dreadcast::CONSUMABLE_SLOT_COUNT) {
        return;
    }
    const int itemIdx = inv.consumableSlots[static_cast<size_t>(consumableSlotIndex)];
    if (itemIdx < 0 || itemIdx >= static_cast<int>(inv.items.size())) {
        return;
    }
    const int bag = inv.firstEmptyBagSlot();
    if (bag < 0) {
        return;
    }
    inv.bagSlots[static_cast<size_t>(bag)] = itemIdx;
    inv.consumableSlots[static_cast<size_t>(consumableSlotIndex)] = -1;
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
        dragSourceConsumable_ = -1;
        contextOpen_ = false;
        rarityInfoOpen_ = false;
        return action;
    }

    const int sw = config::WINDOW_WIDTH;
    const int sh = config::WINDOW_HEIGHT;
    const Vector2 mouse = input.mousePosition();
    const bool leftPress = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool leftRelease = input.isMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const bool rightPress = input.isMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    const Rectangle invPanel = makeInventoryPanelRect(sw, sh);
    const Rectangle rarityPanel = makeRarityPanelRect(sw, sh);
    const Vector2 infoCenter = infoIconCenter(sw, sh);
    const float infoR = 16.0F;

    if (leftPress && CheckCollisionPointCircle(mouse, infoCenter, infoR)) {
        rarityInfoOpen_ = !rarityInfoOpen_;
        contextOpen_ = false;
        dragging_ = false;
        return action;
    }

    if (rarityInfoOpen_) {
        if (input.isKeyPressed(KEY_ESCAPE)) {
            rarityInfoOpen_ = false;
            return action;
        }
        if (leftPress) {
            if (!CheckCollisionPointRec(mouse, rarityPanel) && CheckCollisionPointRec(mouse, invPanel)) {
                rarityInfoOpen_ = false;
            }
            return action;
        }
    }

    if (input.isKeyPressed(KEY_ESCAPE) && contextOpen_) {
        contextOpen_ = false;
        return action;
    }

    if (contextOpen_) {
        const bool itemValid = contextItemIndex_ >= 0 &&
                               contextItemIndex_ < static_cast<int>(inv.items.size());
        const bool itemIsConsumable =
            itemValid && inv.items[static_cast<size_t>(contextItemIndex_)].isConsumable;
        const bool fromBag = contextBagSlot_ >= 0;
        const bool fromConsumableSlot = contextConsumableSlot_ >= 0;
        const bool fromGear = contextEquipSlot_ >= 0;

        if (leftPress) {
            // Option 0
            if (CheckCollisionPointRec(mouse, contextOpt0_)) {
                if (itemIsConsumable) {
                    action.type = InventoryAction::Use;
                    action.itemIndex = contextItemIndex_;
                    action.useBagSlot = fromBag ? contextBagSlot_ : -1;
                    action.useConsumableSlot = fromConsumableSlot ? contextConsumableSlot_ : -1;
                    contextOpen_ = false;
                    return action;
                }

                // Non-consumable: Equip (bag) / Unequip (gear)
                if (fromBag) {
                    tryEquipFromBag(inv, contextBagSlot_);
                } else if (fromGear) {
                    tryUnequip(inv, static_cast<EquipSlot>(contextEquipSlot_));
                }
                contextOpen_ = false;
                return action;
            }

            // Option 1
            if (CheckCollisionPointRec(mouse, contextOpt1_)) {
                if (itemIsConsumable) {
                    if (fromBag) {
                        // Equip to first empty consumable slot.
                        const int toCons = inv.firstEmptyConsumableSlot();
                        if (toCons >= 0) {
                            inv.consumableSlots[static_cast<size_t>(toCons)] = contextItemIndex_;
                            inv.bagSlots[static_cast<size_t>(contextBagSlot_)] = -1;
                        }
                    } else if (fromConsumableSlot) {
                        // Unequip to first empty bag slot.
                        const int toBag = inv.firstEmptyBagSlot();
                        if (toBag >= 0) {
                            inv.bagSlots[static_cast<size_t>(toBag)] = contextItemIndex_;
                            inv.consumableSlots[static_cast<size_t>(contextConsumableSlot_)] = -1;
                        }
                    }
                    contextOpen_ = false;
                    return action;
                }

                // Non-consumable: Drop.
                action.type = InventoryAction::Drop;
                action.itemIndex = contextItemIndex_;
                if (fromBag) {
                    action.dropFromEquipped = false;
                    action.bagSlot = contextBagSlot_;
                    inv.bagSlots[static_cast<size_t>(contextBagSlot_)] = -1;
                } else if (fromGear) {
                    action.dropFromEquipped = true;
                    action.equipSlot = static_cast<EquipSlot>(contextEquipSlot_);
                    inv.equipped[static_cast<size_t>(contextEquipSlot_)] = -1;
                }
                contextOpen_ = false;
                return action;
            }

            // Option 2 (consumables only)
            if (itemIsConsumable && CheckCollisionPointRec(mouse, contextOpt2_)) {
                action.type = InventoryAction::Drop;
                action.itemIndex = contextItemIndex_;
                if (fromBag) {
                    action.dropFromEquipped = false;
                    action.bagSlot = contextBagSlot_;
                    inv.bagSlots[static_cast<size_t>(contextBagSlot_)] = -1;
                } else if (fromConsumableSlot) {
                    action.dropFromEquipped = false;
                    action.bagSlot = -1;
                    inv.consumableSlots[static_cast<size_t>(contextConsumableSlot_)] = -1;
                } else if (fromGear) {
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
                const bool isConsumable = idx >= 0 &&
                                           idx < static_cast<int>(inv.items.size()) &&
                                           inv.items[static_cast<size_t>(idx)].isConsumable;
                const float menuH = rowH * (isConsumable ? 3.0F : 2.0F) + 10.0F;
                contextRect_ = {mouse.x, mouse.y, menuW, menuH};
                clampRectToScreen(contextRect_, sw, sh);
                contextOpt0_ = {contextRect_.x + 4.0F, contextRect_.y + 4.0F,
                                contextRect_.width - 8.0F, rowH};
                contextOpt1_ = {contextRect_.x + 4.0F, contextRect_.y + 5.0F + rowH,
                                contextRect_.width - 8.0F, rowH};
                contextOpt2_ = {0, 0, 0, 0};
                contextItemIndex_ = idx;
                contextBagSlot_ = bagHit;
                contextEquipSlot_ = -1;
                contextConsumableSlot_ = -1;
                contextIsCarried_ = true;
                contextOpt0IsEquip_ = !isConsumable;
                if (isConsumable) {
                    contextOpt2_ = {contextRect_.x + 4.0F, contextRect_.y + 5.0F + rowH * 2.0F,
                                    contextRect_.width - 8.0F, rowH};
                }
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
                contextOpt2_ = {0, 0, 0, 0};
                contextItemIndex_ = idx;
                contextBagSlot_ = -1;
                contextEquipSlot_ = eqHit;
                contextConsumableSlot_ = -1;
                contextIsCarried_ = false;
                contextOpt0IsEquip_ = false;
                contextOpen_ = true;
            }
        }

        const int consHit = hitTestConsumable(sw, sh, mouse);
        if (consHit >= 0) {
            const int idx = inv.consumableSlots[static_cast<size_t>(consHit)];
            if (idx >= 0) {
                const float rowH = 32.0F;
                const float menuW = 168.0F;
                const float menuH = rowH * 3.0F + 10.0F;
                contextRect_ = {mouse.x, mouse.y, menuW, menuH};
                clampRectToScreen(contextRect_, sw, sh);
                contextOpt0_ = {contextRect_.x + 4.0F, contextRect_.y + 4.0F,
                                contextRect_.width - 8.0F, rowH};
                contextOpt1_ = {contextRect_.x + 4.0F, contextRect_.y + 5.0F + rowH,
                                contextRect_.width - 8.0F, rowH};
                contextOpt2_ = {contextRect_.x + 4.0F, contextRect_.y + 5.0F + rowH * 2.0F,
                                contextRect_.width - 8.0F, rowH};
                contextItemIndex_ = idx;
                contextBagSlot_ = -1;
                contextEquipSlot_ = -1;
                contextConsumableSlot_ = consHit;
                contextIsCarried_ = false;
                contextOpt0IsEquip_ = false;
                contextOpen_ = true;
            }
        }
        return action;
    }

    if (leftPress && !dragging_) {
        const bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (shiftHeld) {
            const int eqShift = hitTestEquip(sw, sh, mouse);
            if (eqShift >= 0) {
                const int eqIdx = inv.equipped[static_cast<size_t>(eqShift)];
                if (eqIdx >= 0) {
                    tryUnequip(inv, static_cast<EquipSlot>(eqShift));
                }
                return action;
            }
            const int consShift = hitTestConsumable(sw, sh, mouse);
            if (consShift >= 0) {
                const int cIdx = inv.consumableSlots[static_cast<size_t>(consShift)];
                if (cIdx >= 0) {
                    tryUnequipConsumableToBag(inv, consShift);
                }
                return action;
            }
            const int b = hitTestBag(sw, sh, mouse);
            if (b >= 0) {
                const int idx = inv.bagSlots[static_cast<size_t>(b)];
                if (idx >= 0 && idx < static_cast<int>(inv.items.size())) {
                    if (inv.items[static_cast<size_t>(idx)].isConsumable) {
                        tryEquipConsumableFromBag(inv, b);
                    } else {
                        tryEquipFromBag(inv, b);
                    }
                }
            }
            return action;
        }
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
                    dragSourceConsumable_ = -1;
                }
            } else {
                const int c = hitTestConsumable(sw, sh, mouse);
                if (c >= 0) {
                    const int idx = inv.consumableSlots[static_cast<size_t>(c)];
                    if (idx >= 0) {
                        dragging_ = true;
                        dragItemIndex_ = idx;
                        dragSourceBag_ = -1;
                        dragSourceEquip_ = -1;
                        dragSourceConsumable_ = c;
                    }
                }
            }
        }
        return action;
    }

    if (leftRelease && dragging_) {
        dragging_ = false;
        const int bagHit = hitTestBag(sw, sh, mouse);
        const int eqHit = hitTestEquip(sw, sh, mouse);
        const int consHit = hitTestConsumable(sw, sh, mouse);

        const bool outsidePanel = !CheckCollisionPointRec(mouse, invPanel);

        if (dragSourceBag_ >= 0) {
            if (eqHit >= 0) {
                const auto targetEq = static_cast<EquipSlot>(eqHit);
                if (inv.items[static_cast<size_t>(dragItemIndex_)].slot == targetEq) {
                    tryEquipFromBag(inv, dragSourceBag_);
                }
            } else if (bagHit >= 0 && bagHit != dragSourceBag_) {
                swapBagSlots(inv, dragSourceBag_, bagHit);
            } else if (consHit >= 0 &&
                       inv.items[static_cast<size_t>(dragItemIndex_)].isConsumable) {
                const int prev = inv.consumableSlots[static_cast<size_t>(consHit)];
                inv.consumableSlots[static_cast<size_t>(consHit)] = dragItemIndex_;
                inv.bagSlots[static_cast<size_t>(dragSourceBag_)] = prev;
            } else if (outsidePanel) {
                action.type = InventoryAction::Drop;
                action.itemIndex = dragItemIndex_;
                action.dropFromEquipped = false;
                action.bagSlot = dragSourceBag_;
                inv.bagSlots[static_cast<size_t>(dragSourceBag_)] = -1;
            }
        } else if (dragSourceEquip_ >= 0) {
            if (bagHit >= 0) {
                moveEquippedToBagSlot(inv, static_cast<EquipSlot>(dragSourceEquip_), bagHit);
            } else if (outsidePanel) {
                action.type = InventoryAction::Drop;
                action.itemIndex = dragItemIndex_;
                action.dropFromEquipped = true;
                action.equipSlot = static_cast<EquipSlot>(dragSourceEquip_);
                inv.equipped[static_cast<size_t>(dragSourceEquip_)] = -1;
            }
        } else if (dragSourceConsumable_ >= 0) {
            if (consHit >= 0 && consHit != dragSourceConsumable_) {
                std::swap(inv.consumableSlots[static_cast<size_t>(dragSourceConsumable_)],
                          inv.consumableSlots[static_cast<size_t>(consHit)]);
            } else if (bagHit >= 0) {
                const int bIdx = inv.bagSlots[static_cast<size_t>(bagHit)];
                if (bIdx < 0) {
                    inv.bagSlots[static_cast<size_t>(bagHit)] = dragItemIndex_;
                    inv.consumableSlots[static_cast<size_t>(dragSourceConsumable_)] = -1;
                } else if (inv.items[static_cast<size_t>(bIdx)].isConsumable) {
                    inv.bagSlots[static_cast<size_t>(bagHit)] = dragItemIndex_;
                    inv.consumableSlots[static_cast<size_t>(dragSourceConsumable_)] = bIdx;
                }
            } else if (outsidePanel) {
                action.type = InventoryAction::Drop;
                action.itemIndex = dragItemIndex_;
                action.dropFromEquipped = false;
                action.bagSlot = -1;
                inv.consumableSlots[static_cast<size_t>(dragSourceConsumable_)] = -1;
            }
        }

        dragSourceBag_ = -1;
        dragSourceEquip_ = -1;
        dragSourceConsumable_ = -1;
        dragItemIndex_ = -1;

        // Keep dragging-out drop behavior mutually exclusive with slot placement.
        if (action.type == InventoryAction::Drop && outsidePanel &&
            bagHit < 0 && eqHit < 0 && consHit < 0) {
            return action;
        }
    }

    return action;
}

void InventoryUI::draw(const Font &font, dreadcast::ResourceManager &resources, int screenW, int screenH,
                       const InventoryState &inv, float playerHpRatio,
                       float runicShellCdRatio, float runicShellCdSeconds) {
    if (!open_) {
        return;
    }

    const Rectangle panel = makeInventoryPanelRect(screenW, screenH);
    const float panelW = panel.width;
    const float panelH = panel.height;
    const float px = panel.x;
    const float py = panel.y;
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

    const float y0 = py + 80.0F;
    const float equipBlockH = 2.0F * kEquipSlotH + kSlotGap;
    const float consLabelY = y0 + equipBlockH + 4.0F;
    DrawTextEx(font, "Consumables", {px + 24.0F, consLabelY}, labelSize, 1.0F,
               ui::theme::LABEL_TEXT);
    DrawTextEx(font, "Carried", {px + 340.0F, py + 56.0F}, labelSize, 1.0F,
               ui::theme::LABEL_TEXT);

    static const char *kSlotIconPaths[] = {
        "assets/textures/ui/armor_slot.png",
        "assets/textures/ui/amulet_slot.png",
        "assets/textures/ui/ring_slot.png",
    };
    for (int i = 0; i < static_cast<int>(EquipSlot::COUNT); ++i) {
        const Rectangle r = makeEquipSlotRect(screenW, screenH, i);
        const int idx = inv.equipped[static_cast<size_t>(i)];
        const dreadcast::ItemData *pit =
            (idx >= 0 && idx < static_cast<int>(inv.items.size()))
                ? &inv.items[static_cast<size_t>(idx)]
                : nullptr;
        const bool ghost = dragging_ && dragSourceEquip_ == i;
        drawItemSlotSurface(r, pit, ghost);
        DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
        if (pit == nullptr) {
            const Texture2D slotTex = resources.getTexture(kSlotIconPaths[i]);
            if (slotTex.id != 0 && slotTex.width > 0 && slotTex.height > 0) {
                constexpr float iconSz = 40.0F;
                const float ix = r.x + (r.width - iconSz) * 0.5F;
                const float iy = r.y + (r.height - iconSz) * 0.5F;
                const Rectangle src{0.0F, 0.0F, static_cast<float>(slotTex.width),
                                    static_cast<float>(slotTex.height)};
                const Rectangle dst{ix, iy, iconSz, iconSz};
                DrawTexturePro(slotTex, src, dst, {0.0F, 0.0F}, 0.0F,
                               Fade(ui::theme::MUTED_TEXT, 0.55F));
            }
        } else {
            const Color tint = ghost ? Fade(RAYWHITE, 0.35F) : RAYWHITE;
            InventoryUI::drawItemIcon(*pit, resources, r, tint);
            if (i == 0 && runicShellCdRatio > 0.001F && pit->name == "Runic Shell") {
                DrawRectangleRec(r, Fade(BLACK, 0.45F * runicShellCdRatio));
                char cdBuf[16];
                const int sec = static_cast<int>(std::ceil(static_cast<double>(runicShellCdSeconds)));
                std::snprintf(cdBuf, sizeof(cdBuf), "%ds", sec);
                const float cdFs = 18.0F;
                const Vector2 cdDim = MeasureTextEx(font, cdBuf, cdFs, 1.0F);
                DrawTextEx(font, cdBuf,
                           {r.x + (r.width - cdDim.x) * 0.5F + 1.0F,
                            r.y + (r.height - cdDim.y) * 0.5F + 1.0F},
                           cdFs, 1.0F, Fade(BLACK, 180));
                DrawTextEx(font, cdBuf,
                           {r.x + (r.width - cdDim.x) * 0.5F,
                            r.y + (r.height - cdDim.y) * 0.5F},
                           cdFs, 1.0F, {200, 220, 255, 255});
            }
        }
    }

    for (int i = 0; i < dreadcast::CONSUMABLE_SLOT_COUNT; ++i) {
        const Rectangle r = makeConsumableSlotRect(screenW, screenH, i);
        const int idx = inv.consumableSlots[static_cast<size_t>(i)];
        const dreadcast::ItemData *pit =
            (idx >= 0 && idx < static_cast<int>(inv.items.size()))
                ? &inv.items[static_cast<size_t>(idx)]
                : nullptr;
        const bool ghost = dragging_ && dragSourceConsumable_ == i;
        drawItemSlotSurface(r, pit, ghost);
        DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
        if (pit != nullptr) {
            const Color tint = ghost ? Fade(RAYWHITE, 0.35F) : RAYWHITE;
            const auto &it = *pit;
            InventoryUI::drawItemIcon(it, resources, r, tint);
            if (it.name == "Vial of Cordial Manic" && playerHpRatio < 0.40F - 1.0e-4F) {
                drawCordialManicBlockedOverlay(r);
            }
            if (it.isStackable && it.stackCount >= 1) {
                char cntBuf[16];
                std::snprintf(cntBuf, sizeof(cntBuf), "%d", it.stackCount);
                const float stackFs = 13.0F;
                const Vector2 cd = MeasureTextEx(font, cntBuf, stackFs, 1.0F);
                const float sp = 4.0F;
                const float tx = r.x + r.width - cd.x - sp;
                const float ty = r.y + r.height - cd.y - sp;
                DrawTextEx(font, cntBuf, {tx + 1.0F, ty + 1.0F}, stackFs, 1.0F,
                           Fade(BLACK, 200));
                DrawTextEx(font, cntBuf, {tx, ty}, stackFs, 1.0F, RAYWHITE);
            }
        }
    }

    for (int i = 0; i < dreadcast::BAG_SLOT_COUNT; ++i) {
        const Rectangle r = makeBagSlotRect(screenW, screenH, i);
        const int idx = inv.bagSlots[static_cast<size_t>(i)];
        const dreadcast::ItemData *pit =
            (idx >= 0 && idx < static_cast<int>(inv.items.size()))
                ? &inv.items[static_cast<size_t>(idx)]
                : nullptr;
        const bool ghost = dragging_ && dragSourceBag_ == i;
        drawItemSlotSurface(r, pit, ghost);
        DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
        if (pit != nullptr) {
            const Color tint = ghost ? Fade(RAYWHITE, 0.35F) : RAYWHITE;
            const auto &it = *pit;
            InventoryUI::drawItemIcon(it, resources, r, tint);
            if (it.name == "Vial of Cordial Manic" && playerHpRatio < 0.40F - 1.0e-4F) {
                drawCordialManicBlockedOverlay(r);
            }
            if (it.isStackable && it.stackCount >= 1) {
                char cntBuf[16];
                std::snprintf(cntBuf, sizeof(cntBuf), "%d", it.stackCount);
                const float stackFs = 13.0F;
                const Vector2 cd = MeasureTextEx(font, cntBuf, stackFs, 1.0F);
                const float sp = 4.0F;
                const float tx = r.x + r.width - cd.x - sp;
                const float ty = r.y + r.height - cd.y - sp;
                DrawTextEx(font, cntBuf, {tx + 1.0F, ty + 1.0F}, stackFs, 1.0F,
                           Fade(BLACK, 200));
                DrawTextEx(font, cntBuf, {tx, ty}, stackFs, 1.0F, RAYWHITE);
            }
        }
    }

    if (contextOpen_) {
        DrawRectangleRec(contextRect_, ui::theme::PANEL_FILL);
        DrawRectangleLinesEx(contextRect_, 2.0F, ui::theme::PANEL_BORDER);
        const bool itemValid = contextItemIndex_ >= 0 &&
                               contextItemIndex_ < static_cast<int>(inv.items.size());
        const bool itemIsConsumable =
            itemValid && inv.items[static_cast<size_t>(contextItemIndex_)].isConsumable;
        const bool fromBag = contextBagSlot_ >= 0;

        const char *o0 = itemIsConsumable ? "Use" : (contextIsCarried_ ? "Equip" : "Unequip");
        const char *o1 = itemIsConsumable ? (fromBag ? "Equip" : "Unequip") : "Drop";
        const char *o2 = "Drop";

        const Vector2 m = GetMousePosition();
        const float optFont = 18.0F;

        const Color h0 = CheckCollisionPointRec(m, contextOpt0_) ? ui::theme::BTN_HOVER
                                                                 : ui::theme::SLOT_FILL;
        const Color h1 = CheckCollisionPointRec(m, contextOpt1_) ? ui::theme::BTN_HOVER
                                                                 : ui::theme::SLOT_FILL;
        const Color h2 = CheckCollisionPointRec(m, contextOpt2_) ? ui::theme::BTN_HOVER
                                                                 : ui::theme::SLOT_FILL;

        DrawRectangleRec(contextOpt0_, h0);
        DrawRectangleRec(contextOpt1_, h1);
        DrawTextEx(font, o0, {contextOpt0_.x + 8.0F, contextOpt0_.y + 6.0F}, optFont, 1.0F,
                   RAYWHITE);
        DrawTextEx(font, o1, {contextOpt1_.x + 8.0F, contextOpt1_.y + 6.0F}, optFont, 1.0F,
                   RAYWHITE);

        if (itemIsConsumable) {
            DrawRectangleRec(contextOpt2_, h2);
            DrawTextEx(font, o2, {contextOpt2_.x + 8.0F, contextOpt2_.y + 6.0F}, optFont, 1.0F,
                       RAYWHITE);
        }
    }

    if (dragging_ && dragItemIndex_ >= 0 &&
        dragItemIndex_ < static_cast<int>(inv.items.size())) {
        const auto &dit = inv.items[static_cast<size_t>(dragItemIndex_)];
        const Vector2 m = GetMousePosition();
        const float ix = m.x - ITEM_ICON_DRAW_W * 0.5F;
        const float iy = m.y - ITEM_ICON_DRAW_H * 0.5F;
        if (!dit.iconPath.empty()) {
            const Texture2D tex = resources.getTexture(dit.iconPath);
            if (tex.id != 0 && tex.width > 0 && tex.height > 0) {
                const Rectangle dst{ix, iy, ITEM_ICON_DRAW_W, ITEM_ICON_DRAW_H};
                const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width),
                                    static_cast<float>(tex.height)};
                DrawRectangleRec({ix + 3.0F, iy + 5.0F, ITEM_ICON_DRAW_W, ITEM_ICON_DRAW_H},
                                 Fade(BLACK, 55));
                DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, RAYWHITE);
                DrawRectangleLinesEx(
                    {ix - 2.0F, iy - 2.0F, ITEM_ICON_DRAW_W + 4.0F, ITEM_ICON_DRAW_H + 4.0F}, 2.0F,
                    ui::theme::SLOT_BORDER);
            }
        }
    }

    const Vector2 mouse = GetMousePosition();

    const Vector2 infoCenter = infoIconCenter(screenW, screenH);
    const float infoR = 14.0F;
    const bool infoHover = CheckCollisionPointCircle(mouse, infoCenter, infoR + 2.0F);
    DrawCircleV(infoCenter, infoR, Fade(ui::theme::SLOT_FILL, infoHover ? 220 : 160));
    DrawCircleV(infoCenter, infoR - 2.5F, Fade(BLACK, infoHover ? 35 : 20));
    DrawCircleLines(static_cast<int>(infoCenter.x), static_cast<int>(infoCenter.y),
                    static_cast<int>(infoR),
                    infoHover ? ui::theme::BTN_BORDER : ui::theme::SLOT_BORDER);
    DrawCircleLines(static_cast<int>(infoCenter.x), static_cast<int>(infoCenter.y),
                    static_cast<int>(infoR - 1), Fade(WHITE, infoHover ? 90 : 45));
    const char *infoGlyph = "i";
    const float infoFs = 22.0F;
    const Vector2 infoGd = MeasureTextEx(font, infoGlyph, infoFs, 1.0F);
    DrawTextEx(font, infoGlyph,
               {infoCenter.x - infoGd.x * 0.5F, infoCenter.y - infoGd.y * 0.5F - 1.0F}, infoFs,
               1.0F, infoHover ? RAYWHITE : ui::theme::LABEL_TEXT);

    if (rarityInfoOpen_) {
        const Rectangle rp = makeRarityPanelRect(screenW, screenH);
        DrawRectangleRec(rp, Fade(ui::theme::PANEL_FILL, 248));
        DrawRectangleLinesEx(rp, 2.0F, ui::theme::PANEL_BORDER);
        DrawTextEx(font, "Item Rarity", {rp.x + 16.0F, rp.y + 12.0F}, 28.0F, 1.0F, RAYWHITE);

        float y = rp.y + 48.0F;
        const float fs = 14.0F;
        const float lh = 20.0F;
        DrawTextEx(font, "Gear (armor, rings, amulets)", {rp.x + 16.0F, y}, fs + 2.0F, 1.0F,
                   ui::theme::SUBTITLE_TEXT);
        y += lh + 2.0F;
        auto lineRarityInline = [&](const char *namePart, const char *descPart, Color nameCol) {
            const float x0 = rp.x + 20.0F;
            DrawTextEx(font, namePart, {x0, y}, fs, 1.0F, nameCol);
            const Vector2 nameDim = MeasureTextEx(font, namePart, fs, 1.0F);
            const char *dash = " - ";
            DrawTextEx(font, dash, {x0 + nameDim.x, y}, fs, 1.0F, ui::theme::LABEL_TEXT);
            const Vector2 dashDim = MeasureTextEx(font, dash, fs, 1.0F);
            DrawTextEx(font, descPart, {x0 + nameDim.x + dashDim.x, y}, fs, 1.0F,
                       ui::theme::LABEL_TEXT);
            y += lh * 1.35F;
        };
        lineRarityInline("Tarnished", "Rusted, discarded by the masses.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Tarnished));
        lineRarityInline("Blighted", "Touched by Hell's decay.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Blighted));
        lineRarityInline("Cursed", "Possesses a malevolent will.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Cursed));
        lineRarityInline("Dread", "Tied to legendary massacres.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Dread));
        lineRarityInline("Abyssal", "Forged from the pit's core; reality-bending.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Abyssal));

        y += 4.0F;
        DrawTextEx(font, "Vials (consumables)", {rp.x + 16.0F, y}, fs + 2.0F, 1.0F,
                   ui::theme::SUBTITLE_TEXT);
        y += lh + 2.0F;
        lineRarityInline("Clouded (stack 5)", "Murky sediment; basic healing.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Clouded));
        lineRarityInline("Lucid (stack 3)", "Filtered through a tormented soul.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Lucid));
        lineRarityInline("Absolute (stack 1)", "Pure, undiluted essence.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Absolute));
        lineRarityInline("Special (stack 1)", "Odd effects and strange rules.",
                         dreadcast::rarityColor(dreadcast::ItemRarity::Special));
    }

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
        const bool altHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        const float tipSz = 14.0F;
        const std::string title = itemLabel(it);
        const std::string rline = dreadcast::rarityLine(it);
        const Color rcol = dreadcast::rarityColor(it.rarity);

        std::string ext;
        if (altHeld) {
            char hpBuf[64];
            std::snprintf(hpBuf, sizeof(hpBuf), "Slot: %s",
                          it.isConsumable
                              ? "Consumable"
                              : (it.slot == EquipSlot::Armor
                                     ? "Armor"
                                     : (it.slot == EquipSlot::Amulet ? "Amulet" : "Ring")));
            ext += hpBuf;
            if (it.maxHpBonus > 0.001F) {
                char bonusBuf[64];
                std::snprintf(bonusBuf, sizeof(bonusBuf), " | +%.0f Max HP",
                              static_cast<double>(it.maxHpBonus));
                ext += bonusBuf;
            }
            if (it.hpRegenBonus > 0.001F) {
                char rgBuf[64];
                std::snprintf(rgBuf, sizeof(rgBuf), " | +%.1f HP/s",
                              static_cast<double>(it.hpRegenBonus));
                ext += rgBuf;
            }
            if (it.damageReflectPercent > 0.001F) {
                char rfBuf[64];
                std::snprintf(rfBuf, sizeof(rfBuf), " | Reflect %.0f%%",
                              static_cast<double>(it.damageReflectPercent * 100.0F));
                ext += rfBuf;
            }
            if (it.isConsumable) {
                char consBuf[80];
                std::snprintf(consBuf, sizeof(consBuf), " | Stack %d/%d", it.stackCount,
                              std::max(1, it.maxStack));
                ext += consBuf;
            }
        }

        const Vector2 titleDim = MeasureTextEx(font, title.c_str(), tipSz + 2.0F, 1.0F);
        const Vector2 rarityDim = MeasureTextEx(font, rline.c_str(), tipSz, 1.0F);
        const float descH = measureMultilineHeight(font, it.description, tipSz);
        const float descW = maxLineWidthMultiline(font, it.description, tipSz);
        const Vector2 extDim =
            altHeld ? MeasureTextEx(font, ext.c_str(), tipSz - 1.0F, 1.0F) : Vector2{0.0F, 0.0F};
        const float pad = 6.0F;
        const float gap = 6.0F;
        float hContent = titleDim.y + gap + rarityDim.y;
        if (!it.description.empty()) {
            hContent += gap + descH;
        }
        if (altHeld) {
            hContent += gap + extDim.y;
        }
        const float tw = std::max(
                           std::max(titleDim.x, rarityDim.x),
                           std::max(it.description.empty() ? 0.0F : descW, extDim.x)) +
                       pad * 2.0F;
        const float th = hContent + pad * 2.0F;
        Rectangle tip{mouse.x + 14.0F, mouse.y - th - 14.0F, tw, th};
        clampRectToScreen(tip, screenW, screenH);
        DrawRectangleRec(tip, Fade(ui::theme::PANEL_FILL, 235));
        DrawRectangleLinesEx(tip, 1.5F,
                             Color{rcol.r, rcol.g, rcol.b, static_cast<unsigned char>(210)});
        float yc = tip.y + pad;
        DrawTextEx(font, title.c_str(), {tip.x + pad, yc}, tipSz + 2.0F, 1.0F, RAYWHITE);
        yc += titleDim.y + gap;
        DrawTextEx(font, rline.c_str(), {tip.x + pad, yc}, tipSz, 1.0F, rcol);
        yc += rarityDim.y + gap;
        if (!it.description.empty()) {
            drawMultilineText(font, it.description, tip.x + pad, yc, tipSz, ui::theme::LABEL_TEXT);
            yc += descH + gap;
        }
        if (altHeld) {
            DrawTextEx(font, ext.c_str(), {tip.x + pad, yc}, tipSz - 1.0F, 1.0F,
                       ui::theme::MUTED_TEXT);
        }
    }
}

} // namespace dreadcast::ui
