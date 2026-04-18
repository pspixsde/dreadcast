#include "game.hpp"

#include <memory>
#include <cstdio>

#include <raylib.h>

#include "config.hpp"
#include "scenes/editor_scene.hpp"
#include "scenes/menu_scene.hpp"

namespace dreadcast {

void Game::run(bool editorMode) {
    InitWindow(config::WINDOW_WIDTH, config::WINDOW_HEIGHT, config::WINDOW_TITLE);
    /// ESC is used for pause / inventory — do not map it to closing the window (X still closes).
    SetExitKey(KEY_NULL);
    SetTargetFPS(config::TARGET_FPS);

    resources_.loadUiFont(config::UI_FONT_PATH, config::UI_FONT_BASE_SIZE);
    HideCursor();

    if (editorMode) {
        scenes_.replace(std::make_unique<EditorScene>());
    } else {
        scenes_.replace(std::make_unique<MenuScene>());
    }

    while (!WindowShouldClose() && !scenes_.shouldQuit()) {
        input_.beginFrame();
        const float dt = GetFrameTime();
        resources_.updateAudio();
        scenes_.update(input_, resources_, dt);

        BeginDrawing();
        ClearBackground(BLACK);
        scenes_.draw(resources_);
        scenes_.drawCursor(resources_);

        if (resources_.settings().showFpsCounter) {
            const Font &font = resources_.uiFont();
            const int w = config::WINDOW_WIDTH;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d FPS", static_cast<int>(GetFPS()));
            const float fontSize = 18.0F;
            const Vector2 dim = MeasureTextEx(font, buf, fontSize, 1.0F);
            const float margin = 10.0F;
            DrawTextEx(font, buf, {w - dim.x - margin, margin}, fontSize, 1.0F, RAYWHITE);
        }
        EndDrawing();
    }

    while (!scenes_.empty()) {
        scenes_.pop();
    }
    CloseWindow();
}

} // namespace dreadcast
