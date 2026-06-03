# Dreadcast — Development State Report

**As of:** June 2026  
**Build lineage:** v0.12.7 (see `CHANGELOG.md` for version-by-version notes)  
**Audience:** Product, design, and engineering — what the game *is* today, not a full content bible.

---

## Summary

**Dreadcast** is a single-player, isometric 2D action RPG prototype set in **the Dread Pit** — a harsh underworld framed as a hell-like prison. The player controls the **Undead Hunter**, fights **Imps**, **Hellhounds**, **Wardens**, and **Dreg** swarms (released by stationary **Node** spawners) on a hand-authored map, loots and equips gear, uses vials and abilities, gains levels and skill points, and can craft at **anvils**. There is **no campaign progression across maps**, **no save slots for runs**, **no narrative scripting** (cutscenes, dialogue, quests), and **no multiplayer**.

What exists is a **tight vertical slice**: one playable class, one primary gameplay map (`default.map`), a full combat/inventory/progression loop, rich UI, map editor, and an in-game **Archive** codex. Content is largely **data-driven** (JSON) with a C++ / Raylib / ECS runtime.

---

## Technology & engineering maturity

| Area | State |
|------|--------|
| **Engine** | Custom C++17, **Raylib 5.5**, **EnTT** ECS, **nlohmann/json**, **CMake** + FetchContent |
| **Simulation** | Fixed **60 Hz** tick (`config::FIXED_DT`) for combat/physics; render at target 60 FPS |
| **Resolution** | **1920×1080** window; software cursor (OS cursor hidden) |
| **Audio** | **miniaudio** backend (Raylib raudio disabled); master/game volume + device selection |
| **Persistence** | **`settings.cfg`** only (settings, not progress). Maps saved via **editor** to `assets/maps/*.map` |
| **Platforms** | Windows-focused dev path (PowerShell build notes in `README.md`); no shipped console/mobile targets documented |

The codebase is actively refactored toward **data-driven items/recipes**, a unified **`item_transaction`** inventory layer, **`PlayerEquipmentSnapshot`** for combat stats, and **`combat_resolution`** for damage/heal. Debug self-tests exist for inventory transactions (dev builds).

---

## Player journey (what you can do today)

### Front-end flow

1. **Main menu** — Play, **Archive** (codex), **Settings**, **Change class**, Quit.  
2. **Character select** — One class: **Undead Hunter** (stats, bio, attack/mobility readouts, ability summary).  
3. **Gameplay** — Single arena loaded from **`assets/maps/default.map`** (fallback to embedded default if file missing).  
4. **Pause** — Resume, Archive, Settings, Main Menu (**Esc**; inventory/anvil/map/skill tree consume Esc first).  
5. **Game over** — Enemy kill count, **Retry** (fresh run, same class), **Main Menu**.

There is **no continue-from-save**, **no multiple save slots**, and **no meta-progression** between runs beyond what the player remembers and what is in `settings.cfg`.

### Core loop (customer view)

Explore a fog-shrouded isometric arena → fight enemies → pick up loot → equip and drink vials → spend XP on levels and skill points → optionally forge/disassemble at an anvil → die or quit. **Retry** resets the world, inventory, skill tree progress, and run state.

---

## Combat & controls

### Movement & camera

- **WASD / arrows** move the player; camera follows with smoothing.  
- **Virtual crosshair** (aim cursor) offset from the player; sensitivity in **Settings → Controls**.

### Basic attacks

| Input | Behavior |
|-------|----------|
| **Left mouse** | **Curse bolt** — ranged projectile; uses **6-round chamber** and reload rhythm (not per-shot mana). Base tuning from class JSON + skill minors. |
| **Right mouse (hold)** | **Three-hit melee combo** — frontal cone, knockback, hold to chain; blocked while certain ability phases run. |

Chamber: **6 shots**, **2 s** active reload after emptying mag, **3 s** idle full refill; HUD pellet arc + optional reload ring on aim cursor (setting).

### Class abilities (Undead Hunter)

