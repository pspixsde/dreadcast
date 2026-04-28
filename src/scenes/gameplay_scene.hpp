#pragma once

#include <array>
#include <entt/entt.hpp>
#include <raylib.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.hpp"
#include "core/audio.hpp"
#include "core/camera.hpp"
#include "core/timer.hpp"
#include "game/items.hpp"
#include "game/map_data.hpp"
#include "scenes/scene.hpp"
#include "ui/button.hpp"
#include "ui/inventory_ui.hpp"
#include "ui/skill_tree_ui.hpp"

namespace dreadcast {

struct AbilityDef;

struct FloatingNumber {
    Vector2 worldPos{};
    float amount{0.0F};
    bool isHeal{false};
    /// Ability cooldown refund (cyan); drawn even when damage numbers are hidden.
    bool isManaRestore{false};
    float lifetime{0.85F};
    float elapsed{0.0F};
    float driftX{0.0F};
};

enum class StatusHudKind { HealOverTime, ManicEffect, LeadFever, ManaRegenOverTime, LavaBurn };
enum class StatusSign { Positive, Negative };

struct ActiveStatusHud {
    StatusHudKind kind{StatusHudKind::HealOverTime};
    StatusSign sign{StatusSign::Positive};
    bool persistent{false};
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
    void applyPlayerMoveSpeedFromEquipment();
    void tickVigilantEye(float frameDt);
    void tickHealOverTime(float fixedDt, ResourceManager &resources);
    void tickManaRegenOverTime(float fixedDt);
    void tickLavaHazard(float fixedDt, ResourceManager &resources);
    void tickManicEffect(float fixedDt, ResourceManager &resources);
    void tickRunicShellCooldown(float fixedDt);
    void checkRunicShellTrigger();
    void tryUseConsumableSlot(int slotIndex, ResourceManager &resources);
    void tryUseConsumableBagSlot(int bagSlot, ResourceManager &resources);
    void applyVialHealOverTime(bool wasAlreadyActive, ResourceManager &resources);
    void applyVialRawSpiritMana(bool wasAlreadyActive);
    [[nodiscard]] bool tryApplyCordialManic(ResourceManager &resources);
    [[nodiscard]] Vector2 worldMouseFromScreen(const Vector2 &screenMouse) const;

    void syncStatusHudOrder();
    void pushStatusHud(StatusHudKind kind, std::string iconPath, Color outline,
                       StatusSign sign = StatusSign::Positive, bool persistent = false);
    void removeStatusHudKind(StatusHudKind kind);
    [[nodiscard]] bool tryUseAbility(int abilityIndex);
    void tickAbilityCooldowns(float fixedDt);
    void tickLeadFeverEffect(float fixedDt);
    void tickSlugAim(float fixedDt, ResourceManager &resources);
    void tickSnareDash(float fixedDt);
    void tickStunnedEnemies(float fixedDt);
    void spawnSnareProjectile(const AbilityDef &snareDef, const Vector2 &dirWorld);
    void fireCalamitySlug(const AbilityDef &slugDef, const Vector2 &dirWorld);
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
    void tickChamberState(float fixedDt, ResourceManager &resources);
    void applySkillTreeEffects();
    [[nodiscard]] float lavaAmbientLoudnessFromPlayer() const;
    void drawChamberPelletArc(float portraitCx, float portraitCy, float portraitR);
    void drawMinimapOverlay(bool fullScreen, ResourceManager &resources);
    void tickAnvilUi(InputManager &input, ResourceManager &resources, const ui::AnvilUiLayout &layout);
    void drawAnvilUi(ResourceManager &resources, const ui::AnvilUiLayout &layout);
    void resetAnvilWorkbench();
    [[nodiscard]] ui::AnvilUiLayout buildAnvilUiLayout() const;
    void handleInventoryAnvilAction(const ui::InventoryAction &action);
    void applyAnvilForgeSlotPlace(int slot, int poolIdx);
    bool tryReturnPoolItemToBagOrDrop(int poolIdx);
    void clearDisassembleOutputPool();
    void commitDisassembleRecipe();
    void drawFogOfWarLegacy();
    void initFogResources();
    void unloadFogResources();
    void drawWorldContent(ResourceManager &resources);
    void drawFogMaskTexture();
    void drawFogCompositePass();
    void drawFogDebugOverlay(const Font &font);

