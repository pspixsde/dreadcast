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

  private:
    Camera2D cam_{};
    Vec2 target_{};
    float lerpSpeed_ = 8.0F;
};

} // namespace dreadcast
