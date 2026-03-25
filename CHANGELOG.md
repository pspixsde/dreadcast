# Changelog

All notable changes to **Dreadcast** will be documented in this file.

## v0.4.0 — 2026-03-25

### Added

- **Pause UI:** **PAUSED** title moved higher so it no longer overlaps the **Resume** button.
- **Inventory:** Closing and reopening the inventory clears any stale right-click context menu (`InventoryUI::toggle` / `setOpen`).
- **Combat tuning:** Enemy projectile max travel reduced to **400** (`ENEMY_PROJECTILE_MAX_RANGE`); imp **agitation** — idle until the player enters range (**350**, slightly under shot range), then attack; **5s** outside range returns to idle; **+** shake + red tint + **!** on agitated imps.
- **Map:** `Wall` + `wall_resolve_collisions` — barrier layout with a **safe start**, **imp arena**, and **casket alcove**; walls drawn in `render_system`.
- **Old Casket:** `Interactable` entity — hover + **F** in range opens **Old Casket** and drops **Iron Armor** + **Vile of Pure Blood** (stackable **5**).
- **Items:** `ItemData` buff fields (`maxHpBonus`, `description`, consumable/stacking). **Iron Armor** grants **+10 max HP** when equipped; tooltip on inventory hover and **Alt** extended tooltip on ground drops.
- **Vignette:** Red edge damage flash on HP loss; persistent low-intensity edge red when **HP ≤ 20%** max.
- **Consumables:** **Vile of Pure Blood** — **40 HP** over **8s** via `HealOverTime`; **1** / **2** use consumable slots; HUD placeholder icon + shrinking outline timer above the health bar.
- **Collision helpers:** `find_item_pickup_hover_in_range`, `find_interactable_hover_in_range`; stack merge on pickup for stackable consumables.

### Changed

- **Loot:** Pickup requires **proximity** and **mouse hover** over the drop; imps no longer drop **Iron Armor** after all are killed.
- **Imp placement:** Further from spawn; projectile cap **400**.

### Fixed

- **Inventory:** Context menu no longer persists after reopening without a new right-click.

## v0.3.1 — 2026-03-24

### Added

- **Loot proximity:** `config::LOOT_PICKUP_RANGE` and `find_nearest_item_pickup_in_range` in `collision_system` — nearest ground item within range of the player; **Press [E] to pick up** prompt and pulsing ring highlight in world space.
- **Inventory interactions:** Drag-and-drop between carried and equipment slots; **right-click** context menu on carried items (**Equip**, **Drop**) and equipped items (**Unequip**, **Drop**). **Drop** spawns a pickup at the player’s position (`spawnItemPickupAtPlayer` in `GameplayScene`).
- **Settings:** `src/scenes/settings_scene.hpp` / `.cpp` — **Controls** tab listing movement, aim, attacks, loot, inventory, and pause keys; **Settings** on main menu and pause overlay; **Back** / Esc pops the overlay.

### Changed

- **Movement:** Player speed is normalized in **screen (isometric) space** so vertical and horizontal motion feel equally fast (`input_system.cpp`).
- **Inventory:** Opening inventory no longer freezes the world — simulation continues; player movement, ranged/melee attacks, and **E** loot pickup are disabled while the panel is open.
- **Resolution:** Window size **1920×1080** (`config.hpp`).
- **Main menu:** Removed **Isometric top-down action RPG** subtitle; **character preview** panel (portrait, class name, **Change**); **Play** is a single labeled button (no **Play as …**); **Settings** between **Play** and **Quit**.
- **Pause:** **Settings** button between **Resume** and **Main Menu**.

### Fixed

- **Loot:** Pickup requires the player to be within range of the drop (no mouse-hover requirement).

## v0.3.0 — 2026-03-24

### Added

- **UI theme:** `src/ui/theme.hpp` — demonic magma / ember palette replacing purple-toned UI literals across menus, HUD cluster, pause, inventory, and character select.
- **Character select scene:** `src/scenes/character_select_scene.hpp` / `.cpp` — left grid (portrait + name), right detail panel (description + ability summary); **Choose** commits selection; **Back** / Esc returns without changing. Main menu **Character** opens this screen; **Play** label shows **Play as** the chosen class name.
- **Game over:** On player death, **GAME OVER** overlay with **Enemies slain** count, **Retry** (new run, same class), and **Main Menu** (title with current class preserved). Kill count incremented in `death_system` per destroyed `Enemy`.
- **Consumable slots:** `InventoryState` has **2** consumable slots under the equipped column; UI section **Consumables** (placeholder until consumable items exist).

