#pragma once

#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "scenes/scene_manager.hpp"

namespace dreadcast {

class Game {
  public:
    void run(bool editorMode = false);

  private:
    InputManager input_{};
    ResourceManager resources_{};
    SceneManager scenes_{};
};

} // namespace dreadcast
