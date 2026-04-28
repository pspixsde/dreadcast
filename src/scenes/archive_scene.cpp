#include "scenes/archive_scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "game/game_data.hpp"
#include "game/item_rarity.hpp"
#include "game/items.hpp"
#include "scenes/scene_manager.hpp"
#include "ui/inventory_ui.hpp"
#include "ui/skill_tree_ui.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

namespace {

struct EnemyArchiveEntry {
    const char *name{};
    const char *summary{};
    const char *statsLine{};
    const char *description{};
    const char *lore{};
};

[[nodiscard]] const std::array<EnemyArchiveEntry, 2> &enemyArchiveEntries() {
    static const std::array<EnemyArchiveEntry, 2> k{{
        {"Imp",
         "Kiting ranged nuisance of the Pit.",
         "HP 50  |  Curse bolt ~15 dmg  |  Shoot cd 2.0s  |  Preferred range 220",
         "Cowardly imp that keeps distance, strafes, and peppers you with curse bolts when "
         "agitated.",
         "Dwellers associated with the Pit's cruelty and mockery; specifics TBD."},
        {"Hellhound",
         "Fast melee bruiser that closes the gap.",
         "HP 60  |  Melee 15 dmg  |  Chase 210 u/s  |  Melee range 44",
         "Once agitated, chases the player down and tears at them in melee.",
         "To expand: origin, how they relate to cursed beasts or the Pit."},
    }};
    return k;
}

void drawWrappedText(const Font &font, const char *text, Rectangle area, float fontSize, Color col) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    std::istringstream iss(text);
    std::string word;
    std::string line;
    float y = area.y;
    const float lineH = fontSize + 3.0F;
    const auto flush = [&]() {
        if (!line.empty()) {
            DrawTextEx(font, line.c_str(), {area.x, y}, fontSize, 1.0F, col);
            y += lineH;
            line.clear();
        }
    };
    while (iss >> word) {
        if (MeasureTextEx(font, word.c_str(), fontSize, 1.0F).x > area.width) {
            flush();
            while (!word.empty() && y + lineH <= area.y + area.height) {
                size_t lo = 1;
                size_t hi = word.size();
                size_t best = 1;
                while (lo <= hi) {
                    const size_t mid = (lo + hi) / 2;
                    const std::string part = word.substr(0, mid);
                    if (MeasureTextEx(font, part.c_str(), fontSize, 1.0F).x <= area.width) {
                        best = mid;
                        lo = mid + 1;
                    } else {
                        if (mid == 0) {
                            break;
                        }
                        hi = mid - 1;
                    }
                }
                if (best < 1) {
                    best = 1;
                }
                const std::string part = word.substr(0, best);
                DrawTextEx(font, part.c_str(), {area.x, y}, fontSize, 1.0F, col);
                y += lineH;
                word = word.substr(best);
            }
            continue;
        }
        const std::string tryLine = line.empty() ? word : line + " " + word;
        if (MeasureTextEx(font, tryLine.c_str(), fontSize, 1.0F).x <= area.width) {
            line = tryLine;
        } else {
            flush();
            line = word;
        }
        if (y + lineH > area.y + area.height) {
            return;
        }
    }
    flush();
}

} // namespace

int ArchiveScene::entryCountForTab() const {
    switch (activeTab_) {
    case Tab::Items:
        return static_cast<int>(allCatalogItems().size());
    case Tab::Abilities:
        return static_cast<int>(undeadHunterAbilities().abilities.size());
    case Tab::Skills:
        return ui::kSkillTreeNodeCount;
    case Tab::Enemies:
        return static_cast<int>(enemyArchiveEntries().size());
    }
    return 0;
}

void ArchiveScene::clampSelection() {
    const int n = entryCountForTab();
    if (n <= 0) {
        selectedRow_ = 0;
        return;
    }
    selectedRow_ = std::clamp(selectedRow_, 0, n - 1);
}

void ArchiveScene::cycleTab(int delta) {
    const int t = static_cast<int>(activeTab_) + delta;
    const int nt = 4;
    activeTab_ = static_cast<Tab>((t % nt + nt) % nt);
    selectedRow_ = 0;
    listScrollY_ = 0.0F;
}

Rectangle ArchiveScene::listPanelRect() const {
    return {24.0F, 96.0F, 360.0F, static_cast<float>(config::WINDOW_HEIGHT) - 120.0F};
}

