#pragma once

#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

class MenuScene final : public Scene {
  public:
    explicit MenuScene(int selectedClassIndex = 0);

    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    ui::Button playButton_{};
    ui::Button settingsButton_{};
    ui::Button quitButton_{};
    ui::Button changeClassButton_{};

    int selectedClassIndex_{0};
};

} // namespace dreadcast
