#pragma once

#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

/// Class picker: grid of portraits on the left, details and Choose on the right.
class CharacterSelectScene final : public Scene {
  public:
    explicit CharacterSelectScene(int *selectedClassIndexOut);

    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    int *selectedOut_{nullptr};
    int highlightedIndex_{0};

    ui::Button backButton_{};
    ui::Button chooseButton_{};
};

} // namespace dreadcast
