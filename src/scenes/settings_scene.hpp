#pragma once

#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

/// Settings overlay: Controls tab with keybind reference; Back / Esc returns.
class SettingsScene final : public Scene {
  public:
    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    enum class Tab { Controls, Video };

    ui::Button backButton_{};
    Tab activeTab_{Tab::Controls};

    ui::Button controlsTabButton_{};
    ui::Button videoTabButton_{};
    ui::Button fpsCounterToggleButton_{};
};

} // namespace dreadcast
