#include "scenes/settings_scene.hpp"

#include <algorithm>
#include <cstdio>

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "scenes/scene_manager.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

namespace {

constexpr float kMouseSensMin = 0.1F;
constexpr float kMouseSensMax = 3.0F;

void saveSettingsNow(ResourceManager &resources) {
    (void)resources.settings().saveToFile("settings.cfg");
}

void updateMouseSensitivitySlider(Vector2 mouse, bool mouseDown, bool mousePressed,
                                  ResourceManager &resources, bool &dragging, Rectangle track) {
    if (mousePressed && CheckCollisionPointRec(mouse, track)) {
        dragging = true;
    }
    if (!mouseDown) {
        dragging = false;
    }
    if (dragging || (mousePressed && CheckCollisionPointRec(mouse, track))) {
        float t = (mouse.x - track.x) / track.width;
        t = std::clamp(t, 0.0F, 1.0F);
        resources.settings().mouseSensitivity = kMouseSensMin + t * (kMouseSensMax - kMouseSensMin);
    }
}

void drawMouseSensitivitySlider(const Font &font, Vector2 mouse, float value, Rectangle track) {
    const float labelSz = 22.0F;
    DrawTextEx(font, "Mouse sensitivity", {track.x, track.y - 32.0F}, labelSz, 1.0F,
               ui::theme::LABEL_TEXT);
    DrawRectangleRec(track, Fade(ui::theme::BTN_FILL, 200));
    DrawRectangleLinesEx(track, 2.0F, ui::theme::BTN_BORDER);
    const float t = (value - kMouseSensMin) / (kMouseSensMax - kMouseSensMin);
    const float knobX = track.x + t * track.width;
    const float knobY = track.y + track.height * 0.5F;
    const bool over = CheckCollisionPointCircle(mouse, {knobX, knobY}, 14.0F) ||
                      CheckCollisionPointRec(mouse, track);
    DrawCircleV({knobX, knobY}, over ? 13.0F : 11.0F, ui::theme::BTN_HOVER);
    DrawCircleLinesV({knobX, knobY}, over ? 13.0F : 11.0F, ui::theme::BTN_BORDER);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(value));
    DrawTextEx(font, buf, {track.x + track.width + 16.0F, track.y - 2.0F}, 20.0F, 1.0F,
               RAYWHITE);
}

} // namespace

