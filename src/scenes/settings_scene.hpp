#pragma once

#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

/// Settings overlay: Gameplay, Controls, Video tabs; Back / Esc returns.
class SettingsScene final : public Scene {
  public:
    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    enum class Tab { Gameplay, Controls, Video };

    ui::Button backButton_{};
    Tab activeTab_{Tab::Gameplay};

    ui::Button gameplayTabButton_{};
    ui::Button controlsTabButton_{};
    ui::Button videoTabButton_{};
    ui::Button fpsCounterToggleButton_{};
    ui::Button manaCostToggleButton_{};
    ui::Button damageNumbersToggleButton_{};
    ui::Button reloadCursorToggleButton_{};
    ui::Button separateWhenFullToggleButton_{};
    ui::Button resetButton_{};

    bool draggingMouseSensSlider_{false};
    bool mouseSensSliderPrevDown_{false};
};

} // namespace dreadcast