Rectangle ArchiveScene::detailPanelRect() const {
    const Rectangle lp = listPanelRect();
    return {lp.x + lp.width + 20.0F, lp.y,
            static_cast<float>(config::WINDOW_WIDTH) - (lp.x + lp.width + 44.0F), lp.height};
}

Rectangle ArchiveScene::pickerContentRect() const {
    const Rectangle lp = listPanelRect();
    return {lp.x + 4.0F, lp.y + 8.0F, lp.width - 8.0F, lp.height - 16.0F};
}

void ArchiveScene::update(SceneManager &scenes, InputManager &input, ResourceManager & /*resources*/,
                          float /*frameDt*/) {
    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);

    const int w = config::WINDOW_WIDTH;
    backButton_.rect = {static_cast<float>(w) - 168.0F, 28.0F, 140.0F, 44.0F};
    backButton_.label = "Back";

    const float tabY = 28.0F;
    const float tabX0 = 24.0F;
    const float tabW = 160.0F;
    const float tabGap = 10.0F;
    const char *labels[4] = {"Items", "Abilities", "Skills", "Enemies"};
    for (int i = 0; i < 4; ++i) {
        tabButtons_[static_cast<size_t>(i)].rect = {tabX0 + static_cast<float>(i) * (tabW + tabGap),
                                                    tabY, tabW, 44.0F};
        tabButtons_[static_cast<size_t>(i)].label = labels[i];
    }

    if (backButton_.wasClicked(mouse, click)) {
        scenes.pop();
        return;
    }

    for (int i = 0; i < 4; ++i) {
        if (tabButtons_[static_cast<size_t>(i)].wasClicked(mouse, click)) {
            activeTab_ = static_cast<Tab>(i);
            selectedRow_ = 0;
            listScrollY_ = 0.0F;
            return;
        }
    }

    if (input.isKeyPressed(KEY_ESCAPE)) {
        scenes.pop();
        return;
    }
    if (input.isKeyPressed(KEY_LEFT)) {
        cycleTab(-1);
    }
    if (input.isKeyPressed(KEY_RIGHT)) {
        cycleTab(1);
    }

    const Rectangle list = listPanelRect();
    if (CheckCollisionPointRec(mouse, list)) {
        const float wheel = GetMouseWheelMove();
        if (std::fabs(wheel) > 0.001F) {
            listScrollY_ -= wheel * 28.0F;
            listScrollY_ = std::max(0.0F, listScrollY_);
        }
    }

    clampSelection();
    const Rectangle content = pickerContentRect();
    constexpr float rowH = 30.0F;
    const int n = entryCountForTab();
    const float totalH = static_cast<float>(n) * rowH;
    const float maxScroll = std::max(0.0F, totalH - content.height);
    listScrollY_ = std::min(listScrollY_, maxScroll);

    if (click && CheckCollisionPointRec(mouse, content) && n > 0) {
        const float localY = mouse.y - content.y + listScrollY_;
        const int row = static_cast<int>(localY / rowH);
        if (row >= 0 && row < n) {
            selectedRow_ = row;
        }
    }
}

void ArchiveScene::drawListPanel(const Font &font, Vector2 mouse) {
    const Rectangle lp = listPanelRect();
    DrawRectangleRec(lp, Fade(ui::theme::PANEL_FILL, 250));
    DrawRectangleLinesEx(lp, 2.0F, ui::theme::PANEL_BORDER);

    const Rectangle content = pickerContentRect();
    constexpr float rowH = 30.0F;
    const int n = entryCountForTab();

    BeginScissorMode(static_cast<int>(content.x), static_cast<int>(content.y),
                     static_cast<int>(content.width), static_cast<int>(content.height));

    if (n <= 0) {
        DrawTextEx(font, "(no entries)", {content.x + 8.0F, content.y + 6.0F}, 16.0F, 1.0F,
                   ui::theme::MUTED_TEXT);
    } else {
        for (int i = 0; i < n; ++i) {
            const float y = content.y + static_cast<float>(i) * rowH - listScrollY_;
            if (y + rowH < content.y || y > content.y + content.height) {
                continue;
            }
            const Rectangle row{content.x, y, content.width, rowH};
            std::string label;
            Color rowTint = ui::theme::BTN_FILL;
            switch (activeTab_) {
            case Tab::Items: {
                const auto &it = allCatalogItems()[static_cast<size_t>(i)];
                label = it.name.empty() ? it.catalogId : it.name;
                rowTint = Fade(rarityColor(it.rarity), 0.35F);
                break;
            }
            case Tab::Abilities: {
                const auto &ab = undeadHunterAbilities().abilities[static_cast<size_t>(i)];
                label = ab.name;
                break;
            }
            case Tab::Skills: {
                const auto &sk = ui::skill_tree_archive_entries()[static_cast<size_t>(i)];
                label = sk.name;
                break;
            }
            case Tab::Enemies: {
                label = enemyArchiveEntries()[static_cast<size_t>(i)].name;
                break;
            }
            }
            const bool sel = i == selectedRow_;
            const bool hov = CheckCollisionPointRec(mouse, row);
            DrawRectangleRec(row, hov ? ui::theme::BTN_HOVER : (sel ? ui::theme::SLOT_FILL : rowTint));
            DrawRectangleLinesEx(row, 1.0F, sel ? ui::theme::BTN_BORDER : Fade(ui::theme::BTN_BORDER, 140));
            const float fs = 15.0F;
            const float maxW = row.width - 12.0F;
            const std::string fullLabel = label;
            while (label.size() > 3 &&
                   MeasureTextEx(font, (label + "...").c_str(), fs, 1.0F).x > maxW) {
                label.pop_back();
            }
            if (label.size() < fullLabel.size()) {
                label += "...";
            }
            DrawTextEx(font, label.c_str(), {row.x + 8.0F, row.y + 7.0F}, fs, 1.0F, RAYWHITE);
        }
    }

    EndScissorMode();
}