void SettingsScene::update(SceneManager &scenes, InputManager &input,
                           ResourceManager &resources, float /*frameDt*/) {
    if (input.isKeyPressed(KEY_ESCAPE)) {
        scenes.pop();
        return;
    }

    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool mouseDown = input.isMouseButtonHeld(MOUSE_BUTTON_LEFT);

    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;

    const float tabY = 110.0F;
    const float tabW = 170.0F;
    const float tabH = 40.0F;
    const float gap = 8.0F;
    const float tabsTotal = tabW * 3.0F + gap * 2.0F;
    const float tabX = (static_cast<float>(w) - tabsTotal) * 0.5F;

    gameplayTabButton_.rect = {tabX, tabY, tabW, tabH};
    gameplayTabButton_.label = "Gameplay";
    controlsTabButton_.rect = {tabX + tabW + gap, tabY, tabW, tabH};
    controlsTabButton_.label = "Controls";
    videoTabButton_.rect = {tabX + (tabW + gap) * 2.0F, tabY, tabW, tabH};
    videoTabButton_.label = "Video";

    if (click) {
        if (gameplayTabButton_.wasClicked(mouse, click)) {
            activeTab_ = Tab::Gameplay;
        } else if (controlsTabButton_.wasClicked(mouse, click)) {
            activeTab_ = Tab::Controls;
        } else if (videoTabButton_.wasClicked(mouse, click)) {
            activeTab_ = Tab::Video;
        }
    }

    if (activeTab_ != Tab::Controls) {
        draggingMouseSensSlider_ = false;
        mouseSensSliderPrevDown_ = false;
    }

    const float toggleBtnX = static_cast<float>(w) * 0.5F + 120.0F;
    const float toggleBtnW = 80.0F;
    const float toggleBtnH = 34.0F;

    if (activeTab_ == Tab::Gameplay) {
        const float rowY0 = 200.0F;
        manaCostToggleButton_.rect = {toggleBtnX, rowY0 - 3.0F, toggleBtnW, toggleBtnH};
        manaCostToggleButton_.label =
            resources.settings().showAbilityManaCost ? "On" : "Off";
        if (manaCostToggleButton_.wasClicked(mouse, click)) {
            resources.settings().showAbilityManaCost = !resources.settings().showAbilityManaCost;
            saveSettingsNow(resources);
        }
        const float rowY1 = rowY0 + 46.0F;
        damageNumbersToggleButton_.rect = {toggleBtnX, rowY1 - 3.0F, toggleBtnW, toggleBtnH};
        damageNumbersToggleButton_.label =
            resources.settings().showDamageNumbers ? "On" : "Off";
        if (damageNumbersToggleButton_.wasClicked(mouse, click)) {
            resources.settings().showDamageNumbers = !resources.settings().showDamageNumbers;
            saveSettingsNow(resources);
        }
    } else if (activeTab_ == Tab::Controls) {
        const Rectangle sensTrack = {static_cast<float>(w) * 0.5F - 150.0F, 190.0F, 300.0F,
                                       22.0F};
        updateMouseSensitivitySlider(mouse, mouseDown, click, resources, draggingMouseSensSlider_,
                                     sensTrack);
        if (mouseSensSliderPrevDown_ && !mouseDown) {
            saveSettingsNow(resources);
        }
        mouseSensSliderPrevDown_ = mouseDown;

        resetButton_.rect = {(static_cast<float>(w) - 200.0F) * 0.5F,
                             static_cast<float>(h) - 170.0F, 200.0F, 44.0F};
        resetButton_.label = "Reset";
        if (resetButton_.wasClicked(mouse, click)) {
            resources.settings() = GameSettings{};
            saveSettingsNow(resources);
        }
    }

    if (activeTab_ == Tab::Video) {
        const float rowY = 200.0F;
        fpsCounterToggleButton_.rect = {toggleBtnX, rowY - 3.0F, toggleBtnW, toggleBtnH};
        fpsCounterToggleButton_.label =
            resources.settings().showFpsCounter ? "On" : "Off";
        if (fpsCounterToggleButton_.wasClicked(mouse, click)) {
            resources.settings().showFpsCounter = !resources.settings().showFpsCounter;
            saveSettingsNow(resources);
        }
    }

    backButton_.rect = {(static_cast<float>(w) - 200.0F) * 0.5F,
                        static_cast<float>(h) - 100.0F, 200.0F, 48.0F};
    backButton_.label = "Back";

    if (backButton_.wasClicked(mouse, click)) {
        scenes.pop();
    }
}

