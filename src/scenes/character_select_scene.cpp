#include "scenes/character_select_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "game/character.hpp"
#include "game/game_data.hpp"
#include "scenes/scene_manager.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

namespace {

float drawMultiline(const Font &font, const char *text, float x, float y, float lineSpacing,
                    float fontSize, Color col) {
    if (text == nullptr || text[0] == '\0') {
        return 0.0F;
    }
    const float y0 = y;
    float cy = y;
    const char *lineStart = text;
    for (;;) {
        const char *newline = std::strchr(lineStart, '\n');
        const size_t len =
            newline != nullptr ? static_cast<size_t>(newline - lineStart) : std::strlen(lineStart);
        if (len > 0) {
            std::string line(lineStart, len);
            DrawTextEx(font, line.c_str(), {x, cy}, fontSize, 1.0F, col);
            const Vector2 dim = MeasureTextEx(font, line.c_str(), fontSize, 1.0F);
            cy += std::max(lineSpacing, dim.y + 2.0F);
        } else {
            cy += lineSpacing;
        }
        if (newline == nullptr) {
            break;
        }
        lineStart = newline + 1;
    }
    return cy - y0;
}

void drawHRule(float x0, float x1, float y, Color c) {
    DrawLineEx({x0, y}, {x1, y}, 1.5F, Fade(c, 0.78F));
}

} // namespace

