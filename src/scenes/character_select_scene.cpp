#include "scenes/character_select_scene.hpp"

#include <algorithm>
#include <cstdio>

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "game/character.hpp"
#include "scenes/scene_manager.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

CharacterSelectScene::CharacterSelectScene(int *selectedClassIndexOut)
    : selectedOut_(selectedClassIndexOut) {
    if (selectedOut_ != nullptr) {
        highlightedIndex_ = std::clamp(*selectedOut_, 0, CLASS_COUNT - 1);
    }
}

void CharacterSelectScene::update(SceneManager &scenes, InputManager &input,
                                  ResourceManager & /*resources*/, float /*frameDt*/) {
    if (selectedOut_ == nullptr) {
        scenes.pop();
        return;
    }

    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (input.isKeyPressed(KEY_ESCAPE)) {
        scenes.pop();
        return;
    }

    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    const float panelMargin = 40.0F;
    const float px = panelMargin;
    const float py = 100.0F; // matches draw() left panel top for hit-testing
    const int cols = 3;
    const float cardW = 110.0F;
    const float cardH = 130.0F;
    const float gap = 16.0F;
    const float gridOriginX = px + 24.0F;
    const float gridOriginY = py + 56.0F;

    for (int i = 0; i < CLASS_COUNT; ++i) {
        const int row = i / cols;
        const int col = i % cols;
        const Rectangle card{gridOriginX + static_cast<float>(col) * (cardW + gap),
                             gridOriginY + static_cast<float>(row) * (cardH + gap), cardW, cardH};
        if (click && CheckCollisionPointRec(mouse, card)) {
            highlightedIndex_ = i;
        }
    }

    backButton_.rect = {panelMargin, 24.0F, 140.0F, 44.0F};
    backButton_.label = "Back";

    chooseButton_.rect = {static_cast<float>(w) - panelMargin - 200.0F,
                          static_cast<float>(h) - 80.0F, 200.0F, 48.0F};
    chooseButton_.label = "Choose";

    if (backButton_.wasClicked(mouse, click)) {
        scenes.pop();
        return;
    }
    if (chooseButton_.wasClicked(mouse, click)) {
        *selectedOut_ = highlightedIndex_;
        scenes.pop();
    }
}