    void spawnItemPickupAtPlayer(int itemIndex);
    void spawnItemPickupAtWorld(const Vector2 &worldPos, int itemIndex);

    /// Wraps `InventoryState::removeItemAtIndex` (swap-with-last) and rewrites every other
    /// place a pool index can live: ground `ItemPickup`s, `forgeSlots_`, `disassembleInputIndex_`,
    /// `disassembleOutputPool_`. All callers must use this helper instead of removing directly.
    void removeInventoryItemAndRewrite(int idx);

    entt::registry registry_{};
    GameCamera camera_{};
    FixedStepTimer fixedTimer_{config::FIXED_DT};
    entt::entity player_{};
    bool spawned_{false};

    bool paused_{false};
    bool gameOver_{false};
    ui::InventoryUI inventoryUi_{};
    ui::SkillTreeUI skillTreeUi_{};
    InventoryState inventory_{};

    ui::Button resumeButton_{};
    ui::Button archivePauseButton_{};
    ui::Button settingsPauseButton_{};
    ui::Button mainMenuButton_{};
    ui::Button retryButton_{};
    ui::Button gameOverMenuButton_{};

    float noManaFlashTimer_{0.0F};
    float inventoryFullFlashTimer_{0.0F};
    float damageFlashTimer_{0.0F};
    float hurtGruntCooldown_{0.0F};
    float prevPlayerHp_{100.0F};
    float hotRefreshFlashTimer_{0.0F};
    float spiritRefreshFlashTimer_{0.0F};
    float lavaDamageAccumulator_{0.0F};
    bool inLavaPrev_{false};
    float runicShellFlashTimer_{0.0F};
    float runicShellFinishFlash_{0.0F};
    float snareImpactFlash_{0.0F};
    Vector2 snareImpactWorld_{0.0F, 0.0F};
    float skillPointFlashTimer_{0.0F};

    float abilityCdRem_[3]{0.0F, 0.0F, 0.0F};
    float abilityFinishFlash_[3]{0.0F, 0.0F, 0.0F};
    std::vector<ActiveStatusHud> statusHudOrder_{};

    bool hoveringHudElement_{false};
    std::vector<FloatingNumber> floatingNumbers_{};
    std::unordered_map<entt::entity, float> hpBeforeFixedStep_{};
    std::unordered_map<entt::entity, bool> hellhoundPrevAgitated_{};
    std::unordered_set<entt::entity> deathSfxPlayed_{};

    MapData loadedMap_{};
    bool fullMapOpen_{false};
    /// Corner minimap only: world-space center of the panned view (deadzone follow).
    float minimapFollowCamX_{0.0F};
    float minimapFollowCamY_{0.0F};
    bool minimapFollowCamInitialized_{false};

    bool anvilOpen_{false};
    entt::entity activeAnvil_{entt::null};
    int anvilTab_{0}; // 0 Forge, 1 Disassemble
    /// Pool indices placed in forge (-1 empty); up to 6 cells for future recipes.
    std::array<int, 6> forgeSlots_{};
    int disassembleInputIndex_{-1};
    /// Pending disassemble outputs (pool indices, not in bag) until dragged to inventory.
    std::array<int, 6> disassembleOutputPool_{};
    int disassembleOutputCount_{0};

    enum class AnvilBenchDragKind { None, ForgeSlot, DisOut };
    AnvilBenchDragKind anvilBenchDragKind_{AnvilBenchDragKind::None};
    int anvilBenchForgeSlot_{-1};
    int anvilBenchDisOutSlot_{-1};
    int anvilBenchDragPoolIdx_{-1};

    int selectedClass_{0};
    int enemiesSlain_{0};

    entt::entity hoveredPickup_{entt::null};
    entt::entity hoveredInteract_{entt::null};

    /// Cached from settings each frame for virtual aiming.
    float aimMouseSensitivity_{1.0F};
    bool bagPriorityShiftIntoInventory_{false};
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

    float lavaAudioDbgLastLoudness_{0.0F};
    float lavaAudioDbgLastTargetVol_{0.0F};
    SoundHandle lavaAudioDbgLastHandle_{-1};
    bool lavaAudioDbgLastPlaying_{false};
};

} // namespace dreadcast