CharacterSelectScene::CharacterSelectScene(int *selectedClassIndexOut)
    : selectedOut_(selectedClassIndexOut) {
    if (selectedOut_ != nullptr) {
        highlightedIndex_ = std::clamp(*selectedOut_, 0, std::max(0, characterCount() - 1));
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

    for (int i = 0; i < characterCount(); ++i) {
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

    const char *leftTitle = "Characters";
    DrawTextEx(font, leftTitle, {px + 16.0F, py + 12.0F}, 20.0F, 1.0F, ui::theme::LABEL_TEXT);

    const int cols = 3;
    const float cardW = 110.0F;
    const float cardH = 130.0F;
    const float gap = 16.0F;
    const float gridOriginX = px + 24.0F;
    const float gridOriginY = py + 56.0F;

    for (int i = 0; i < characterCount(); ++i) {
        const int row = i / cols;
        const int col = i % cols;
        const Rectangle card{gridOriginX + static_cast<float>(col) * (cardW + gap),
                             gridOriginY + static_cast<float>(row) * (cardH + gap), cardW, cardH};
        const bool sel = i == highlightedIndex_;
        const Color fill = sel ? ui::theme::BTN_HOVER : ui::theme::SLOT_FILL;
        const Color border = sel ? ui::theme::BTN_BORDER : ui::theme::SLOT_BORDER;
        DrawRectangleRec(card, fill);
        DrawRectangleLinesEx(card, 2.0F, border);

        const CharacterClass &cls = characterAt(i);
        const float pr = 28.0F;
        const float cx = card.x + card.width * 0.5F;
        const float cy = card.y + 20.0F + pr;
        DrawCircle(static_cast<int>(cx), static_cast<int>(cy), pr, ui::theme::PORTRAIT_FILL);
        DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), pr, ui::theme::PORTRAIT_RING);
        const char iniCh = cls.name.empty() ? '?' : cls.name[0];
        char ini[2] = {iniCh, '\0'};
        const float pFont = 32.0F;
        const Vector2 pDim = MeasureTextEx(font, ini, pFont, 1.0F);
        DrawTextEx(font, ini, {cx - pDim.x * 0.5F, cy - pDim.y * 0.5F}, pFont, 1.0F, RAYWHITE);

        const float nameSize = 15.0F;
        const Vector2 nDim = MeasureTextEx(font, cls.name.c_str(), nameSize, 1.0F);
        DrawTextEx(font, cls.name.c_str(), {card.x + (card.width - nDim.x) * 0.5F, card.y + cardH - 28.0F},
                   nameSize, 1.0F, RAYWHITE);
    }

    const float detailX = px + 440.0F;
    const float detailW = static_cast<float>(w) - detailX - panelMargin;
    DrawRectangleRec({detailX, py, detailW, panelH}, ui::theme::PANEL_FILL);
    DrawRectangleLinesEx({detailX, py, detailW, panelH}, 2.0F, ui::theme::PANEL_BORDER);

    const CharacterClass &sel = characterAt(highlightedIndex_);
    const float padL = 22.0F;
    const float padR = 18.0F;
    float y = py + 14.0F;

    DrawTextEx(font, "Details", {detailX + padL, y}, 22.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 34.0F;

    const float dpr = 40.0F;
    const float dcx = detailX + padL + dpr;
    const float dcy = y + dpr;
    DrawCircle(static_cast<int>(dcx), static_cast<int>(dcy), dpr, ui::theme::PORTRAIT_FILL);
    DrawCircleLines(static_cast<int>(dcx), static_cast<int>(dcy), dpr, ui::theme::PORTRAIT_RING);
    const char ini2Ch = sel.name.empty() ? '?' : sel.name[0];
    char ini2[2] = {ini2Ch, '\0'};
    const float dpFont = 44.0F;
    const Vector2 dpd = MeasureTextEx(font, ini2, dpFont, 1.0F);
    DrawTextEx(font, ini2, {dcx - dpd.x * 0.5F, dcy - dpd.y * 0.5F}, dpFont, 1.0F, RAYWHITE);

    const float nameX = detailX + padL + dpr * 2.0F + 16.0F;
    DrawTextEx(font, sel.name.c_str(), {nameX, y + 18.0F}, 26.0F, 1.0F, RAYWHITE);
    y += dpr * 2.0F + 12.0F;

    DrawTextEx(font, sel.description.c_str(), {detailX + padL, y}, 17.0F, 1.0F, ui::theme::SUBTITLE_TEXT);
    {
        const Vector2 dd = MeasureTextEx(font, sel.description.c_str(), 17.0F, 1.0F);
        y += dd.y + 14.0F;
    }

    DrawTextEx(font, "Abilities", {detailX + padL, y}, 18.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 26.0F;
    y += drawMultiline(font, sel.detailAbilities.c_str(), detailX + padL, y, 18.0F, 15.0F,
                       ui::theme::MUTED_TEXT);
    y += 12.0F;

    drawHRule(detailX + padL, detailX + detailW - padR, y, ui::theme::PANEL_BORDER);
    y += 14.0F;

    DrawTextEx(font, "Bio", {detailX + padL, y}, 18.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 24.0F;
    y += drawMultiline(font, sel.bio.c_str(), detailX + padL, y, 20.0F, 15.0F, ui::theme::MUTED_TEXT);
    y += 12.0F;

    drawHRule(detailX + padL, detailX + detailW - padR, y, ui::theme::PANEL_BORDER);
    y += 14.0F;

    DrawTextEx(font, "Health & Mana", {detailX + padL, y}, 18.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 28.0F;

    {
        constexpr float barW = 132.0F;
        constexpr float barH = 12.0F;
        const Rectangle hpBar{detailX + padL, y + 3.0F, barW, barH};
        DrawRectangleRec(hpBar, Color{90, 22, 28, 220});
        DrawRectangleRec({hpBar.x, hpBar.y, barW * 0.92F, barH}, Color{200, 55, 65, 244});
        DrawRectangleLinesEx(hpBar, 1.0F, Fade(ui::theme::PANEL_BORDER, 0.72F));
        char hpBuf[96];
        std::snprintf(hpBuf, sizeof(hpBuf), "%.0f max HP   +%.1f /s", static_cast<double>(sel.baseMaxHp),
                      static_cast<double>(sel.hpRegen));
        DrawTextEx(font, hpBuf, {detailX + padL + barW + 14.0F, y}, 15.0F, 1.0F, RAYWHITE);
        y += 28.0F;

        const Rectangle mpBar{detailX + padL, y + 3.0F, barW, barH};
        DrawRectangleRec(mpBar, Color{22, 38, 90, 220});
        DrawRectangleRec({mpBar.x, mpBar.y, barW * 0.92F, barH}, Color{70, 120, 220, 244});
        DrawRectangleLinesEx(mpBar, 1.0F, Fade(ui::theme::PANEL_BORDER, 0.72F));
        char mpBuf[96];
        std::snprintf(mpBuf, sizeof(mpBuf), "%.0f max Mana   +%.1f /s",
                      static_cast<double>(sel.baseMaxMana), static_cast<double>(sel.manaRegen));
        DrawTextEx(font, mpBuf, {detailX + padL + barW + 14.0F, y}, 15.0F, 1.0F, RAYWHITE);
        y += 34.0F;
    }

    drawHRule(detailX + padL, detailX + detailW - padR, y, ui::theme::PANEL_BORDER);
    y += 14.0F;

    DrawTextEx(font, "Attack", {detailX + padL, y}, 18.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 26.0F;
    DrawTextEx(font, "Ranged", {detailX + padL + 6.0F, y}, 15.0F, 1.0F, ui::theme::SUBTITLE_TEXT);
    y += 22.0F;
    {
        char b1[160];
        std::snprintf(b1, sizeof(b1),
                      "Curse bolt damage %.0f   range %.0f   speed %.0f (world units / second)",
                      static_cast<double>(sel.rangedDamage), static_cast<double>(sel.rangedRange),
                      static_cast<double>(sel.rangedProjectileSpeed));
        DrawTextEx(font, b1, {detailX + padL + 14.0F, y}, 14.0F, 1.0F, RAYWHITE);
        y += 22.0F;
    }
    DrawTextEx(font, "Melee", {detailX + padL + 6.0F, y}, 15.0F, 1.0F, ui::theme::SUBTITLE_TEXT);
    y += 22.0F;
    {
        char b2[128];
        std::snprintf(b2, sizeof(b2), "Combo hits %.0f damage each   reach %.0f",
                      static_cast<double>(sel.meleeDamage), static_cast<double>(sel.meleeRange));
        DrawTextEx(font, b2, {detailX + padL + 14.0F, y}, 14.0F, 1.0F, RAYWHITE);
        y += 22.0F;
        DrawTextEx(font, "Three-hit string, frontal cone, hold RMB to chain.", {detailX + padL + 14.0F, y},
                   13.0F, 1.0F, ui::theme::MUTED_TEXT);
        y += 28.0F;
    }

    drawHRule(detailX + padL, detailX + detailW - padR, y, ui::theme::PANEL_BORDER);
    y += 14.0F;

    DrawTextEx(font, "Mobility", {detailX + padL, y}, 18.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 26.0F;
    {
        char m1[128];
        std::snprintf(m1, sizeof(m1), "Base move speed %.0f (world units / second)",
                      static_cast<double>(sel.moveSpeed));
        DrawTextEx(font, m1, {detailX + padL + 6.0F, y}, 14.0F, 1.0F, RAYWHITE);
        y += 22.0F;
        char m2[128];
        std::snprintf(m2, sizeof(m2), "Fog vision radius %.0f (exploration / combat awareness)",
                      static_cast<double>(sel.visionRange));
        DrawTextEx(font, m2, {detailX + padL + 6.0F, y}, 14.0F, 1.0F, RAYWHITE);
        y += 28.0F;
    }

    drawHRule(detailX + padL, detailX + detailW - padR, y, ui::theme::PANEL_BORDER);
    y += 14.0F;

    DrawTextEx(font, "Leveling", {detailX + padL, y}, 18.0F, 1.0F, ui::theme::LABEL_TEXT);
    y += 26.0F;
    {
        char lv[256];
        std::snprintf(lv, sizeof(lv),
                      "Each level requires 100 XP (from enemy kills). Per level: +%.0f max HP, "
                      "+%.0f max Mana, +%.0f ranged damage, +%.0f melee damage.",
                      static_cast<double>(sel.levelMaxHpGain),
                      static_cast<double>(sel.levelMaxManaGain),
                      static_cast<double>(sel.levelProjectileDamageGain),
                      static_cast<double>(sel.levelMeleeDamageGain));
        y += drawMultiline(font, lv, detailX + padL, y, 20.0F, 14.0F, ui::theme::MUTED_TEXT);
    }

    const Vector2 mouse = GetMousePosition();
    backButton_.draw(font, 20.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
    chooseButton_.draw(font, 22.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                       ui::theme::BTN_BORDER);
}

} // namespace dreadcast