Loaded from **`assets/data/abilities.json`**; keys **1 / 2 / 3**; HUD with cooldowns, mana cost display (toggle), tooltips.

| Ability | Role |
|---------|------|
| **Lead Fever** | Timed scatter-shot mode (4 pellets, knockback, full pellet damage). |
| **Deadlight Snare** | Thrown net, backward dash, pull + **stun** on hit. |
| **Calamity Slug** | **1 s** channel, then high-damage piercing shot with sideways knockback. |

Mana costs and cooldowns are data-tuned. Equipment (e.g. **Pulse Link** ring) can modify ability mana cost and grant mana on cooldown finish.

### Enemies

| Type | Role |
|------|------|
| **Imp** | Ranged kiter (`ranged_kiter`) — curse bolts, preferred distance, panic band near melee, agitation + LOS. **50 HP**, **25 XP**. |
| **Hellhound** | Melee chaser (`melee_chaser`) — high speed, bite in range. **60 HP**, **30 XP**. |
| **Warden** | Mid-range bruiser (`mid_bruiser`) — telegraphed **line slam** (35 dmg) + close-range **shockwave** (knockback + 20% slow, 10s cd). **120 HP**, **50 XP**. |
| **Dreg** | Fragile, fast melee **swarmer** (`dreg_swarmer`) — surrounds the player in numbers. **10 HP**, **6 dmg**, **0 XP**. |
| **Node** | Stationary **`Immovable`** Dreg **spawner** — dormant until the player enters; bursts 8 Dregs (temporary speed buff) then drips 3 every 3s; returns to dormant + self-heals after the player leaves for 6s. **150 HP**, **65 XP**. |

Enemy tuning is **data-driven** in `assets/data/enemies.json` (`EnemyArchetype` catalog with a `behavior` field); the spawner fills `ecs::EnemyAI` / `ecs::WardenTuning` and both `enemy_ai_system` and the editor read the catalog.

**AI highlights:** Agitation/calm-down timers, line-of-sight, last-known position, knockback/stun freezes AI briefly, stuck-on-wall slide logic, damage permanently widens aggro range, death SFX and (hellhound) agro SFX.

**Fog of war:** Wall-aware visibility radius; enemies/projectiles hidden without LOS; soft-edged darkness (shader path with CPU fallback). Equippable **vision** bonuses (e.g. **Vigilant Eye** stillness bonus).

### Hazards & environment

- **Walls** — block movement and shots.  
- **Lava** — walkable but **slows movement** and deals **environmental damage** over time; **Lava Burn** status; proximity loop + tick hiss SFX.  
- **Solid polygons** — concave collision, fog LOS, rendered in world.  
- **Environmental damage** routes through **`combat_resolution`** (reflect, on-hit item triggers where wired).

### Death & feedback

- Game over at **0 HP** (lava and lethal damage respect regen ordering so kills stick).  
- Optional floating damage/heal numbers; vignette flash on hit; low-HP edge tint.  
- Undead Hunter hurt/death SFX with hurt cooldown.

---

## Progression

### XP & leveling

- Enemies grant **XP on death** (no mana shards or other drops).  
- Level threshold: **100 XP** per level (current tuning).  
- Level-up: increased max HP/mana, melee damage, ranged damage per class JSON; **+1 skill point** each level.  
- HUD: level + XP ring on portrait; enemy **level disc** beside HP bar (display level).

### Skill tree (**K**)

Undead Hunter tree: **2 major** nodes (pre-active at run start) + **4 minor** nodes (spend points by **holding E** on eligible adjacent nodes).

| Node | Implementation status |
|------|------------------------|
| **Heavy Round** | +2 ranged damage — **wired** |
| **Hot Lead** | +40 projectile speed — **wired** |
| **Long Reach** | +8 melee range — **wired** |
| **Heavy Strike** | +5 melee damage — **wired** |
| **Bottomless Chamber** | Described as infinite free basic shots — **UI/lore only; not wired to chamber/mana** |
| **Last Hand** | Described as melee fallback when out of ammo/mana — **not wired** |

