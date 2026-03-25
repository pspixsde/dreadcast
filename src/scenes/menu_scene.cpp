#include "scenes/menu_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "game/character.hpp"
#include "scenes/character_select_scene.hpp"
#include "scenes/gameplay_scene.hpp"
#include "scenes/scene_manager.hpp"
#include "scenes/settings_scene.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

MenuScene::MenuScene(int selectedClassIndex) : selectedClassIndex_(selectedClassIndex) {}

void MenuScene::update(SceneManager &scenes, InputManager &input, ResourceManager & /*resources*/,
                       float /*frameDt*/) {
    if (input.isKeyPressed(KEY_ESCAPE)) {
        scenes.requestQuit();
        return;
    }

    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (changeClassButton_.wasClicked(mouse, click)) {
        scenes.push(std::make_unique<CharacterSelectScene>(&selectedClassIndex_));
        return;
    }

    if (playButton_.wasClicked(mouse, click)) {
        scenes.replace(std::make_unique<GameplayScene>(selectedClassIndex_));
        return;
    }
    if (settingsButton_.wasClicked(mouse, click)) {
        scenes.push(std::make_unique<SettingsScene>());
        return;
    }
    if (quitButton_.wasClicked(mouse, click)) {
        scenes.requestQuit();
    }
}

void MenuScene::draw(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    DrawRectangle(0, 0, w, h, ui::theme::MENU_BG);

    const Font &font = resources.uiFont();
    const float titleSize = 48.0F;
    const char *title = "DREADCAST";
    const Vector2 titleDim = MeasureTextEx(font, title, titleSize, 1.0F);
    DrawTextEx(font, title, {(static_cast<float>(w) - titleDim.x) * 0.5F, h * 0.5F - 220.0F},
               titleSize, 1.0F, RAYWHITE);

    const float panelW = 400.0F;
    const float panelH = 88.0F;
    const float panelX = (static_cast<float>(w) - panelW) * 0.5F;
    const float panelY = h * 0.5F - 100.0F;
    DrawRectangleRec({panelX, panelY, panelW, panelH}, ui::theme::PANEL_FILL);
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 2.0F, ui::theme::PANEL_BORDER);

    const int cix = std::clamp(selectedClassIndex_, 0, CLASS_COUNT - 1);
    const CharacterClass &cls = AVAILABLE_CLASSES[static_cast<size_t>(cix)];

    const float portraitR = 32.0F;
    const float cx = panelX + 24.0F + portraitR;
    const float cy = panelY + panelH * 0.5F;
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), portraitR, ui::theme::PORTRAIT_FILL);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), portraitR, ui::theme::PORTRAIT_RING);
    char oneChar[2] = {cls.name[0], '\0'};
    const float portraitFont = 34.0F;
    const Vector2 idim = MeasureTextEx(font, oneChar, portraitFont, 1.0F);
    DrawTextEx(font, oneChar, {cx - idim.x * 0.5F, cy - idim.y * 0.5F}, portraitFont, 1.0F,
               RAYWHITE);

    const float nameSize = 22.0F;
    const Vector2 nameDim = MeasureTextEx(font, cls.name, nameSize, 1.0F);
    // Keep the class name clear of the portrait circle.
    const float nameX = panelX + portraitR * 2.0F + 36.0F;
    const float nameY = panelY + panelH * 0.5F - nameDim.y * 0.5F;
    DrawTextEx(font, cls.name, {nameX, nameY}, nameSize, 1.0F, RAYWHITE);

    changeClassButton_.rect = {panelX + panelW - 116.0F, panelY + 26.0F, 100.0F, 36.0F};
    changeClassButton_.label = "Change";

    const float btnW = 280.0F;
    const float btnH = 52.0F;
    const float btnX = (static_cast<float>(w) - btnW) * 0.5F;
    const float btnFont = 24.0F;

    playButton_.label = "Play";
    playButton_.rect = {btnX, h * 0.5F + 20.0F, btnW, btnH};

    settingsButton_.rect = {btnX, h * 0.5F + 85.0F, btnW, btnH};
    settingsButton_.label = "Settings";

    quitButton_.rect = {btnX, h * 0.5F + 150.0F, btnW, btnH};
    quitButton_.label = "Quit";

    const Vector2 mouse = GetMousePosition();
    changeClassButton_.draw(font, 18.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                          ui::theme::BTN_BORDER);
    playButton_.draw(font, btnFont, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
    settingsButton_.draw(font, btnFont, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                         ui::theme::BTN_BORDER);
    quitButton_.draw(font, btnFont, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
}

} // namespace dreadcast
