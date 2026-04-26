# Dreadcast

Isometric 2D action RPG built with **Raylib**, **EnTT** (ECS), **nlohmann/json** (data-driven characters, abilities, items), and **CMake** (C++17).

## Gameplay

- **Character select:** Choose a class (currently **Undead Hunter**) before starting a run.
- **Movement:** **WASD** or **arrow keys**.
- **Precision combat:** Ranged curse bolt (mana) on **left mouse**, three-hit melee combo on **right mouse** (hold to chain). Virtual crosshair follows aim from the player with configurable mouse sensitivity (**Settings → Controls**).
- **Class abilities (Undead Hunter):** **[1] Lead Fever**, **[2] Deadlight Snare**, **[3] Calamity Slug** — mana costs and cooldowns; lower-right HUD bar with tooltips.
- **Progression:** Enemies grant XP; each level needs **100 XP**. Level-ups grant stat gains (per class JSON) and **skill points** for the skill tree.
- **Skill tree:** **K** opens/closes the panel (or click the left-edge bag shortcut stack). Spend a point by **holding E** on an eligible (non-core, adjacent) node. **Esc** also closes the tree.
- **Full map overlay:** **M** toggles; **Esc** closes while the map is open (inventory must be closed). You can also open it from the left-edge shortcut column.
- **Fog of war:** Wall-aware visibility around the player; soft screen-space edge on the darkness overlay (falls back to a ring if GPU mask/shader init fails). **F3** in gameplay toggles an optional fog debug panel (RT previews + stats).
- **Enemy AI:** Proximity agitation, calm-down, line of sight, knockback and stun states; imp and hellhound types.
- **RPG items:** Equippable armor (e.g. Iron Armor, Barbed Tunic, Runic Shell), stackable vials (Pure Blood HOT, Cordial Manic), rarity tiers, passive regen from class + gear.
- **Crafting station:** **Anvils** on the map (**F** when in range with **Tab** inventory) open a forge / disassemble workbench (panel shifts when active). Forge recipes match by required ingredients/counts regardless of input-slot order.
- **Inventory:** Tabbed bag, equip slots, consumable bar (**C** / **V**), drag-and-drop, Shift+click equip/unequip, tooltips; **Alt** extends tooltip detail on world drops.
- **Map-driven content:** Loads **`assets/maps/default.map`** in normal play (walls, lava, spawn, enemies, items, anvils, casket). Optional **in-game editor** (see below).

## Default map flow

Typical layout on the shipped map:

- Safe start → imp arena → **anvil** workshop spot → **Old Casket** alcove.

**Old Casket** loot comes from the **`CASKET`** line in the map file (on **`default.map`**, several vials). Iron armor, tunics, rings, and other gear are separate **`ITEM`** placements unless you author the map otherwise. Additional kinds and spawns are supported via the editor and **`ITEM`** / **`CASKET`** lines in **`.map`** files.

## Controls

| Action | Input |
|--------|--------|
| Move | **WASD** or **arrow keys** |
| Pause (Resume / Settings / Main Menu) | **Esc** (when no other UI handles it first) |
| Inventory | **Tab** (game keeps simulating; player actions disabled while open) |
| Ranged attack | **Left mouse** |
| Melee combo | **Right mouse** (hold) |
| Abilities | **1** / **2** / **3** |
| Use consumable slots | **C** / **V** |
| Pick up hovered loot | **E** (in range) |
| Interact (casket, anvil, etc.) | **F** (in range) |
| Skill tree | **K** (toggle); **Esc** closes |
| Learn skill node | **Hold E** while hovering an eligible node (costs 1 skill point) |
| Full map | **M** (toggle); **Esc** closes |
| Extended item tooltips (world) | Hold **Alt** |
| Fog debug (gameplay) | **F3** |

While **Tab** inventory is open, **Esc** closes it (and exits anvil workbench mode if it was open).

## Settings

Tabs: **Gameplay**, **Controls**, **Video**, **Audio**, **Credits**. Changes that persist are written to **`settings.cfg`** next to the executable.

- **Gameplay:** Ability mana cost on HUD, damage/heal numbers, reload ring on aim cursor, separate world drops when inventory is full, and bag-priority behavior for shift-returning workbench items (default ON).
- **Controls:** Mouse sensitivity slider, **Reset** (defaults + reinit audio from settings).
- **Video:** FPS counter toggle.
- **Audio:** Master and game volume, output device list.

## Editor mode

From the project root, run the game with **`--editor`** to open the map editor: walls, lava, solids, player spawn, enemies, items, anvils, casket placement, multi-map **Prev / Next**, save/load, selection copy-paste, and camera pan.

## Build & run

CMake uses **FetchContent** for **Raylib 5.5**, **EnTT v3.16.0**, and **nlohmann/json** (v3.11.3).

From the project root (PowerShell example):

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Dreadcast.exe
```

Another generator (e.g. Visual Studio) works if you prefer; with a multi-config generator the executable is often under **`build\Release\`**. Ensure **Release** or **Debug** matches your intent.

The build copies **`assets/`** next to the executable so fonts, maps, data JSON, textures, and cursors resolve at runtime.

## Assets

Shipped under **`assets/`** (copied on build), including:

- **`assets/data/*.json`** — characters, abilities, items
- **`assets/fonts/Cinzel.ttf`** — UI font
- **`assets/maps/*.map`** — level data
- **`assets/textures/`** — items, enemies, UI slot art
- **`assets/cursors/`** — software cursor sprites

## Lore

Non-mechanical flavor notes can go in **`LORE.md`** at the repo root.