Major nodes are **marked learned** at run start but their described mechanics are **placeholders** relative to minors.

---

## Items, inventory & crafting

### Inventory model

- **9 bag slots**, **3 equip slots** (armor, amulet, ring), **2 consumable bar slots** (**C** / **V**).  
- Drag-and-drop, Shift+click equip/unequip, stack merge/split, context menus, rarity-colored slots and tooltips, **(i)** rarity reference panel.  
- World pickups: hover + **E** in range; **F** for interactables; **Alt** for extended ground tooltips.  
- While inventory is open, simulation continues but **player combat/actions are blocked**.

### Item catalog (shipped JSON)

**10 equippables/consumables** in `assets/data/items.json`, including:

- **Armor:** Iron Armor, Barbed Tunic, Runic Shell (proc below 30% HP).  
- **Consumables:** Pure Blood (HoT), Cordial Manic (speed/invuln/drain gate), Raw Spirit (mana over time).  
- **Ring:** Hollow Ring, Pulse Link.  
- **Amulet:** Frayed Amulet (move speed), Vigilant Eye (fog vision + stillness bonus).

Items support legacy numeric stats and optional declarative **`effects[]`** (triggers/actions parsed at load with warnings for unknown fields). Active vial/gear effects tick through **`ActiveItemEffects`** (HoT, manic, runic cooldown overlay, etc.).

**Rarity** tiers separate gear (Tarnished → Abyssal) and consumables (Clouded → Special) with stack caps by consumable tier.

### World loot & interactables

- **`ITEM`** map lines spawn ground pickups at load.  
- **`CASKET`** — **F** opens; drops a **ring of pickups**. Contents are **rolled per session** from the casket's **tier** (`old`/`sealed`/`wrought`), not authored in the editor — see `rollCasketLoot`.  
- **`ANVIL`** — **F** opens forge/disassemble UI tied to inventory.

### Crafting

- Recipes in **`assets/data/recipes.json`** (schema v1).  
- Shipped: **forge** Pure Blood + Iron Armor → Barbed Tunic; **disassemble** reverses.  
- Matching is **order-agnostic** across forge inputs; cross-cell ingredient aggregation; first matching recipe wins.  
- Recipe **conditions** (e.g. `playerLevelAtLeast`) and **sideEffects** parse from JSON but unimplemented kinds **log warnings** only.

### Transaction layer

Inventory mutations (bag, equip, consumable, anvil bench, world drop/pickup) go through **`item_transaction`** with consistent pool-index rewriting for ground items and anvil drag.

---

## World & content scope

### Maps

- **Gameplay:** `assets/maps/default.map` — walled arena with lava pools, solid polygons, a **Warden**, a Dreg-spawning **Node**, scattered items, one **anvil**, and three caskets (**Old / Sealed / Wrought**). Layout matches README “safe start → arena → workshop → casket alcove” flow.  
- **Editor:** Additional maps (e.g. `map_001.map`); **Prev/Next**, new map, save/load, dirty prompt on exit.

### Narrative & setting (design vs build)

`LORE.md` drafts **Nine Arenas** of the Dread Pit, **Vareth**, and Arena II wind hazards. **In-game:**

- No dialogue, VO, or quest system.  
- No Vareth encounter or Arena II wind mechanic.  
- Play space is effectively **one grey-threshold-style arena**, not a multi-arena campaign.

### Archive (codex)

From menu or pause: read-only **Items / Abilities / Skills / Enemies** tabs with icons, stats blurbs, and lore snippets where authored. The **Enemies** tab covers **Imp, Hellhound, Warden, Dreg, and Node**. Does not unlock with gameplay; it mirrors catalog data.

---

## UI & accessibility of features

| Feature | Notes |
|---------|--------|
| **HUD** | Portrait, HP/mana, regen hint, XP ring, chamber pellets, equipment strip, consumables, abilities, status effect strip (timers, positive/negative styling, Lava Burn). |
| **Shortcuts** | Left-edge **Tab / K / M** buttons with key art. |
| **Full map** | **M** — walls, lava, solids, player, fog-visible enemies, anvil, casket. |
| **Settings** | Gameplay, Controls, Video, Audio, Credits; live persist to `settings.cfg`. |
| **Debug** | **F3** fog (and lava audio) debug overlay in gameplay. |

