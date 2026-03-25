#include "core/camera.hpp"

#include <algorithm>

#include "config.hpp"

namespace dreadcast {

void GameCamera::init(int screenWidth, int screenHeight) {
    cam_.target = {0.0F, 0.0F};
    cam_.offset = {static_cast<float>(screenWidth) * 0.5F,
                   static_cast<float>(screenHeight) * 0.5F};
    cam_.rotation = 0.0F;
    cam_.zoom = 1.0F;
    target_ = cam_.target;
    lerpSpeed_ = config::CAMERA_FOLLOW_LERP;
}

void GameCamera::update(float dt) {
    const float t = std::min(1.0F, lerpSpeed_ * dt);
    cam_.target.x = Lerp(cam_.target.x, target_.x, t);
    cam_.target.y = Lerp(cam_.target.y, target_.y, t);
}

} // namespace dreadcast
