#pragma once

#include <entt/entt.hpp>

#include "config.hpp"
#include "core/camera.hpp"
#include "core/timer.hpp"
#include "game/items.hpp"
#include "scenes/scene.hpp"
#include "ui/button.hpp"
#include "ui/inventory_ui.hpp"

namespace dreadcast {

class GameplayScene final : public Scene {
  public:
    explicit GameplayScene(int selectedClassIndex = 0);

    void onEnter() override;
    void onExit() override;

    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    void spawnWorld();
    void spawnWalls();
    void applyPlayerMaxHpFromEquipment();
    void tickHealOverTime(float fixedDt);
    void tryUseConsumableSlot(int slotIndex);
    void tryUseConsumableBagSlot(int bagSlot);
    void applyVialHealOverTime();
    [[nodiscard]] Vector2 worldMouseFromScreen(const Vector2 &screenMouse) const;

    void drawHud(ResourceManager &resources);
    void drawPauseOverlay(ResourceManager &resources);
    void drawFlashMessages(ResourceManager &resources);
    void drawLootProximityPrompt(ResourceManager &resources);
    void drawLootPickupHighlight(ResourceManager &resources);
    void drawGameOverScreen(ResourceManager &resources);
    void drawDamageVignette();

    void spawnItemPickupAtPlayer(int itemIndex);
    void spawnItemPickupAtWorld(const Vector2 &worldPos, int itemIndex);

    entt::registry registry_{};
    GameCamera camera_{};
    FixedStepTimer fixedTimer_{config::FIXED_DT};
    entt::entity player_{};
    bool spawned_{false};

    bool paused_{false};
    bool gameOver_{false};
    ui::InventoryUI inventoryUi_{};
    InventoryState inventory_{};

    ui::Button resumeButton_{};
    ui::Button settingsPauseButton_{};
    ui::Button mainMenuButton_{};
    ui::Button retryButton_{};
    ui::Button gameOverMenuButton_{};

    float noManaFlashTimer_{0.0F};
    float inventoryFullFlashTimer_{0.0F};
    float damageFlashTimer_{0.0F};
    float prevPlayerHp_{100.0F};

    int selectedClass_{0};
    int enemiesSlain_{0};

    entt::entity hoveredPickup_{entt::null};
    entt::entity hoveredInteract_{entt::null};
};

} // namespace dreadcast
