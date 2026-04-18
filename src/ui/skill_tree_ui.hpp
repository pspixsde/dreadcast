#pragma once

#include <array>

#include <raylib.h>

namespace dreadcast {

class InputManager;
class ResourceManager;

namespace ui {

/// Undead Hunter skill tree placeholder: 2 core nodes + 4 side placeholders, hold E to spend points.
class SkillTreeUI {
  public:
    [[nodiscard]] bool isOpen() const { return open_; }
    void setOpen(bool v) { open_ = v; }
    void toggle() { open_ = !open_; }

    /// Reset learned nodes for a new run (two core nodes stay active).
    void resetProgress();

    void update(InputManager &input, int &skillPoints, float frameDt, bool &consumeEscOut,
                bool &flashNoPoints);
    void draw(const Font &font, ResourceManager &resources, int skillPoints,
              float noSkillPointFlashTimer);

  private:
    bool open_{false};
    float holdEProgress_{0.0F};
    int hoverNode_{-1};
    std::array<bool, 6> learned_{true, true, false, false, false, false};
};

} // namespace ui
} // namespace dreadcast
