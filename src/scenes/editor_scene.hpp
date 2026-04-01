#pragma once

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
    enum class Tool { Select, PlaceWall, PlaceEnemy, PlaceCasket, SetPlayerSpawn, PlaceItem };
    enum class SelectedType { None, Wall, Enemy, Casket, PlayerSpawn, Item };

    enum class ResizeHandle { None, Left, Right, Top, Bottom };

    enum class ClipboardKind { None, Wall, Enemy, Casket, Item };

    struct Selection {
        SelectedType type{SelectedType::None};
        int index{-1};
    };

    [[nodiscard]] Vector2 worldMouseFromScreen(const Vector2 &screenMouse) const;
    [[nodiscard]] Selection pickSelection(const Vector2 &worldMouse) const;
    [[nodiscard]] Rectangle toolbarPanelRect() const;
    [[nodiscard]] bool isMouseOverToolbar(Vector2 screenMouse) const;
    [[nodiscard]] ResizeHandle wallHandleAt(const Vector2 &worldMouse, const WallData &w) const;
    void drawWorldGrid() const;
    void drawToolbar(const Font &font, Vector2 mouse);
    void drawEditorWorld(const Font &font);
    void drawAltOverlays(const Font &font);
    void drawWallResizeHandles(const Font &font, const WallData &w) const;
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
    void pasteFromClipboard(const Vector2 &worldMouse);
    void copySelectionToClipboard();

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

    std::vector<ui::Button> toolButtons_{};
    ui::Button saveButton_{};
    ui::Button loadButton_{};
    ui::Button backButton_{};
    ui::Button enemyTypeButton_{};
    ui::Button itemTypeButton_{};
    ui::Button mapPrevButton_{};
    ui::Button mapNextButton_{};
    ui::Button mapNewButton_{};
    int selectedEnemyType_{0};
    /// 0 = iron armor, 1 = vial of pure blood (see `ItemSpawnData::kind`).
    int selectedItemKind_{0};

    std::vector<std::string> mapFiles_{};
    int currentMapIndex_{0};

    ClipboardKind clipboardKind_{ClipboardKind::None};
    WallData clipboardWall_{};
    EnemySpawnData clipboardEnemy_{};
    ItemSpawnData clipboardItem_{};
    Vector2 clipboardCasketPos_{};
    bool clipboardHasCasket_{false};

    float statusTimer_{0.0F};
    const char *statusText_{"Ready"};
};

} // namespace dreadcast