### Changed

- **Main menu:** Removed inline character carousel and tagline under the class name; narrower flow with **Character**, **Play as …**, **Quit** only.
- **Inventory:** Carried slots **9** in a **3×3** grid; panel **680×420**; equipment + consumable layout as above.
- **Loot:** Item pickups require **mouse hover** over the drop in world space and **E** to pick up; hovered item name tooltip near cursor. Mana shards still pick up on walk-over.
- **Movement:** WASD / arrows map to **screen-up/down/left/right** under isometric projection via `isoToWorld` on the input vector in `input_system.cpp`.
- **Character data:** `CharacterClass::detailAbilities` for the select-screen detail panel.

### Fixed

- Isometric controls: **W** moves visually north instead of north-east (and analogous for **A/S/D**).

## v0.2.0 — 2026-03-24

### Added

- **Isometric presentation:** World logic stays Cartesian; `worldToIso` / `isoToWorld` in `src/core/iso_utils.hpp`; camera follows the player’s isometric screen position; floor grid drawn as isometric lines; mouse aim converts through `isoToWorld` after `GetScreenToWorld2D`.
- **8-directional facing:** `FacingDir` and `Facing` in `ecs/components.hpp`; aim snaps to 8 sectors via `facingFromAngle`; player and enemies update facing each tick.
- **Placeholder rendering:** Entities with `Player`/`Enemy` + `Facing` draw an isometric diamond body and a small directional arrow (textures still supported when present); projectiles and other sprites use a flat iso diamond.
- **Character classes:** `src/game/character.hpp` with **Undead Hunter**; main menu **character panel** with portrait placeholder, name, description, and `<` / `>` (disabled when only one class); `GameplayScene` takes selected class index (for future class-specific rules and HUD portrait initial).
- **HUD v2:** Bottom-left cluster with circular portrait (class initial), thicker health bar and thinner mana bar stacked with small gap, numeric **current / max** centered on each bar, no separate “Health” / “Mana” labels; semi-transparent backing behind the group.

### Changed

- **Enemies:** Imp texture and idle animation removed; **Imp** placeholders use tinted iso diamonds + facing arrow (same behavior: 50 HP, ranged AI, mana shards on death).
- **Player:** Square sprite (**36×36**); projectile spawn distance derived from sprite half-diagonal + `PROJECTILE_RADIUS` + margin; enemy projectile spawn uses the same idea.
- **Inventory:** Equipment slots reduced to **Armor**, **Amulet**, and **Ring**; inventory panel resized (**600×340**); bag still 10 slots.
- **Main menu:** Subtitle updated to **Isometric top-down action RPG**; **Play** passes the selected class into `GameplayScene`.

### Removed

- **Imp art references** from enemy spawning (no `TextureSprite` / `SpriteAnimation` for Imps in code).

## v0.1.0 — 2026-03-22

### Added

- **UI font:** Cinzel variable font (`assets/fonts/Cinzel.ttf`, SIL Open Font License) loaded via `ResourceManager::loadUiFont`; scenes receive `ResourceManager` for `DrawTextEx` throughout.
- **Main menu:** `Play` and `Quit` as hoverable buttons (`src/ui/button.hpp`); Esc still exits from the title screen.
- **HUD:** Health and mana bars (100 / 100) with numeric readouts; flash text for **“Not enough mana”** and **“Inventory full”**.
- **Ranged combat:** LMB fires a player projectile toward the cursor (10 mana per shot, 10 damage, limited range); projectiles spawn slightly ahead of the player to avoid self-collision.
- **Melee combat:** Hold RMB for a visible arc (`MeleeAttacker`), 20 damage per hit on cooldown with knockback and enemy velocity damping.
- **Imps:** Named **Imp** with world-space health bars; 50 HP; face the player; shoot every 2s when player is beyond melee range (enemy projectiles deal 10 damage); on death drop a **mana shard** (+20 mana on pickup).
- **Pause:** Esc toggles pause; dim overlay with **Resume** and **Main Menu** (returns to title).
- **Inventory (Tab):** Equipment slots (helmet, armor, gloves, bracer, two rings, boots) and 10 bag slots; click bag to equip / unequip to first empty bag (`src/ui/inventory_ui.hpp` / `.cpp`); Tab toggles; Esc closes inventory when open.
- **Loot:** After all three Imps are defeated, **Iron Armor** spawns on the ground; walking over it adds it to the bag (same item pool as equipment UI).
- **ECS systems:** `combat_system`, `projectile_system`, `collision_system` (projectiles, pickups), `enemy_ai_system`, `death_system`; `config.hpp` extended with combat constants.
- **Items:** `src/game/items.hpp` — `EquipSlot`, `ItemData`, `InventoryState`.
- **Art (Imp):** Static reference sprite `assets/textures/enemies/imp/imp_base.png` (**128×128**); folder `assets/textures/enemies/imp/animations/` for future animation PNGs (see `animations/README.md`).

