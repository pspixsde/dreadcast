#pragma once

namespace dreadcast {

/// Accumulates time for fixed-timestep simulation.
class FixedStepTimer {
  public:
    explicit FixedStepTimer(float fixedDt) : fixedDt_(fixedDt) {}

    void reset() { accumulator_ = 0.0F; }

    /// Call once per frame with frame delta time. Returns how many fixed steps to run.
    int consumeSteps(float frameDt) {
        accumulator_ += frameDt;
        int steps = 0;
        while (accumulator_ >= fixedDt_) {
            accumulator_ -= fixedDt_;
            ++steps;
            // Spiral of death guard: cap steps per frame
            if (steps >= maxStepsPerFrame_) {
                accumulator_ = 0.0F;
                break;
            }
        }
        return steps;
    }

    float fixedDt() const { return fixedDt_; }
    float alpha() const {
        // Interpolation factor for rendering between last fixed state and next
        return accumulator_ / fixedDt_;
    }

    void setMaxStepsPerFrame(int maxSteps) { maxStepsPerFrame_ = maxSteps; }

  private:
    float fixedDt_;
    float accumulator_ = 0.0F;
    int maxStepsPerFrame_ = 5;
};

} // namespace dreadcast