void SettingsScene::draw(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    DrawRectangle(0, 0, w, h, ui::theme::MENU_BG);

    const Font &font = resources.uiFont();

    const char *title = "Settings";
    const float titleSize = 40.0F;
    const Vector2 titleDim = MeasureTextEx(font, title, titleSize, 1.0F);
    DrawTextEx(font, title, {(static_cast<float>(w) - titleDim.x) * 0.5F, 48.0F}, titleSize, 1.0F,
               RAYWHITE);

    const Vector2 mouse = GetMousePosition();

    const float tabY = 110.0F;
    const float tabW = 170.0F;
    const float tabH = 40.0F;
    const float gap = 8.0F;
    const float tabsTotal = tabW * 3.0F + gap * 2.0F;
    const float tabX = (static_cast<float>(w) - tabsTotal) * 0.5F;

    gameplayTabButton_.rect = {tabX, tabY, tabW, tabH};
    gameplayTabButton_.label = "Gameplay";
    controlsTabButton_.rect = {tabX + tabW + gap, tabY, tabW, tabH};
    controlsTabButton_.label = "Controls";
    videoTabButton_.rect = {tabX + (tabW + gap) * 2.0F, tabY, tabW, tabH};
    videoTabButton_.label = "Video";

    gameplayTabButton_.draw(font, 18.0F, mouse,
                            activeTab_ == Tab::Gameplay ? ui::theme::BTN_HOVER : ui::theme::BTN_FILL,
                            ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);
    controlsTabButton_.draw(font, 18.0F, mouse,
                            activeTab_ == Tab::Controls ? ui::theme::BTN_HOVER : ui::theme::BTN_FILL,
                            ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);
    videoTabButton_.draw(font, 18.0F, mouse,
                         activeTab_ == Tab::Video ? ui::theme::BTN_HOVER : ui::theme::BTN_FILL,
                         ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);

    const float toggleBtnX = static_cast<float>(w) * 0.5F + 120.0F;
    const float toggleBtnW = 80.0F;
    const float toggleBtnH = 34.0F;

    if (activeTab_ == Tab::Gameplay) {
        const float labelSz = 22.0F;
        const float rowX = static_cast<float>(w) * 0.5F - 220.0F;
        float rowY = 200.0F;
        const char *manaLbl = "Show ability mana cost";
        DrawTextEx(font, manaLbl, {rowX, rowY}, labelSz, 1.0F, ui::theme::LABEL_TEXT);
        manaCostToggleButton_.rect = {toggleBtnX, rowY - 3.0F, toggleBtnW, toggleBtnH};
        manaCostToggleButton_.label =
            resources.settings().showAbilityManaCost ? "On" : "Off";
        manaCostToggleButton_.draw(font, 18.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER,
                                   RAYWHITE, ui::theme::BTN_BORDER);
        rowY += 46.0F;
        const char *dmgLbl = "Show damage/heal numbers";
        DrawTextEx(font, dmgLbl, {rowX, rowY}, labelSz, 1.0F, ui::theme::LABEL_TEXT);
        damageNumbersToggleButton_.rect = {toggleBtnX, rowY - 3.0F, toggleBtnW, toggleBtnH};
        damageNumbersToggleButton_.label =
            resources.settings().showDamageNumbers ? "On" : "Off";
        damageNumbersToggleButton_.draw(font, 18.0F, mouse, ui::theme::BTN_FILL,
                                        ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);
    } else if (activeTab_ == Tab::Controls) {
        const Rectangle sensTrack = {static_cast<float>(w) * 0.5F - 150.0F, 190.0F, 300.0F,
                                     22.0F};
        drawMouseSensitivitySlider(font, mouse, resources.settings().mouseSensitivity, sensTrack);

        const float rowLabelX = static_cast<float>(w) * 0.5F - 280.0F;
        const float rowKeyX = static_cast<float>(w) * 0.5F + 40.0F;
        float rowY = 268.0F;
        const float rowGap = 36.0F;
        const float textSize = 20.0F;

        struct Row {
            const char *action;
            const char *key;
        };
        const Row rows[] = {{"Move", "WASD / Arrow keys"},
                            {"Aim", "Mouse (sensitivity above)"},
                            {"Ranged attack", "Left mouse button"},
                            {"Melee attack", "Right mouse button"},
                            {"Pick up item", "E (near drop, cursor on item)"},
                            {"Interact", "F (near object, cursor on it)"},
                            {"Use consumable slots", "C / V"},
                            {"Character abilities", "1 / 2 / 3 (Undead Hunter)"},
                            {"Item details (world)", "Hold Alt over a drop"},
                            {"Inventory", "Tab"},
                            {"Pause", "Esc"}};

        for (const Row &r : rows) {
            DrawTextEx(font, r.action, {rowLabelX, rowY}, textSize, 1.0F, ui::theme::LABEL_TEXT);
            DrawTextEx(font, r.key, {rowKeyX, rowY}, textSize, 1.0F, RAYWHITE);
            rowY += rowGap;
        }

        resetButton_.rect = {(static_cast<float>(w) - 200.0F) * 0.5F,
                             static_cast<float>(h) - 170.0F, 200.0F, 44.0F};
        resetButton_.label = "Reset";
        resetButton_.draw(font, 18.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                          ui::theme::BTN_BORDER);
    } else {
        const float labelSz = 22.0F;
        const char *label = "Show FPS Counter";
        const Vector2 labelDim = MeasureTextEx(font, label, labelSz, 1.0F);
        const float rowY = 200.0F;
        const float rowX = static_cast<float>(w) * 0.5F - 220.0F;
        DrawTextEx(font, label, {rowX, rowY}, labelSz, 1.0F, ui::theme::LABEL_TEXT);

        fpsCounterToggleButton_.rect = {toggleBtnX, rowY - 3.0F, toggleBtnW, toggleBtnH};
        fpsCounterToggleButton_.label = resources.settings().showFpsCounter ? "On" : "Off";
        fpsCounterToggleButton_.draw(font, 18.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER,
                                     RAYWHITE, ui::theme::BTN_BORDER);
        (void)labelDim;
    }

    backButton_.draw(font, 22.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
}

} // namespace dreadcast
