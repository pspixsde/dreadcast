#include "scenes/editor_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <memory>

#include <raylib.h>

#include "config.hpp"
#include "core/input.hpp"
#include "core/iso_utils.hpp"
#include "core/resource_manager.hpp"
#include "scenes/menu_scene.hpp"
#include "scenes/scene_manager.hpp"
#include "ui/theme.hpp"

namespace dreadcast {

namespace {

constexpr float kEditorPanSpeed = 700.0F;
constexpr float kPickRadius = 30.0F;
constexpr float kDefaultWallHalf = 40.0F;
constexpr float kWallHandlePickWorld = 22.0F;

float distSq(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

Rectangle EditorScene::toolbarPanelRect() const {
    return {4.0F, 4.0F, 210.0F, 520.0F};
}

bool EditorScene::isMouseOverToolbar(Vector2 screenMouse) const {
    return CheckCollisionPointRec(screenMouse, toolbarPanelRect());
}

void EditorScene::refreshMapFileList() {
    mapFiles_.clear();
    std::error_code ec;
    constexpr const char *kDir = "assets/maps";
    std::filesystem::create_directories(kDir, ec);
    for (const auto &ent : std::filesystem::directory_iterator(kDir, ec)) {
        if (!ent.is_regular_file()) {
            continue;
        }
        if (ent.path().extension() == ".map") {
            mapFiles_.push_back(ent.path().stem().string());
        }
    }
    if (mapFiles_.empty()) {
        mapFiles_.push_back("default");
    }
    std::sort(mapFiles_.begin(), mapFiles_.end());
}

void EditorScene::ensureValidMapIndex() {
    if (mapFiles_.empty()) {
        refreshMapFileList();
    }
    if (currentMapIndex_ < 0) {
        currentMapIndex_ = 0;
    }
    if (currentMapIndex_ >= static_cast<int>(mapFiles_.size())) {
        currentMapIndex_ = static_cast<int>(mapFiles_.size()) - 1;
    }
}

EditorScene::ResizeHandle EditorScene::wallHandleAt(const Vector2 &worldMouse,
                                                      const WallData &w) const {
    const Vector2 left{w.cx - w.halfW, w.cy};
    const Vector2 right{w.cx + w.halfW, w.cy};
    const Vector2 top{w.cx, w.cy - w.halfH};
    const Vector2 bottom{w.cx, w.cy + w.halfH};
    const float r2 = kWallHandlePickWorld * kWallHandlePickWorld;
    if (distSq(worldMouse, left) <= r2) {
        return ResizeHandle::Left;
    }
    if (distSq(worldMouse, right) <= r2) {
        return ResizeHandle::Right;
    }
    if (distSq(worldMouse, top) <= r2) {
        return ResizeHandle::Top;
    }
    if (distSq(worldMouse, bottom) <= r2) {
        return ResizeHandle::Bottom;
    }
    return ResizeHandle::None;
}

void EditorScene::onEnter() {
    camera_.init(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    camera_.setLerpSpeed(20.0F);
    refreshMapFileList();
    ensureValidMapIndex();
    map_ = defaultMapData();
    loadMap();

    toolButtons_.clear();
    const char *labels[] = {"Select", "Wall", "Enemy", "Casket", "Player"};
    for (int i = 0; i < 5; ++i) {
        ui::Button b{};
        b.rect = {16.0F, 70.0F + i * 44.0F, 118.0F, 34.0F};
        b.label = labels[i];
        toolButtons_.push_back(b);
    }
    mapPrevButton_.rect = {16.0F, 300.0F, 56.0F, 30.0F};
    mapPrevButton_.label = "<";
    mapNextButton_.rect = {80.0F, 300.0F, 56.0F, 30.0F};
    mapNextButton_.label = ">";
    mapNewButton_.rect = {16.0F, 338.0F, 120.0F, 30.0F};
    mapNewButton_.label = "New map";
    saveButton_.rect = {16.0F, 378.0F, 118.0F, 34.0F};
    saveButton_.label = "Save";
    loadButton_.rect = {16.0F, 422.0F, 118.0F, 34.0F};
    loadButton_.label = "Load";
    backButton_.rect = {16.0F, 466.0F, 118.0F, 34.0F};
    backButton_.label = "Back";
    enemyTypeButton_.rect = {142.0F, 158.0F, 118.0F, 34.0F};
    enemyTypeButton_.label = "Enemy: Imp";
}

Vector2 EditorScene::worldMouseFromScreen(const Vector2 &screenMouse) const {
    const Vector2 isoMouse = GetScreenToWorld2D(screenMouse, camera_.camera());
    return isoToWorld(isoMouse);
}

EditorScene::Selection EditorScene::pickSelection(const Vector2 &worldMouse) const {
    Selection s{};
    const float pickR2 = kPickRadius * kPickRadius;
    if (distSq(worldMouse, map_.playerSpawn) <= pickR2) {
        s.type = SelectedType::PlayerSpawn;
        return s;
    }
    if (map_.hasCasket && distSq(worldMouse, map_.casketPos) <= pickR2) {
        s.type = SelectedType::Casket;
        return s;
    }
    for (int i = 0; i < static_cast<int>(map_.enemies.size()); ++i) {
        const Vector2 e{map_.enemies[static_cast<size_t>(i)].x, map_.enemies[static_cast<size_t>(i)].y};
        if (distSq(worldMouse, e) <= pickR2) {
            s.type = SelectedType::Enemy;
            s.index = i;
            return s;
        }
    }
    for (int i = 0; i < static_cast<int>(map_.walls.size()); ++i) {
        const WallData &w = map_.walls[static_cast<size_t>(i)];
        if (worldMouse.x >= w.cx - w.halfW && worldMouse.x <= w.cx + w.halfW &&
            worldMouse.y >= w.cy - w.halfH && worldMouse.y <= w.cy + w.halfH) {
            s.type = SelectedType::Wall;
            s.index = i;
            return s;
        }
    }
    return s;
}

void EditorScene::handlePlacement(const Vector2 &worldMouse) {
    switch (activeTool_) {
    case Tool::PlaceWall:
        map_.walls.push_back({worldMouse.x, worldMouse.y, kDefaultWallHalf, kDefaultWallHalf});
        selected_.type = SelectedType::Wall;
        selected_.index = static_cast<int>(map_.walls.size()) - 1;
        break;
    case Tool::PlaceEnemy:
        map_.enemies.push_back(
            {worldMouse.x, worldMouse.y, selectedEnemyType_ == 0 ? "imp" : "hellhound"});
        selected_.type = SelectedType::Enemy;
        selected_.index = static_cast<int>(map_.enemies.size()) - 1;
        break;
    case Tool::PlaceCasket:
        map_.hasCasket = true;
        map_.casketPos = worldMouse;
        selected_.type = SelectedType::Casket;
        selected_.index = -1;
        break;
    case Tool::SetPlayerSpawn:
        map_.playerSpawn = worldMouse;
        selected_.type = SelectedType::PlayerSpawn;
        selected_.index = -1;
        break;
    case Tool::Select:
        break;
    }
}

void EditorScene::applySelectionMove(const Vector2 &worldMouse) {
    const Vector2 target{worldMouse.x + dragOffset_.x, worldMouse.y + dragOffset_.y};
    switch (selected_.type) {
    case SelectedType::Wall:
        if (selected_.index >= 0 && selected_.index < static_cast<int>(map_.walls.size())) {
            auto &w = map_.walls[static_cast<size_t>(selected_.index)];
            w.cx = target.x;
            w.cy = target.y;
        }
        break;
    case SelectedType::Enemy:
        if (selected_.index >= 0 && selected_.index < static_cast<int>(map_.enemies.size())) {
            auto &e = map_.enemies[static_cast<size_t>(selected_.index)];
            e.x = target.x;
            e.y = target.y;
        }
        break;
    case SelectedType::Casket:
        map_.casketPos = target;
        map_.hasCasket = true;
        break;
    case SelectedType::PlayerSpawn:
        map_.playerSpawn = target;
        break;
    case SelectedType::None:
        break;
    }
}

void EditorScene::deleteSelection() {
    if (selected_.type == SelectedType::Wall && selected_.index >= 0 &&
        selected_.index < static_cast<int>(map_.walls.size())) {
        map_.walls.erase(map_.walls.begin() + selected_.index);
    } else if (selected_.type == SelectedType::Enemy && selected_.index >= 0 &&
               selected_.index < static_cast<int>(map_.enemies.size())) {
        map_.enemies.erase(map_.enemies.begin() + selected_.index);
    } else if (selected_.type == SelectedType::Casket) {
        map_.hasCasket = false;
    }
    selected_ = {};
}

void EditorScene::duplicateSelection() {
    if (selected_.type == SelectedType::Wall && selected_.index >= 0 &&
        selected_.index < static_cast<int>(map_.walls.size())) {
        WallData copy = map_.walls[static_cast<size_t>(selected_.index)];
        copy.cx += 36.0F;
        copy.cy += 36.0F;
        map_.walls.push_back(copy);
        selected_.index = static_cast<int>(map_.walls.size()) - 1;
    } else if (selected_.type == SelectedType::Enemy && selected_.index >= 0 &&
               selected_.index < static_cast<int>(map_.enemies.size())) {
        EnemySpawnData copy = map_.enemies[static_cast<size_t>(selected_.index)];
        copy.x += 24.0F;
        copy.y += 24.0F;
        map_.enemies.push_back(copy);
        selected_.index = static_cast<int>(map_.enemies.size()) - 1;
    }
}

void EditorScene::copySelectionToClipboard() {
    clipboardKind_ = ClipboardKind::None;
    if (selected_.type == SelectedType::Wall && selected_.index >= 0 &&
        selected_.index < static_cast<int>(map_.walls.size())) {
        clipboardWall_ = map_.walls[static_cast<size_t>(selected_.index)];
        clipboardKind_ = ClipboardKind::Wall;
    } else if (selected_.type == SelectedType::Enemy && selected_.index >= 0 &&
               selected_.index < static_cast<int>(map_.enemies.size())) {
        clipboardEnemy_ = map_.enemies[static_cast<size_t>(selected_.index)];
        clipboardKind_ = ClipboardKind::Enemy;
    } else if (selected_.type == SelectedType::Casket && map_.hasCasket) {
        clipboardCasketPos_ = map_.casketPos;
        clipboardHasCasket_ = true;
        clipboardKind_ = ClipboardKind::Casket;
    }
}

void EditorScene::pasteFromClipboard(const Vector2 &worldMouse) {
    switch (clipboardKind_) {
    case ClipboardKind::Wall: {
        WallData w = clipboardWall_;
        w.cx = worldMouse.x;
        w.cy = worldMouse.y;
        map_.walls.push_back(w);
        selected_.type = SelectedType::Wall;
        selected_.index = static_cast<int>(map_.walls.size()) - 1;
        break;
    }
    case ClipboardKind::Enemy: {
        EnemySpawnData e = clipboardEnemy_;
        e.x = worldMouse.x;
        e.y = worldMouse.y;
        map_.enemies.push_back(e);
        selected_.type = SelectedType::Enemy;
        selected_.index = static_cast<int>(map_.enemies.size()) - 1;
        break;
    }
    case ClipboardKind::Casket:
        map_.hasCasket = true;
        map_.casketPos = worldMouse;
        selected_.type = SelectedType::Casket;
        selected_.index = -1;
        break;
    case ClipboardKind::None:
        break;
    }
}

bool EditorScene::saveMap() {
    ensureValidMapIndex();
    const std::string path = "assets/maps/" + mapFiles_[static_cast<size_t>(currentMapIndex_)] + ".map";
    return map_.saveToFile(path);
}

bool EditorScene::loadMap() {
    ensureValidMapIndex();
    MapData loaded{};
    const std::string path = "assets/maps/" + mapFiles_[static_cast<size_t>(currentMapIndex_)] + ".map";
    if (!loaded.loadFromFile(path)) {
        return false;
    }
    map_ = loaded;
    selected_ = {};
    resizingWall_ = ResizeHandle::None;
    draggingSelection_ = false;
    return true;
}

void EditorScene::newMap() {
    refreshMapFileList();
    int n = 1;
    std::string name;
    for (;;) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "map_%03d", n);
        name = buf;
        if (std::find(mapFiles_.begin(), mapFiles_.end(), name) == mapFiles_.end()) {
            break;
        }
        ++n;
    }
    mapFiles_.push_back(name);
    std::sort(mapFiles_.begin(), mapFiles_.end());
    currentMapIndex_ = static_cast<int>(
        std::find(mapFiles_.begin(), mapFiles_.end(), name) - mapFiles_.begin());
    map_ = defaultMapData();
    selected_ = {};
    resizingWall_ = ResizeHandle::None;
    statusText_ = "New map (unsaved layout)";
    statusTimer_ = 2.0F;
}

void EditorScene::handleSelectionInput(InputManager &input, const Vector2 &worldMouse) {
    const bool leftPressed = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool leftHeld = input.isMouseButtonHeld(MOUSE_BUTTON_LEFT);
    const bool leftReleased = input.isMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if (resizingWall_ != ResizeHandle::None) {
        if (selected_.type == SelectedType::Wall && selected_.index >= 0 &&
            selected_.index < static_cast<int>(map_.walls.size())) {
            auto &w = map_.walls[static_cast<size_t>(selected_.index)];
            const float fixedRight = wallResizeStart_.cx + wallResizeStart_.halfW;
            const float fixedLeft = wallResizeStart_.cx - wallResizeStart_.halfW;
            const float fixedTop = wallResizeStart_.cy - wallResizeStart_.halfH;
            const float fixedBottom = wallResizeStart_.cy + wallResizeStart_.halfH;
            if (resizingWall_ == ResizeHandle::Right) {
                const float newRight = fixedRight + (worldMouse.x - wallResizeMouseStart_.x);
                w.cx = (fixedLeft + newRight) * 0.5F;
                w.halfW = std::max(10.0F, (newRight - fixedLeft) * 0.5F);
            } else if (resizingWall_ == ResizeHandle::Left) {
                const float newLeft = fixedLeft + (worldMouse.x - wallResizeMouseStart_.x);
                w.cx = (newLeft + fixedRight) * 0.5F;
                w.halfW = std::max(10.0F, (fixedRight - newLeft) * 0.5F);
            } else if (resizingWall_ == ResizeHandle::Bottom) {
                const float newBottom = fixedBottom + (worldMouse.y - wallResizeMouseStart_.y);
                w.cy = (fixedTop + newBottom) * 0.5F;
                w.halfH = std::max(10.0F, (newBottom - fixedTop) * 0.5F);
            } else if (resizingWall_ == ResizeHandle::Top) {
                const float newTop = fixedTop + (worldMouse.y - wallResizeMouseStart_.y);
                w.cy = (newTop + fixedBottom) * 0.5F;
                w.halfH = std::max(10.0F, (fixedBottom - newTop) * 0.5F);
            }
        }
        if (leftReleased) {
            resizingWall_ = ResizeHandle::None;
        }
        return;
    }

    if (activeTool_ != Tool::Select) {
        if (leftPressed) {
            handlePlacement(worldMouse);
        }
        return;
    }

    if (leftPressed) {
        if (selected_.type == SelectedType::Wall && selected_.index >= 0 &&
            selected_.index < static_cast<int>(map_.walls.size())) {
            auto &w = map_.walls[static_cast<size_t>(selected_.index)];
            const ResizeHandle h = wallHandleAt(worldMouse, w);
            if (h != ResizeHandle::None) {
                resizingWall_ = h;
                wallResizeStart_ = w;
                wallResizeMouseStart_ = worldMouse;
                draggingSelection_ = false;
                return;
            }
        }

        selected_ = pickSelection(worldMouse);
        if (selected_.type != SelectedType::None) {
            Vector2 center = worldMouse;
            if (selected_.type == SelectedType::Wall) {
                const auto &w = map_.walls[static_cast<size_t>(selected_.index)];
                center = {w.cx, w.cy};
            } else if (selected_.type == SelectedType::Enemy) {
                const auto &e = map_.enemies[static_cast<size_t>(selected_.index)];
                center = {e.x, e.y};
            } else if (selected_.type == SelectedType::Casket) {
                center = map_.casketPos;
            } else if (selected_.type == SelectedType::PlayerSpawn) {
                center = map_.playerSpawn;
            }
            dragOffset_ = {center.x - worldMouse.x, center.y - worldMouse.y};
            draggingSelection_ = true;
        }
    }
    if (leftHeld && draggingSelection_) {
        applySelectionMove(worldMouse);
    }
    if (leftReleased) {
        draggingSelection_ = false;
    }

    if (selected_.type == SelectedType::Wall && selected_.index >= 0 &&
        selected_.index < static_cast<int>(map_.walls.size()) && resizingWall_ == ResizeHandle::None) {
        auto &w = map_.walls[static_cast<size_t>(selected_.index)];
        const float d = IsKeyDown(KEY_LEFT_SHIFT) ? 2.0F : 1.0F;
        if (input.isKeyHeld(KEY_LEFT)) {
            w.halfW = std::max(10.0F, w.halfW - d);
        }
        if (input.isKeyHeld(KEY_RIGHT)) {
            w.halfW += d;
        }
        if (input.isKeyHeld(KEY_UP)) {
            w.halfH += d;
        }
        if (input.isKeyHeld(KEY_DOWN)) {
            w.halfH = std::max(10.0F, w.halfH - d);
        }
    }
}

void EditorScene::drawWallResizeHandles(const Font & /*font*/, const WallData &w) const {
    const Vector2 pts[4] = {worldToIso({w.cx - w.halfW, w.cy}), worldToIso({w.cx + w.halfW, w.cy}),
                            worldToIso({w.cx, w.cy - w.halfH}), worldToIso({w.cx, w.cy + w.halfH})};
    for (const Vector2 &p : pts) {
        DrawCircleV(p, 6.0F, {255, 220, 120, 255});
        DrawCircleLinesV(p, 6.0F, {80, 60, 40, 255});
    }
}

void EditorScene::update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                         float frameDt) {
    statusTimer_ = std::max(0.0F, statusTimer_ - frameDt);
    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool overToolbar = isMouseOverToolbar(mouse);

    for (int i = 0; i < static_cast<int>(toolButtons_.size()); ++i) {
        if (toolButtons_[static_cast<size_t>(i)].wasClicked(mouse, click)) {
            activeTool_ = static_cast<Tool>(i);
        }
    }
    if (mapPrevButton_.wasClicked(mouse, click)) {
        ensureValidMapIndex();
        const int n = static_cast<int>(mapFiles_.size());
        currentMapIndex_ = (currentMapIndex_ - 1 + n) % n;
        const bool ok = loadMap();
        statusText_ = ok ? "Loaded map" : "Load failed";
        statusTimer_ = 2.0F;
    }
    if (mapNextButton_.wasClicked(mouse, click)) {
        ensureValidMapIndex();
        const int n = static_cast<int>(mapFiles_.size());
        currentMapIndex_ = (currentMapIndex_ + 1) % n;
        const bool ok = loadMap();
        statusText_ = ok ? "Loaded map" : "Load failed";
        statusTimer_ = 2.0F;
    }
    if (mapNewButton_.wasClicked(mouse, click)) {
        newMap();
    }
    if (saveButton_.wasClicked(mouse, click)) {
        const bool ok = saveMap();
        ensureValidMapIndex();
        statusText_ = ok ? "Saved map" : "Save failed";
        statusTimer_ = 2.0F;
    }
    if (loadButton_.wasClicked(mouse, click)) {
        const bool ok = loadMap();
        statusText_ = ok ? "Loaded map" : "Load failed";
        statusTimer_ = 2.0F;
    }
    if (backButton_.wasClicked(mouse, click) || input.isKeyPressed(KEY_ESCAPE)) {
        scenes.replace(std::make_unique<MenuScene>());
        return;
    }
    const bool showEnemyType =
        activeTool_ == Tool::PlaceEnemy ||
        (activeTool_ == Tool::Select && selected_.type == SelectedType::Enemy);
    enemyTypeButton_.rect = {toolButtons_[2].rect.x + toolButtons_[2].rect.width + 8.0F,
                             toolButtons_[2].rect.y, 118.0F, 34.0F};
    if (showEnemyType && enemyTypeButton_.wasClicked(mouse, click)) {
        selectedEnemyType_ = (selectedEnemyType_ + 1) % 2;
    }
    enemyTypeButton_.label = selectedEnemyType_ == 0 ? "Enemy: Imp" : "Enemy: Hound";

    Vector2 camMove{0.0F, 0.0F};
    if (input.isKeyHeld(KEY_A)) {
        camMove.x -= 1.0F;
    }
    if (input.isKeyHeld(KEY_D)) {
        camMove.x += 1.0F;
    }
    if (input.isKeyHeld(KEY_W)) {
        camMove.y -= 1.0F;
    }
    if (input.isKeyHeld(KEY_S)) {
        camMove.y += 1.0F;
    }
    if (std::fabs(camMove.x) > 0.001F || std::fabs(camMove.y) > 0.001F) {
        const float len = std::sqrt(camMove.x * camMove.x + camMove.y * camMove.y);
        camMove.x /= len;
        camMove.y /= len;
        camera_.setFollowTarget({camera_.camera().target.x + camMove.x * kEditorPanSpeed * frameDt,
                                 camera_.camera().target.y + camMove.y * kEditorPanSpeed * frameDt});
    }

    if (input.isMouseButtonHeld(MOUSE_BUTTON_MIDDLE)) {
        const Vector2 cart = worldMouseFromScreen(mouse);
        const Vector2 isoGrip = worldToIso(cart);
        const Vector2 screenGrip = GetWorldToScreen2D(isoGrip, camera_.camera());
        const Vector2 d = {mouse.x - screenGrip.x, mouse.y - screenGrip.y};
        camera_.setFollowTarget({camera_.camera().target.x - d.x / camera_.camera().zoom,
                                 camera_.camera().target.y - d.y / camera_.camera().zoom});
    }

    const float wheel = GetMouseWheelMove();
    if (std::fabs(wheel) > 0.001F) {
        camera_.camera().zoom = std::clamp(camera_.camera().zoom + wheel * 0.08F, 0.35F, 2.2F);
    }

    const Vector2 worldMouse = worldMouseFromScreen(mouse);
    if (!overToolbar) {
        handleSelectionInput(input, worldMouse);
    }
    if (input.isKeyPressed(KEY_DELETE)) {
        deleteSelection();
    }
    if ((input.isKeyHeld(KEY_LEFT_CONTROL) || input.isKeyHeld(KEY_RIGHT_CONTROL)) &&
        input.isKeyPressed(KEY_D)) {
        duplicateSelection();
    }
    if ((input.isKeyHeld(KEY_LEFT_CONTROL) || input.isKeyHeld(KEY_RIGHT_CONTROL)) &&
        input.isKeyPressed(KEY_C)) {
        copySelectionToClipboard();
    }
    if ((input.isKeyHeld(KEY_LEFT_CONTROL) || input.isKeyHeld(KEY_RIGHT_CONTROL)) &&
        input.isKeyPressed(KEY_V) && !overToolbar) {
        pasteFromClipboard(worldMouse);
    }

    if (fixedTimer_.consumeSteps(frameDt) > 0) {
        // keep timer in sync
    }
    camera_.update(frameDt);
    (void)resources;
}

void EditorScene::drawWorldGrid() const {
    const int gridExtent = 2800;
    const int step = 64;
    const int tiles = gridExtent / step;
    const Color gridCol = {55, 32, 32, 255};
    for (int i = -tiles; i <= tiles; ++i) {
        const float x = static_cast<float>(i * step);
        DrawLineV(worldToIso({x, static_cast<float>(-gridExtent)}),
                  worldToIso({x, static_cast<float>(gridExtent)}), gridCol);
    }
    for (int j = -tiles; j <= tiles; ++j) {
        const float y = static_cast<float>(j * step);
        DrawLineV(worldToIso({static_cast<float>(-gridExtent), y}),
                  worldToIso({static_cast<float>(gridExtent), y}), gridCol);
    }
}

void EditorScene::drawAltOverlays(const Font &font) {
    const int segments = 56;
    for (const EnemySpawnData &e : map_.enemies) {
        const float agRange = (e.type == "hellhound") ? config::HELLHOUND_AGITATION_RANGE
                                                       : config::IMP_AGITATION_RANGE;
        for (int i = 0; i < segments; ++i) {
            const float a0 = (static_cast<float>(i) / segments) * 2.0F * PI;
            const float a1 = (static_cast<float>(i + 1) / segments) * 2.0F * PI;
            const Vector2 p0 = worldToIso({e.x + std::cosf(a0) * agRange, e.y + std::sinf(a0) * agRange});
            const Vector2 p1 = worldToIso({e.x + std::cosf(a1) * agRange, e.y + std::sinf(a1) * agRange});
            DrawLineV(p0, p1, {255, 110, 80, 170});
        }

        const float size = (e.type == "hellhound") ? config::HELLHOUND_SPRITE_SIZE : config::IMP_SPRITE_SIZE;
        const float half = size * 0.5F;
        const Vector2 b1 = worldToIso({e.x - half, e.y - half});
        const Vector2 b2 = worldToIso({e.x + half, e.y - half});
        const Vector2 b3 = worldToIso({e.x + half, e.y + half});
        const Vector2 b4 = worldToIso({e.x - half, e.y + half});
        DrawLineV(b1, b2, {255, 190, 120, 210});
        DrawLineV(b2, b3, {255, 190, 120, 210});
        DrawLineV(b3, b4, {255, 190, 120, 210});
        DrawLineV(b4, b1, {255, 190, 120, 210});
    }

    const float pHalf = 18.0F;
    const Vector2 p1 = worldToIso({map_.playerSpawn.x - pHalf, map_.playerSpawn.y - pHalf});
    const Vector2 p2 = worldToIso({map_.playerSpawn.x + pHalf, map_.playerSpawn.y - pHalf});
    const Vector2 p3 = worldToIso({map_.playerSpawn.x + pHalf, map_.playerSpawn.y + pHalf});
    const Vector2 p4 = worldToIso({map_.playerSpawn.x - pHalf, map_.playerSpawn.y + pHalf});
    DrawLineV(p1, p2, {120, 210, 255, 210});
    DrawLineV(p2, p3, {120, 210, 255, 210});
    DrawLineV(p3, p4, {120, 210, 255, 210});
    DrawLineV(p4, p1, {120, 210, 255, 210});

    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segments) * 2.0F * PI;
        const float a1 = (static_cast<float>(i + 1) / segments) * 2.0F * PI;
        const Vector2 c0 = worldToIso({map_.playerSpawn.x + std::cosf(a0) * config::FOG_OF_WAR_RADIUS,
                                       map_.playerSpawn.y + std::sinf(a0) * config::FOG_OF_WAR_RADIUS});
        const Vector2 c1 = worldToIso({map_.playerSpawn.x + std::cosf(a1) * config::FOG_OF_WAR_RADIUS,
                                       map_.playerSpawn.y + std::sinf(a1) * config::FOG_OF_WAR_RADIUS});
        DrawLineV(c0, c1, {120, 120, 220, 170});
    }

    DrawTextEx(font, "ALT: unit bounds + aggro + FOW radius", {160.0F, 52.0F}, 15.0F, 1.0F,
               ui::theme::MUTED_TEXT);
}