void ArchiveScene::drawDetailPanel(const Font &font, ResourceManager &resources) {
    const Rectangle dp = detailPanelRect();
    DrawRectangleRec(dp, Fade(ui::theme::PANEL_FILL, 250));
    DrawRectangleLinesEx(dp, 2.0F, ui::theme::PANEL_BORDER);

    const int n = entryCountForTab();
    if (n <= 0 || selectedRow_ < 0 || selectedRow_ >= n) {
        DrawTextEx(font, "Select an entry", {dp.x + 16.0F, dp.y + 20.0F}, 18.0F, 1.0F,
                   ui::theme::MUTED_TEXT);
        return;
    }

    float y = dp.y + 16.0F;
    const float bodyW = dp.width - 32.0F;

    auto drawIconTex = [&](const char *path, float size) {
        if (path == nullptr || path[0] == '\0') {
            return;
        }
        const Texture2D tex = resources.getTexture(path);
        if (tex.id == 0) {
            return;
        }
        const Rectangle dst{dp.x + 16.0F, y, size, size};
        const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width), static_cast<float>(tex.height)};
        DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, WHITE);
    };

    switch (activeTab_) {
    case Tab::Items: {
        const ItemData &it = allCatalogItems()[static_cast<size_t>(selectedRow_)];
        const Rectangle iconR{dp.x + 16.0F, y, 72.0F, 72.0F};
        DrawRectangleLinesEx(iconR, 1.5F, rarityColor(it.rarity));
        ui::InventoryUI::drawItemIcon(it, resources, iconR, WHITE);
        y += 84.0F;
        const char *title = it.name.empty() ? it.catalogId.c_str() : it.name.c_str();
        DrawTextEx(font, title, {dp.x + 16.0F, y}, 22.0F, 1.0F, RAYWHITE);
        y += 30.0F;
        char meta[192];
        const char *slot =
            it.isConsumable ? "Consumable"
                            : (it.slot == EquipSlot::Armor
                                   ? "Armor"
                                   : (it.slot == EquipSlot::Amulet ? "Amulet" : "Ring"));
        std::snprintf(meta, sizeof(meta), "%s  |  %s  |  Stack %d / %d", rarityName(it.rarity), slot,
                      it.stackCount, std::max(1, it.maxStack));
        DrawTextEx(font, meta, {dp.x + 16.0F, y}, 15.0F, 1.0F, ui::theme::LABEL_TEXT);
        y += 26.0F;
        drawWrappedText(font, it.description.c_str(), {dp.x + 16.0F, y, bodyW, 220.0F}, 15.0F,
                        Fade(RAYWHITE, 0.92F));
        break;
    }
    case Tab::Abilities: {
        const AbilityDef &ab = undeadHunterAbilities().abilities[static_cast<size_t>(selectedRow_)];
        drawIconTex(ab.iconPath.c_str(), 80.0F);
        y += 88.0F;
        DrawTextEx(font, ab.name.c_str(), {dp.x + 16.0F, y}, 22.0F, 1.0F, RAYWHITE);
        y += 30.0F;
        char line[160];
        std::snprintf(line, sizeof(line), "Mana %.0f  |  Cooldown %.1fs  |  Duration %.1fs",
                      static_cast<double>(ab.manaCost), static_cast<double>(ab.cooldown),
                      static_cast<double>(ab.effectDuration));
        DrawTextEx(font, line, {dp.x + 16.0F, y}, 15.0F, 1.0F, ui::theme::LABEL_TEXT);
        y += 26.0F;
        drawWrappedText(font, ab.description.c_str(), {dp.x + 16.0F, y, bodyW, 260.0F}, 15.0F,
                        Fade(RAYWHITE, 0.92F));
        break;
    }
    case Tab::Skills: {
        const auto &sk = ui::skill_tree_archive_entries()[static_cast<size_t>(selectedRow_)];
        drawIconTex(sk.iconPath, 80.0F);
        y += 88.0F;
        DrawTextEx(font, sk.name, {dp.x + 16.0F, y}, 22.0F, 1.0F, RAYWHITE);
        y += 30.0F;
        const char *t = sk.type == ui::SkillNodeType::Major ? "Major" : "Minor";
        DrawTextEx(font, t, {dp.x + 16.0F, y}, 15.0F, 1.0F, ui::theme::LABEL_TEXT);
        y += 24.0F;
        drawWrappedText(font, sk.description, {dp.x + 16.0F, y, bodyW, 280.0F}, 15.0F,
                        Fade(RAYWHITE, 0.92F));
        break;
    }
    case Tab::Enemies: {
        const EnemyArchiveEntry &en = enemyArchiveEntries()[static_cast<size_t>(selectedRow_)];
        DrawTextEx(font, en.name, {dp.x + 16.0F, y}, 24.0F, 1.0F, RAYWHITE);
        y += 34.0F;
        DrawTextEx(font, en.summary, {dp.x + 16.0F, y}, 16.0F, 1.0F, ui::theme::LABEL_TEXT);
        y += 26.0F;
        DrawTextEx(font, en.statsLine, {dp.x + 16.0F, y}, 14.0F, 1.0F, Fade(SKYBLUE, 0.9F));
        y += 24.0F;
        drawWrappedText(font, en.description, {dp.x + 16.0F, y, bodyW, 160.0F}, 15.0F,
                        Fade(RAYWHITE, 0.92F));
        y += 170.0F;
        if (en.lore != nullptr && en.lore[0] != '\0') {
            DrawTextEx(font, "Lore", {dp.x + 16.0F, y}, 18.0F, 1.0F, Color{200, 170, 120, 255});
            y += 26.0F;
            drawWrappedText(font, en.lore, {dp.x + 16.0F, y, bodyW, dp.y + dp.height - y - 12.0F},
                            14.0F, Fade(RAYWHITE, 0.85F));
        }
        break;
    }
    }
}

