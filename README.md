# Dreadcast

Isometric 2D action RPG built with **Raylib** (purely code-based).

## Gameplay

- Precision combat: ranged (mana) and melee (right mouse) with cooldowns.
- Enemy behavior is proximity-driven: enemies remain idle until you enter their agitation zone, then start attacking. After you leave for a while, they calm down again.
- RPG build variety via equipment and cursed-style items.
- Layered status effects (currently includes a heal-over-time consumable with a HUD placeholder icon).
- Tightly designed encounters with simple barrier/wall geometry and an interactable casket.

## Current map loop (v0.4.0)

- Safe area (spawn room)
- Imp arena (3 imps)
- Casket room (centered "Old Casket")

The casket drops:
- `Iron Armor` (equip buff: +10 max HP)
- `Vile of Pure Blood` (stackable consumable, max stack 5; heals smoothly over 8 seconds when used)

## Controls

Menu / UI:
- `Esc` = pause overlay / close overlays
- `Tab` = open/close inventory (game simulation keeps running; player actions are disabled while open)

Combat:
- Left mouse button = ranged attack (mana cost)
- Right mouse button (hold) = melee swing/arc (cooldown)

Loot / Interact:
- `E` = pick up hovered ground loot (must be close enough and hovered)
- `F` = open hovered/interactable casket (must be close enough and hovered)
- `1` / `2` = use the consumable in the corresponding consumable slot (if any)
- `ALT` (hold) = extend tooltip on world drops to show item buff effects

## Build & Run

This project uses **CMake** with **FetchContent** for Raylib and EnTT.

Commands (from the project root):

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Dreadcast.exe
```

When building, the `assets/` folder is copied next to the executable so runtime font/asset paths resolve correctly.

## Assets

Current repo includes placeholder assets/folders. The UI font path expects:
- `assets/fonts/Cinzel.ttf`

If you later add real textures/sounds, ensure they are placed under `assets/` so they get copied at build time.