void EditorScene::drawEditorWorld(const Font &font) {
    for (int i = 0; i < static_cast<int>(map_.walls.size()); ++i) {
        const WallData &w = map_.walls[static_cast<size_t>(i)];
        const Vector2 p1 = worldToIso({w.cx - w.halfW, w.cy - w.halfH});
        const Vector2 p2 = worldToIso({w.cx + w.halfW, w.cy - w.halfH});
        const Vector2 p3 = worldToIso({w.cx + w.halfW, w.cy + w.halfH});
        const Vector2 p4 = worldToIso({w.cx - w.halfW, w.cy + w.halfH});
        const bool sel = selected_.type == SelectedType::Wall && selected_.index == i;
        const Color fill = sel ? Color{90, 70, 65, 255} : Color{55, 48, 42, 255};
        const Color edge = sel ? Color{255, 200, 140, 255} : Color{110, 90, 75, 255};
        DrawTriangle(p1, p2, p3, fill);
        DrawTriangle(p1, p3, p4, fill);
        DrawLineV(p1, p2, edge);
        DrawLineV(p2, p3, edge);
        DrawLineV(p3, p4, edge);
        DrawLineV(p4, p1, edge);
        if (sel) {
            char dimBuf[64];
            std::snprintf(dimBuf, sizeof(dimBuf), "W %.0f H %.0f", static_cast<double>(w.halfW * 2.0F),
                          static_cast<double>(w.halfH * 2.0F));
            DrawTextEx(font, dimBuf, {p1.x, p1.y - 18.0F}, 14.0F, 1.0F, ui::theme::LABEL_TEXT);
            drawWallResizeHandles(font, w);
        }
    }

    for (int i = 0; i < static_cast<int>(map_.enemies.size()); ++i) {
        const EnemySpawnData &e = map_.enemies[static_cast<size_t>(i)];
        const Vector2 iso = worldToIso({e.x, e.y});
        const bool sel = selected_.type == SelectedType::Enemy && selected_.index == i;
        const bool isHound = e.type == "hellhound";
        const float r = sel ? 14.0F : 10.0F;
        DrawCircleV(iso, r, sel ? Color{255, 130, 95, 255}
                                : (isHound ? Color{175, 90, 55, 255} : Color{220, 80, 60, 255}));
        DrawTextEx(font, isHound ? "Hellhound" : "Imp", {iso.x + 8.0F, iso.y - 18.0F}, 14.0F, 1.0F,
                   RAYWHITE);
    }

    const Vector2 pIso = worldToIso(map_.playerSpawn);
    const bool pSel = selected_.type == SelectedType::PlayerSpawn;
    DrawCircleV(pIso, pSel ? 13.0F : 10.0F, pSel ? Color{160, 220, 255, 255} : Color{80, 160, 255, 255});
    DrawTextEx(font, "Player Spawn", {pIso.x + 8.0F, pIso.y - 18.0F}, 14.0F, 1.0F, RAYWHITE);

    if (map_.hasCasket) {
        const Vector2 cIso = worldToIso(map_.casketPos);
        const bool sel = selected_.type == SelectedType::Casket;
        DrawCircleV(cIso, sel ? 14.0F : 11.0F, sel ? Color{190, 150, 105, 255} : Color{130, 95, 55, 255});
        DrawTextEx(font, "Old Casket", {cIso.x + 8.0F, cIso.y - 18.0F}, 14.0F, 1.0F, RAYWHITE);
    }
}

