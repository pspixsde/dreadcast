#pragma once

#include <array>
#include <string>
#include <vector>

#include <raylib.h>

#include "core/camera.hpp"
#include "core/timer.hpp"
#include "game/map_data.hpp"
#include "scenes/scene.hpp"
#include "ui/button.hpp"

namespace dreadcast {

class EditorScene final : public Scene {
  public:
    void onEnter() override;
    void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                float frameDt) override;
    void draw(ResourceManager &resources) override;

  private:
    enum class Tool { Select, PlaceWall, PlaceLava, PlaceEnemy, PlaceCasket, SetPlayerSpawn,
                      PlaceItem, PlaceSolid, PlaceAnvil };
    enum class EditorTab { Elements, Items, Units };
    enum class SelectedType { None, Wall, Lava, Enemy, Casket, PlayerSpawn, Item, SolidShape,
                              Anvil };

    enum class ResizeHandle { None, Left, Right, Top, Bottom };

    enum class ClipboardKind { None, Wall, Lava, Enemy, Casket, Item };

    struct Selection {
        SelectedType type{SelectedType::None};
        int index{-1};
    };

    [[nodiscard]] Vector2 worldMouseFromScreen(const Vector2 &screenMouse) const;
    [[nodiscard]] Selection pickSelection(const Vector2 &worldMouse) const;
    [[nodiscard]] Rectangle toolbarPanelRect() const;
    /// Hit target for blocking world clicks (includes dropdown column past the panel edge).
    [[nodiscard]] Rectangle editorUiHitRect() const;
    [[nodiscard]] bool isMouseOverEditorUi(Vector2 screenMouse) const;
    [[nodiscard]] ResizeHandle wallHandleAt(const Vector2 &worldMouse, const WallData &w) const;
    [[nodiscard]] ResizeHandle lavaHandleAt(const Vector2 &worldMouse, const LavaData &w) const;
    void drawWorldGrid() const;
    void drawToolbar(const Font &font, Vector2 mouse);
    void drawEditorWorld(const Font &font);
    void drawAltOverlays(const Font &font);
    void drawWallResizeHandles(const Font &font, const WallData &w) const;
    void drawLavaResizeHandles(const Font &font, const LavaData &w) const;
    void handlePlacement(const Vector2 &worldMouse);
    void handleSelectionInput(InputManager &input, const Vector2 &worldMouse);
    void applySelectionMove(const Vector2 &worldMouse);
    void deleteSelection();
    void duplicateSelection();
    void refreshMapFileList();
    void ensureValidMapIndex();
    bool saveMap();
    bool loadMap();
    void newMap();
    [[nodiscard]] bool hasUnsavedChanges() const;
    void syncSavedSnapshot();
    void applyPendingWithoutSaving(SceneManager &scenes);
    void drawUnsavedChangesModal(const Font &font, Vector2 mouse);
    void pushUndoSnapshot();
    void popUndoSnapshot();
    void pasteFromClipboard(const Vector2 &worldMouse);
    void copySelectionToClipboard();
    void drawCasketLootPanel(const Font &font, Vector2 mouse);
    void handleCasketLootPanelClick(const Vector2 &mouse, bool click, bool &uiConsumedClick);
    [[nodiscard]] Rectangle casketLootPanelRect() const;

    GameCamera camera_{};
    FixedStepTimer fixedTimer_{1.0F / 60.0F};
    MapData map_{};

    Tool activeTool_{Tool::Select};
    Selection selected_{};
    bool draggingSelection_{false};
    Vector2 dragOffset_{};

    ResizeHandle resizingWall_{ResizeHandle::None};
    WallData wallResizeStart_{};
    Vector2 wallResizeMouseStart_{};
    ResizeHandle resizingLava_{ResizeHandle::None};
    LavaData lavaResizeStart_{};
    Vector2 lavaResizeMouseStart_{};

    ui::Button saveButton_{};
    ui::Button loadButton_{};
    ui::Button backButton_{};
    ui::Button mapPrevButton_{};
    ui::Button mapNextButton_{};
    ui::Button mapNewButton_{};
    int selectedEnemyType_{0};
    /// Index into item kind table (see `handlePlacement` / `ItemSpawnData::kind`).
    int selectedItemKind_{0};
    EditorTab activeTab_{EditorTab::Elements};

    std::vector<std::string> mapFiles_{};
    int currentMapIndex_{0};

    ClipboardKind clipboardKind_{ClipboardKind::None};
    WallData clipboardWall_{};
    LavaData clipboardLava_{};
    EnemySpawnData clipboardEnemy_{};
    ItemSpawnData clipboardItem_{};
    CasketData clipboardCasket_{};

    /// Vertices for in-progress solid polygon (PlaceSolid tool).
    std::vector<Vector2> solidDraftVerts_{};
    /// While dragging a placed solid, original verts and centroid at grab time.
    std::vector<Vector2> solidMoveSnapshot_{};
    Vector2 solidMoveCenterStart_{0.0F, 0.0F};
    bool solidMoveActive_{false};

    /// World-space segment of the edge we snapped to.
    bool snapGuideActive_{false};
    Vector2 snapGuideA_{0.0F, 0.0F};
    Vector2 snapGuideB_{0.0F, 0.0F};
    /// World-space segment of the dragged edge being aligned.
    bool snapDraggedGuideActive_{false};
    Vector2 snapDraggedGuideA_{0.0F, 0.0F};
    Vector2 snapDraggedGuideB_{0.0F, 0.0F};

    float statusTimer_{0.0F};
    const char *statusText_{"Ready"};

    MapData savedMapSnapshot_{};
    std::array<MapData, 5> undoSnapshots_{};
    int undoSnapshotCount_{0};
    enum class PendingAction { None, Back, PrevMap, NextMap, NewMap };
    PendingAction pendingAction_{PendingAction::None};
    bool showUnsavedDialog_{false};
    ui::Button unsavedSaveButton_{};
    ui::Button unsavedDontSaveButton_{};
    ui::Button unsavedCancelButton_{};

    bool gripActive_{false};
    Vector2 gripWorldAnchor_{0.0F, 0.0F};
};

} // namespace dreadcast
