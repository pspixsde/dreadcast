#include "game.hpp"

#include <memory>

#include <raylib.h>

#include "config.hpp"
#include "scenes/menu_scene.hpp"

namespace dreadcast {

void Game::run() {
    InitWindow(config::WINDOW_WIDTH, config::WINDOW_HEIGHT, config::WINDOW_TITLE);
    /// ESC is used for pause / inventory — do not map it to closing the window (X still closes).
    SetExitKey(KEY_NULL);
    InitAudioDevice();
    SetTargetFPS(config::TARGET_FPS);

    resources_.loadUiFont(config::UI_FONT_PATH, config::UI_FONT_BASE_SIZE);

    scenes_.replace(std::make_unique<MenuScene>());

    while (!WindowShouldClose() && !scenes_.shouldQuit()) {
        input_.beginFrame();
        const float dt = GetFrameTime();
        scenes_.update(input_, resources_, dt);

        BeginDrawing();
        ClearBackground(BLACK);
        scenes_.draw(resources_);
        EndDrawing();
    }

    while (!scenes_.empty()) {
        scenes_.pop();
    }
    CloseAudioDevice();
    CloseWindow();
}

} // namespace dreadcast