void EditorScene::drawToolbar(const Font &font, Vector2 mouse) {
    const Rectangle panel = toolbarPanelRect();
    DrawRectangleRec(panel, Fade(ui::theme::PANEL_FILL, 248));
    DrawRectangleLinesEx(panel, 2.0F, ui::theme::PANEL_BORDER);
    DrawTextEx(font, "Editor", {16.0F, 18.0F}, 22.0F, 1.0F, RAYWHITE);
    for (int i = 0; i < static_cast<int>(toolButtons_.size()); ++i) {
        const bool active = static_cast<int>(activeTool_) == i;
        toolButtons_[static_cast<size_t>(i)].draw(
            font, 17.0F, mouse, active ? ui::theme::BTN_HOVER : ui::theme::BTN_FILL,
            ui::theme::BTN_HOVER, RAYWHITE, ui::theme::BTN_BORDER);
    }
    mapPrevButton_.draw(font, 16.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                        ui::theme::BTN_BORDER);
    mapNextButton_.draw(font, 16.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                        ui::theme::BTN_BORDER);
    mapNewButton_.draw(font, 15.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                       ui::theme::BTN_BORDER);
    ensureValidMapIndex();
    char mapBuf[80];
    if (!mapFiles_.empty()) {
        std::snprintf(mapBuf, sizeof(mapBuf), "%s",
                      mapFiles_[static_cast<size_t>(currentMapIndex_)].c_str());
    } else {
        mapBuf[0] = '-';
        mapBuf[1] = '\0';
    }
    DrawTextEx(font, mapBuf, {144.0F, 302.0F}, 16.0F, 1.0F, ui::theme::LABEL_TEXT);

    saveButton_.draw(font, 17.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
    const bool showEnemyType =
        activeTool_ == Tool::PlaceEnemy ||
        (activeTool_ == Tool::Select && selected_.type == SelectedType::Enemy);
    if (showEnemyType) {
        enemyTypeButton_.draw(font, 16.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER,
                              RAYWHITE, ui::theme::BTN_BORDER);
    }
    loadButton_.draw(font, 17.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
    backButton_.draw(font, 17.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                     ui::theme::BTN_BORDER);
}

void EditorScene::draw(ResourceManager &resources) {
    DrawRectangle(0, 0, config::WINDOW_WIDTH, config::WINDOW_HEIGHT, ui::theme::CLEAR_BG);
    BeginMode2D(camera_.camera());
    drawWorldGrid();
    drawEditorWorld(resources.uiFont());
    const bool altHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (altHeld) {
        drawAltOverlays(resources.uiFont());
    }
    EndMode2D();

    const Font &font = resources.uiFont();
    const Vector2 mouse = GetMousePosition();
    drawToolbar(font, mouse);

    const Vector2 worldMouse = worldMouseFromScreen(mouse);
    char posBuf[96];
    std::snprintf(posBuf, sizeof(posBuf), "Cursor: %.1f, %.1f", static_cast<double>(worldMouse.x),
                  static_cast<double>(worldMouse.y));
    DrawTextEx(font, posBuf, {16.0F, 520.0F}, 16.0F, 1.0F, ui::theme::LABEL_TEXT);

    char countBuf[96];
    std::snprintf(countBuf, sizeof(countBuf), "Walls: %d  Enemies: %d", static_cast<int>(map_.walls.size()),
                  static_cast<int>(map_.enemies.size()));
    DrawTextEx(font, countBuf, {16.0F, 540.0F}, 16.0F, 1.0F, ui::theme::LABEL_TEXT);

    if (statusTimer_ > 0.0F) {
        DrawTextEx(font, statusText_, {16.0F, 566.0F}, 16.0F, 1.0F, RAYWHITE);
    }

    DrawTextEx(font, "Select: drag | Del remove | Ctrl+D dup | Ctrl+C/V copy-paste", {220.0F, 12.0F},
               16.0F, 1.0F, ui::theme::MUTED_TEXT);
    DrawTextEx(font, "Wall resize: handles / arrows (Shift=faster) | Mid-mouse: camera grip", {220.0F, 32.0F},
               16.0F, 1.0F, ui::theme::MUTED_TEXT);
}

} // namespace dreadcast
