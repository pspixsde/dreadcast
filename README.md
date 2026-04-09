# Dreadcast

Isometric 2D action RPG built with **Raylib**, **EnTT** (ECS), and **CMake** (C++17).

## Gameplay

- **Character select:** Choose a class (currently **Undead Hunter**) before starting a run.
- **Precision combat:** Ranged curse bolt (mana) on **left mouse**, three-hit melee combo on **right mouse** (hold to chain). Virtual crosshair follows aim from the player with configurable mouse sensitivity (**Settings → Gameplay**).
- **Class abilities (Undead Hunter):** **[1] Lead Fever**, **[2] Deadlight Snare**, **[3] Calamity Slug** — mana costs and cooldowns; lower-right HUD bar with tooltips.
- **Fog of war:** Wall-aware visibility around the player; soft screen-space edge on the darkness overlay (falls back to a ring if GPU mask/shader init fails). **F3** in gameplay toggles an optional fog debug panel (RT previews + stats).
- **Enemy AI:** Proximity agitation, calm-down, line of sight, knockback and stun states; imp and hellhound types.
- **RPG items:** Equippable armor (e.g. Iron Armor, Barbed Tunic, Runic Shell), stackable vials (Pure Blood HOT, Cordial Manic), rarity tiers, passive regen from class + gear.
- **Inventory:** Tabbed bag, equip slots, consumable bar (**C** / **V**), drag-and-drop, Shift+click equip/unequip, tooltips; **Alt** extends tooltip detail on world drops.
- **Map-driven content:** Loads **`assets/maps/default.map`** in normal play (walls, spawn, enemies, items, casket). Optional **in-game editor** (see below).

## Default map flow

Typical layout on the shipped map:

- Safe start → imp arena → **Old Casket** alcove.

Opening the casket still drops **Iron Armor** and **Vial of Pure Blood** (stackable HOT consumable). Additional item kinds and map spawns are supported via the editor and `ITEM` lines in `.map` files.

## Controls

| Action | Input |
|--------|--------|
| Pause / close overlays | **Esc** |
| Inventory | **Tab** (game keeps simulating; player actions disabled while open) |
| Ranged attack | **Left mouse** |
| Melee combo | **Right mouse** (hold) |
| Abilities | **1** / **2** / **3** |
| Use consumable slots | **C** / **V** |
| Pick up hovered loot | **E** (in range) |
| Interact (e.g. casket) | **F** (in range) |
| Extended item tooltips (world) | Hold **Alt** |
| Fog debug (gameplay) | **F3** |

## Settings

- **Gameplay:** Mouse sensitivity, **Save** / **Reset** (writes **`settings.cfg`** next to the executable via the resource layer).
- **Video:** FPS counter toggle.

## Editor mode

From the project root, run the game with **`--editor`** to open the map editor: walls, player spawn, enemies, items, casket placement, multi-map **Prev / Next**, save/load, selection copy-paste, and camera pan.

## Build & run

Dependencies are fetched by CMake (**Raylib 5.5**, **EnTT v3.16.0**).

From the project root (PowerShell example):

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Dreadcast.exe
```

Another generator (e.g. Visual Studio) works if you prefer; ensure **Release** or **Debug** matches your intent.

The build copies **`assets/`** next to the executable so fonts, maps, textures, and cursors resolve at runtime.

## Assets

Shipped under **`assets/`** (copied on build), including:

- **`assets/fonts/Cinzel.ttf`** — UI font
- **`assets/maps/*.map`** — level data
- **`assets/textures/`** — items, enemies, UI slot art
- **`assets/cursors/`** — software cursor sprites

## Lore

Non-mechanical flavor notes can go in **`LORE.md`** at the repo root.