void CharacterSelectScene::draw(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    DrawRectangle(0, 0, w, h, ui::theme::MENU_BG);

    const Font &font = resources.uiFont();

    const char *hdr = "CHARACTER";
    const float hdrSize = 36.0F;
    const Vector2 hdrDim = MeasureTextEx(font, hdr, hdrSize, 1.0F);
    DrawTextEx(font, hdr, {(static_cast<float>(w) - hdrDim.x) * 0.5F, 32.0F}, hdrSize, 1.0F,
               RAYWHITE);

    const float panelMargin = 40.0F;
    const float px = panelMargin;
    const float py = 100.0F;
    const float panelH = static_cast<float>(h) - py - 40.0F;
    DrawRectangleRec({px, py, 420.0F, panelH}, ui::theme::PANEL_FILL);
    DrawRectangleLinesEx({px, py, 420.0F, panelH}, 2.0F, ui::theme::PANEL_BORDER);

    const char *leftTitle = "Classes";
    DrawTextEx(font, leftTitle, {px + 16.0F, py + 12.0F}, 20.0F, 1.0F, ui::theme::LABEL_TEXT);

    const int cols = 3;
    const float cardW = 110.0F;
    const float cardH = 130.0F;
    const float gap = 16.0F;
    const float gridOriginX = px + 24.0F;
    const float gridOriginY = py + 56.0F;

    for (int i = 0; i < CLASS_COUNT; ++i) {
        const int row = i / cols;
        const int col = i % cols;
        const Rectangle card{gridOriginX + static_cast<float>(col) * (cardW + gap),
                             gridOriginY + static_cast<float>(row) * (cardH + gap), cardW, cardH};
        const bool sel = i == highlightedIndex_;
        const Color fill = sel ? ui::theme::BTN_HOVER : ui::theme::SLOT_FILL;
        const Color border = sel ? ui::theme::BTN_BORDER : ui::theme::SLOT_BORDER;
        DrawRectangleRec(card, fill);
        DrawRectangleLinesEx(card, 2.0F, border);

        const CharacterClass &cls = AVAILABLE_CLASSES[static_cast<size_t>(i)];
        const float pr = 28.0F;
        const float cx = card.x + card.width * 0.5F;
        const float cy = card.y + 20.0F + pr;
        DrawCircle(static_cast<int>(cx), static_cast<int>(cy), pr, ui::theme::PORTRAIT_FILL);
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), pr, ui::theme::PORTRAIT_RING);
        char ini[2] = {cls.name[0], '\0'};
        const float pFont = 32.0F;
        const Vector2 pDim = MeasureTextEx(font, ini, pFont, 1.0F);
        DrawTextEx(font, ini, {cx - pDim.x * 0.5F, cy - pDim.y * 0.5F}, pFont, 1.0F, RAYWHITE);

        const float nameSize = 15.0F;
        const Vector2 nDim = MeasureTextEx(font, cls.name, nameSize, 1.0F);
        DrawTextEx(font, cls.name, {card.x + (card.width - nDim.x) * 0.5F, card.y + cardH - 28.0F},
                   nameSize, 1.0F, RAYWHITE);
    }

    const float detailX = px + 440.0F;
    const float detailW = static_cast<float>(w) - detailX - panelMargin;
    DrawRectangleRec({detailX, py, detailW, panelH}, ui::theme::PANEL_FILL);
    DrawRectangleLinesEx({detailX, py, detailW, panelH}, 2.0F, ui::theme::PANEL_BORDER);

    const CharacterClass &sel = AVAILABLE_CLASSES[static_cast<size_t>(highlightedIndex_)];
    DrawTextEx(font, "Details", {detailX + 20.0F, py + 16.0F}, 22.0F, 1.0F,
               ui::theme::LABEL_TEXT);

    const float dpr = 52.0F;
    const float dcx = detailX + 80.0F;
    const float dcy = py + 100.0F;
    DrawCircle(static_cast<int>(dcx), static_cast<int>(dcy), dpr, ui::theme::PORTRAIT_FILL);
    DrawCircleLines(static_cast<int>(dcx), static_cast<int>(dcy), dpr, ui::theme::PORTRAIT_RING);
    char ini2[2] = {sel.name[0], '\0'};
    const float dpFont = 56.0F;
    const Vector2 dpd = MeasureTextEx(font, ini2, dpFont, 1.0F);
    DrawTextEx(font, ini2, {dcx - dpd.x * 0.5F, dcy - dpd.y * 0.5F}, dpFont, 1.0F, RAYWHITE);

    DrawTextEx(font, sel.name, {detailX + 160.0F, py + 70.0F}, 28.0F, 1.0F, RAYWHITE);
    DrawTextEx(font, sel.description, {detailX + 24.0F, py + 170.0F}, 17.0F, 1.0F,
               ui::theme::SUBTITLE_TEXT);

    DrawTextEx(font, "Abilities", {detailX + 24.0F, py + 220.0F}, 18.0F, 1.0F,
               ui::theme::LABEL_TEXT);
    DrawTextEx(font, sel.detailAbilities, {detailX + 24.0F, py + 250.0F}, 16.0F, 1.0F,
               ui::theme::MUTED_TEXT);

    char regenBuf[96];
    std::snprintf(regenBuf, sizeof(regenBuf), "Passive Regen: +%.1f HP/s, +%.1f Mana/s",
                  sel.hpRegen, sel.manaRegen);
    DrawTextEx(font, regenBuf, {detailX + 24.0F, py + 330.0F}, 16.0F, 1.0F,
               ui::theme::SUBTITLE_TEXT);

    const Vector2 mouse = GetMousePosition();
    backButton_.draw(font, 20.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
    chooseButton_.draw(font, 22.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                       ui::theme::BTN_BORDER);
}

} // namespace dreadcast