### Changed

- **Scene API:** `Scene::update` / `draw` take `ResourceManager &` for shared font and UI.
- **Render:** Enemy name tags and health bars; melee swing arc overlay; `render_system` takes `Font`.
- **Movement:** Enemies apply knockback decay via `ENEMY_VELOCITY_DAMPING`.
- **Build (CMake):** `CMakeLists.txt` checked—configures and links cleanly with **CMake 4.x**, **Ninja**, **Release** (Raylib + EnTT via `FetchContent`; `file(GLOB_RECURSE … CONFIGURE_DEPENDS src/*.cpp)` picks up new sources). **POST_BUILD** copies `assets/` next to the executable so fonts and future assets resolve when running from `build/` output folders. If you switch generators (e.g. Ninja ↔ Visual Studio), use a fresh build directory or clear `CMakeCache.txt`. Optional upstream noise: **CMP0135** on Raylib’s URL fetch (timestamps); harmless or fix with policy / `DOWNLOAD_EXTRACT_TIMESTAMP`.

### Fixed

- **Build:** Removed unused `EquipSlot` variable in `inventory_ui.cpp` and marked unused `ResourceManager &resources` in `GameplayScene::update` so Release builds stay warning-clean with `-Wextra`.
- **Input:** `SetExitKey(KEY_NULL)` so **Esc** no longer closes the window (Raylib’s default). Esc is used for pause / closing the inventory; the window still closes with the **X** button or **Quit** on the main menu.
- **Font:** Bundled `assets/fonts/Cinzel.ttf` (was missing, so Raylib fell back to the default bitmap font). `ResourceManager` resolves the font relative to the **current working directory** or **next to the executable** after the CMake `assets/` copy.

### Removed

- N/A

## v0.0.0 — 2026-03-22

### Added

- **Project bootstrap:** C++17 CMake project named Dreadcast with `FetchContent` for **Raylib 5.5** (URL tarball) and **EnTT v3.16.0** (Git tag), static Raylib build, examples/games disabled.
- **Build / repo hygiene:** `.gitignore` (build artifacts, IDE cruft), `.clang-format` (LLVM-based style), `assets/` placeholders (`textures`, `sounds`, `fonts` with `.gitkeep`).
- **Config:** `src/config.hpp` — window size (1280×720), title, target FPS, fixed timestep (60 Hz), player speed, camera lerp.
- **Core:** `types.hpp` (Vec2 helpers, lerp, angle), `InputManager` (Raylib keyboard/mouse + per-frame mouse snapshot), `GameCamera` (Camera2D, smooth follow), `ResourceManager` (lazy texture/sound cache), `FixedStepTimer` (fixed-step accumulator with per-frame step cap).
- **ECS:** `components.hpp` — `Transform`, `Velocity`, `Sprite`, `Health`, tags `Player` / `Enemy`.
- **Systems:** `input_system` (WASD + mouse aim → player velocity/rotation), `movement_system` (integrate velocity), `render_system` (colored rectangles via `DrawRectanglePro`).
- **Scenes:** abstract `Scene`; `SceneManager` (`replace`, `push`, `pop`, `requestQuit` / `shouldQuit`); `MenuScene` (title, Enter to play, Esc to quit); `GameplayScene` (grid, player + placeholder enemies, HUD, Esc to menu).
- **Application:** `Game` (window, `InitAudioDevice`, `SetTargetFPS`, main loop, scene updates/draw, teardown); `main.cpp` entry point.
- **Git:** repository initialized in the project directory.

### Changed

- N/A (initial release)

### Fixed

- **Build:** `menu_scene.cpp` and `gameplay_scene.cpp` now include `core/input.hpp` so `InputManager` is a complete type when calling `isKeyPressed` (fixes incomplete-type errors with strict toolchains, e.g. Clang).

### Removed

- N/A (initial release)