void ArchiveScene::draw(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    DrawRectangle(0, 0, w, h, ui::theme::MENU_BG);

    const Font &font = resources.uiFont();
    const Vector2 mouse = GetMousePosition();

    DrawTextEx(font, "Archive", {24.0F, 32.0F}, 36.0F, 1.0F, RAYWHITE);

    for (int ti = 0; ti < 4; ++ti) {
        ui::Button &tb = tabButtons_[static_cast<size_t>(ti)];
        const bool on = static_cast<int>(activeTab_) == ti;
        const Color fill = on ? ui::theme::SLOT_FILL : ui::theme::BTN_FILL;
        const Vector2 md = MeasureTextEx(font, tb.label, 17.0F, 1.0F);
        const bool hov = CheckCollisionPointRec(mouse, tb.rect);
        DrawRectangleRec(tb.rect, hov ? ui::theme::BTN_HOVER : fill);
        DrawRectangleLinesEx(tb.rect, on ? 2.0F : 1.0F, on ? ui::theme::BTN_BORDER : Fade(ui::theme::BTN_BORDER, 160));
        DrawTextEx(font, tb.label,
                   {tb.rect.x + (tb.rect.width - md.x) * 0.5F,
                    tb.rect.y + (tb.rect.height - md.y) * 0.5F},
                   17.0F, 1.0F, RAYWHITE);
    }

    backButton_.draw(font, 20.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);

    drawListPanel(font, mouse);
    drawDetailPanel(font, resources);

    const float hintY = static_cast<float>(h) - 36.0F;
    DrawTextEx(font, "Esc / Back to return  |  Wheel scrolls list  |  Left/Right changes tab",
               {24.0F, hintY}, 15.0F, 1.0F, ui::theme::MUTED_TEXT);
}

} // namespace dreadcast
