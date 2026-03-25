#pragma once

#include <raylib.h>

namespace dreadcast {

/// Wraps Raylib input and snapshots mouse state once per frame for stable reads.
class InputManager {
  public:
    void beginFrame();

    bool isKeyHeld(int key) const;
    bool isKeyPressed(int key) const;
    bool isKeyReleased(int key) const;

    bool isMouseButtonHeld(int button) const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonReleased(int button) const;

    Vector2 mousePosition() const { return mousePos_; }
    Vector2 mouseDelta() const { return mouseDelta_; }

  private:
    Vector2 mousePos_{};
    Vector2 mouseDelta_{};
};

} // namespace dreadcast
