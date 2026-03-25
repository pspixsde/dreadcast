#include "scenes/settings_scene.hpp"

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "scenes/scene_manager.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

void SettingsScene::update(SceneManager &scenes, InputManager &input,
                           ResourceManager &resources, float /*frameDt*/) {
    if (input.isKeyPressed(KEY_ESCAPE)) {
        scenes.pop();
        return;
    }

    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);

    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;

    const float tabY = 110.0F;
    const float tabW = 180.0F;
    const float tabH = 40.0F;
    const float gap = 10.0F;
    const float tabX = (static_cast<float>(w) - (tabW * 2.0F + gap)) * 0.5F;
    controlsTabButton_.rect = {tabX, tabY, tabW, tabH};
    controlsTabButton_.label = "Controls";
    videoTabButton_.rect = {tabX + tabW + gap, tabY, tabW, tabH};
    videoTabButton_.label = "Video";

    if (click) {
        if (controlsTabButton_.wasClicked(mouse, click)) {
            activeTab_ = Tab::Controls;
        } else if (videoTabButton_.wasClicked(mouse, click)) {
            activeTab_ = Tab::Video;
        }
    }

    if (activeTab_ == Tab::Video) {
        fpsCounterToggleButton_.rect = {static_cast<float>(w) * 0.5F - 160.0F, 250.0F, 320.0F, 48.0F};
        fpsCounterToggleButton_.label =
            resources.settings().showFpsCounter ? "On" : "Off";
        if (fpsCounterToggleButton_.wasClicked(mouse, click)) {
            resources.settings().showFpsCounter = !resources.settings().showFpsCounter;
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
    const float tabW = 180.0F;
    const float tabH = 40.0F;
    const float gap = 10.0F;
    const float tabX = (static_cast<float>(w) - (tabW * 2.0F + gap)) * 0.5F;

    controlsTabButton_.rect = {tabX, tabY, tabW, tabH};
    controlsTabButton_.label = "Controls";
    videoTabButton_.rect = {tabX + tabW + gap, tabY, tabW, tabH};
    videoTabButton_.label = "Video";

    controlsTabButton_.draw(font, 20.0F, mouse,
                             activeTab_ == Tab::Controls ? ui::theme::BTN_HOVER : ui::theme::BTN_FILL,
                             ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);
    videoTabButton_.draw(font, 20.0F, mouse,
                         activeTab_ == Tab::Video ? ui::theme::BTN_HOVER : ui::theme::BTN_FILL,
                         ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);

    if (activeTab_ == Tab::Controls) {
        const float rowLabelX = static_cast<float>(w) * 0.5F - 280.0F;
        const float rowKeyX = static_cast<float>(w) * 0.5F + 40.0F;
        float rowY = 190.0F;
        const float rowGap = 36.0F;
        const float textSize = 20.0F;

        struct Row {
            const char *action;
            const char *key;
        };
        const Row rows[] = {{"Move", "WASD / Arrow keys"},
                            {"Aim", "Mouse"},
                            {"Ranged attack", "Left mouse button"},
                            {"Melee attack", "Right mouse button"},
                            {"Pick up item", "E (near drop, cursor on item)"},
                            {"Interact", "F (near object, cursor on it)"},
                            {"Use consumable slots", "C / V"},
                            {"Item details (world)", "Hold Alt over a drop"},
                            {"Inventory", "Tab"},
                            {"Pause", "Esc"}};

        for (const Row &r : rows) {
            DrawTextEx(font, r.action, {rowLabelX, rowY}, textSize, 1.0F, ui::theme::LABEL_TEXT);
            DrawTextEx(font, r.key, {rowKeyX, rowY}, textSize, 1.0F, RAYWHITE);
            rowY += rowGap;
        }
    } else {
        const float labelSz = 22.0F;
        DrawTextEx(font, "Show FPS Counter",
                   {static_cast<float>(w) * 0.5F - 210.0F, 190.0F}, labelSz, 1.0F,
                   ui::theme::LABEL_TEXT);

        fpsCounterToggleButton_.rect = {static_cast<float>(w) * 0.5F - 160.0F, 250.0F, 320.0F,
                                         48.0F};
        fpsCounterToggleButton_.label = resources.settings().showFpsCounter ? "On" : "Off";
        fpsCounterToggleButton_.draw(font, 22.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER,
                                     RAYWHITE, ui::theme::BTN_BORDER);
    }

    backButton_.draw(font, 22.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
}

} // namespace dreadcast
