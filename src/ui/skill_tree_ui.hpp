#pragma once

#include <array>

#include <raylib.h>

namespace dreadcast {

class InputManager;
class ResourceManager;

namespace ui {

inline constexpr int kSkillTreeNodeCount = 6;

enum class SkillNodeType { Major, Minor };

struct SkillArchiveEntry {
    const char *name{};
    const char *description{};
    SkillNodeType type{SkillNodeType::Minor};
    const char *iconPath{};
};

[[nodiscard]] const std::array<SkillArchiveEntry, kSkillTreeNodeCount> &skill_tree_archive_entries();

/// Undead Hunter skill tree: 2 major core nodes + 4 minor side nodes, hold E to spend points.
class SkillTreeUI {
  public:
    [[nodiscard]] bool isOpen() const { return open_; }
    void setOpen(bool v) { open_ = v; }
    void toggle() { open_ = !open_; }

    /// Reset learned nodes for a new run (two core nodes stay active).
    void resetProgress();

    [[nodiscard]] const std::array<bool, kSkillTreeNodeCount> &learned() const { return learned_; }

    void update(InputManager &input, int &skillPoints, float frameDt, bool &consumeEscOut,
                bool &flashNoPoints);
    void draw(const Font &font, ResourceManager &resources, int skillPoints,
              float noSkillPointFlashTimer);

  private:
    bool open_{false};
    float holdEProgress_{0.0F};
    int hoverNode_{-1};
    std::array<bool, kSkillTreeNodeCount> learned_{true, true, false, false, false, false};
};

} // namespace ui
} // namespace dreadcast
