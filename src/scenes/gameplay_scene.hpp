#pragma once

#include <entt/entt.hpp>
#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "core/camera.hpp"
#include "core/timer.hpp"
#include "game/items.hpp"
#include "game/map_data.hpp"
#include "scenes/scene.hpp"
#include "ui/button.hpp"
#include "ui/inventory_ui.hpp"

namespace dreadcast {

struct FloatingNumber {
    Vector2 worldPos{};
    float amount{0.0F};
    bool isHeal{false};
    float lifetime{0.85F};
    float elapsed{0.0F};
    float driftX{0.0F};
};

enum class StatusHudKind { HealOverTime, ManicEffect, LeadFever, ManaRegenOverTime };

struct ActiveStatusHud {
    StatusHudKind kind{StatusHudKind::HealOverTime};
    std::string iconPath{};
    Color outline{WHITE};
};

class GameplayScene final : public Scene {
  public:
    explicit GameplayScene(int selectedClassIndex = 0);
    ~GameplayScene();

    void onEnter() override;
    void onExit() override;

    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;
    void drawCursor(ResourceManager &resources) override;

  private:
    void spawnWorld();
    void spawnWalls(const MapData &map);
    void spawnLavas(const MapData &map);
    void applyPlayerMaxHpFromEquipment();
    void applyPlayerMaxManaFromEquipment();
    void tickHealOverTime(float fixedDt);
    void tickManaRegenOverTime(float fixedDt);
    void tickLavaHazard(float fixedDt);
    void tickManicEffect(float fixedDt);
    void tickRunicShellCooldown(float fixedDt);
    void checkRunicShellTrigger();
    void tryUseConsumableSlot(int slotIndex);
    void tryUseConsumableBagSlot(int bagSlot);
    void applyVialHealOverTime(bool wasAlreadyActive);
    void applyVialRawSpiritMana(bool wasAlreadyActive);
    [[nodiscard]] bool tryApplyCordialManic();
    [[nodiscard]] Vector2 worldMouseFromScreen(const Vector2 &screenMouse) const;

    void syncStatusHudOrder();
    void pushStatusHud(StatusHudKind kind, std::string iconPath, Color outline);
    void removeStatusHudKind(StatusHudKind kind);
    void tryUseAbility(int abilityIndex);
    void tickAbilityCooldowns(float fixedDt);
    void tickLeadFeverEffect(float fixedDt);
    void tickSlugAim(float fixedDt);
    void tickSnareDash(float fixedDt);
    void tickStunnedEnemies(float fixedDt);
    void spawnSnareProjectile(const Vector2 &dirWorld);
    void fireCalamitySlug(const Vector2 &dirWorld);
    [[nodiscard]] Vector2 playerAimDirectionWorld() const;

    void drawHud(ResourceManager &resources);
    void drawPauseOverlay(ResourceManager &resources);
    void drawFlashMessages(ResourceManager &resources);
    void drawLootProximityPrompt(ResourceManager &resources);
    void drawLootPickupHighlight(ResourceManager &resources);
    void drawGameOverScreen(ResourceManager &resources);
    void drawDamageVignette();
    void drawFloatingCombatNumbers(ResourceManager &resources);
    void tickFloatingNumbers(float frameDt);
    void drawFogOfWarLegacy();
    void initFogResources();
    void unloadFogResources();
    void drawWorldContent(ResourceManager &resources);
    void drawFogMaskTexture();
    void drawFogCompositePass();
    void drawFogDebugOverlay(const Font &font);

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
    float hotRefreshFlashTimer_{0.0F};
    float spiritRefreshFlashTimer_{0.0F};
    float lavaDamageAccumulator_{0.0F};
    float runicShellFlashTimer_{0.0F};
    float snareImpactFlash_{0.0F};
    Vector2 snareImpactWorld_{0.0F, 0.0F};

    float abilityCdRem_[3]{0.0F, 0.0F, 0.0F};
    std::vector<ActiveStatusHud> statusHudOrder_{};

    bool hoveringHudElement_{false};
    std::vector<FloatingNumber> floatingNumbers_{};
    std::unordered_map<entt::entity, float> hpBeforeFixedStep_{};

    int selectedClass_{0};
    int enemiesSlain_{0};

    entt::entity hoveredPickup_{entt::null};
    entt::entity hoveredInteract_{entt::null};

    /// Cached from settings each frame for virtual aiming.
    float aimMouseSensitivity_{1.0F};
    Vector2 aimScreenPos_{0.0F, 0.0F};
    bool aimScreenInit_{false};

    RenderTexture2D fogSceneTarget_{};
    RenderTexture2D fogMaskTarget_{};
    Shader fogCompositeShader_{};
    bool fogResourcesReady_{false};
    int fogOverlayLocStrength_{-1};
    int fogOverlayLocEdgeSoft_{-1};
    std::vector<Vector2> fogVisWorld_{};
    std::vector<Vector2> fogVisFan_{};

    /// F3 toggles. When true: RT previews + text (mask black = visible hole, white = fog).
    bool fogDebugOverlay_{false};
    bool fogDbgLegacyPath_{false};
    bool fogDbgMaskSkipped_{false};
    int fogDbgBoundarySamples_{0};
    int fogDbgFanVerts_{0};
};

} // namespace dreadcast
