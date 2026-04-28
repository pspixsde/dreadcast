#pragma once

#include <array>

#include <raylib.h>

#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

class ArchiveScene final : public Scene {
  public:
    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    enum class Tab { Items, Abilities, Skills, Enemies };

    [[nodiscard]] int entryCountForTab() const;
    void clampSelection();
    void cycleTab(int delta);
    [[nodiscard]] Rectangle listPanelRect() const;
    [[nodiscard]] Rectangle detailPanelRect() const;
    [[nodiscard]] Rectangle pickerContentRect() const;
    void drawListPanel(const Font &font, Vector2 mouse);
    void drawDetailPanel(const Font &font, ResourceManager &resources);

    Tab activeTab_{Tab::Items};
    int selectedRow_{0};
    float listScrollY_{0.0F};
    ui::Button backButton_{};
    std::array<ui::Button, 4> tabButtons_{};
};

} // namespace dreadcast
