#include "game.hpp"

#include <string>

int main(int argc, char *argv[]) {
    bool editorMode = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--editor") {
            editorMode = true;
        }
    }
    dreadcast::Game game;
    game.run(editorMode);
    return 0;
}
