#pragma once

#include "core/types.hpp"

namespace dreadcast {

class GameCamera {
  public:
    void init(int screenWidth, int screenHeight);
    void setFollowTarget(Vec2 worldPosition) { target_ = worldPosition; }

    void update(float dt);

    Camera2D &camera() { return cam_; }
    const Camera2D &camera() const { return cam_; }

    void setLerpSpeed(float s) { lerpSpeed_ = s; }

    /// After mutating `camera().target` directly (e.g. editor grip), keep lerp target in sync.
    void syncFollowFromCamera() {
        target_.x = cam_.target.x;
        target_.y = cam_.target.y;
    }

  private:
    Camera2D cam_{};
    Vec2 target_{};
    float lerpSpeed_ = 8.0F;
};

} // namespace dreadcast