---

## Audio (current)

SFX grouped under `assets/sounds/` by category (**ambient**, **abilities**, **combat**, **characters**, **enemies**, **pickups**, **items**). Notable behaviors:

- Contextual pickup sounds by item category.  
- Ability-specific throws/impacts/scatter fire.  
- Cordial manic heartbeat (exclusive, pitch/duration tied to effect).  
- Pure Blood looping vial sound with fade.  
- Lava proximity loop + damage hiss; stops on pause/game over/scene exit.

---

## Map editor (`--editor`)

Separate entry: **no** main-menu play loop. Supports walls, lava, **solid polygons**, player spawn, enemies (imp/hellhound/warden/dreg) under the **Units** tab, Dreg-spawning **Nodes** under **Elements**, **all catalog items**, anvils, casket **tier** selection (loot rolled at runtime), camera pan/grip, copy-paste, undo ring, magnetic snap, unsaved-changes modal, multi-map navigation. Authoring tool is **production-ready for level data**; it is not a narrative or quest editor.

---

## Data & content pipeline

| Asset | Purpose |
|-------|---------|
| `assets/data/characters.json` | Playable classes (1 shipped). |
| `assets/data/abilities.json` | Per-class ability bar (3 for Undead Hunter). |
| `assets/data/items.json` | Full item catalog (**required** at startup). |
| `assets/data/enemies.json` | Enemy archetype catalog + AI globals (**required** at startup). |
| `assets/data/recipes.json` | Forge/disassemble recipes. |
| `assets/maps/*.map` | Geometry, spawns, interactables. |
| `assets/textures/` | Items, skills, abilities, UI, enemies. |
| `docs/content_schema.md` | Authoring contracts for JSON and combat APIs. |

Loaders warn on schema mismatch, unknown effect keys, and bad recipe item references. Empty/missing items file **fails startup**.

---

## What is not in the game (explicit gaps)

Useful for scoping expectations:

| Missing / not present | Notes |
|------------------------|--------|
| **Second playable class** | Character pipeline supports multiple JSON entries; only Undead Hunter is shipped. |
| **Campaign / world map** | Single combat map per run; no travel between arenas. |
| **Story delivery** | No quests, NPCs, dialogue UI, or scripted beats (incl. Vareth). |
| **Run persistence** | Retry wipes progress; no inventory/XP save. |
| **Multiplayer / co-op** | — |
| **Shops, currencies** | No economy yet. |
| **Full skill tree fantasy** | Two major nodes lack gameplay hooks. |
| **Advanced recipe features** | Some condition/sideEffect kinds stubbed. |
| **Arena II wind hazard** | Lore only. |
| **Tutorial/onboarding** | Player learns by playing; no guided tutorial sequence. |
| **Achievements, cloud saves, localization** | — |

Work-in-progress assets may exist in the tree (e.g. icons not yet in `items.json`) without being playable.

---

## Quality & polish level

**Strong for a prototype slice:**

- Cohesive HUD, inventory, anvil, skill tree, pause, settings, archive.  
- Fog, lava ambience, item procs, ability VFX/SFX, knockback/stun feel.  
- Editor and JSON authoring path with validation warnings.

**Still early-product:**

- One class, one primary map, no narrative shell, no meta-progression.  
- Balance and content breadth are demo-scale (10 items, 4 enemy types + Node spawner, 1 forge loop).  
- Some UI copy promises mechanics not yet implemented (skill majors).

---

## How this document stays accurate

- **Mechanics:** `README.md`, `CHANGELOG.md`, `src/`, `assets/data/`.  
- **Flavor / long-term vision:** `LORE.md` (non-authoritative for what ships).  
- **Version:** Latest section at top of `CHANGELOG.md`.

When shipping milestones, update this file’s **As of** date and summary tables—not every changelog entry.
