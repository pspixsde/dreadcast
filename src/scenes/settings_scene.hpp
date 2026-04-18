#pragma once

#include <string>
#include <vector>

#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

/// Settings overlay: Gameplay, Controls, Video, Audio, Credits; Back / Esc returns.
class SettingsScene final : public Scene {
  public:
    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    enum class Tab { Gameplay, Controls, Video, Audio, Credits };

    ui::Button backButton_{};
    Tab activeTab_{Tab::Gameplay};

    ui::Button gameplayTabButton_{};
    ui::Button controlsTabButton_{};
    ui::Button videoTabButton_{};
    ui::Button audioTabButton_{};
    ui::Button creditsTabButton_{};
    ui::Button fpsCounterToggleButton_{};
    ui::Button manaCostToggleButton_{};
    ui::Button damageNumbersToggleButton_{};
    ui::Button reloadCursorToggleButton_{};
    ui::Button separateWhenFullToggleButton_{};
    ui::Button resetButton_{};

    bool draggingMouseSensSlider_{false};
    bool mouseSensSliderPrevDown_{false};

    bool draggingMasterVolSlider_{false};
    bool masterVolSliderPrevDown_{false};
    bool draggingGameVolSlider_{false};
    bool gameVolSliderPrevDown_{false};

    std::vector<std::string> audioDeviceNamesCache_{};
};

} // namespace dreadcast
