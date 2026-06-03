# Content authoring contracts

This project loads gameplay data from JSON under `assets/data/`. The runtime validates **schema versions** and logs unknown or inconsistent fields during development builds.

## Items (`assets/data/items.json`)

| Field | Notes |
|--------|--------|
| `schemaVersion` | **Preferred.** Must be `1` for this loader revision. Legacy top-level `"version"` (integer) is accepted as an alias. |
| `items[]` | Each entry builds an [`ItemData`](../src/game/items.hpp) row keyed by **`id`** (stored as **`catalogId`** at runtime). |

**Effects:** Optional `effects[]` objects accept only the keys **`trigger`**, **`condition`**, **`action`**. Trigger and action `kind` strings must match parsers in [`game_data.cpp`](../src/game/game_data.cpp) (`itemEffectTriggerFromString`, `itemEffectActionKindFromString`). If `effects[]` is empty, numeric legacy fields are synthesized into effects.

## Enemies (`assets/data/enemies.json`)

| Field | Notes |
|--------|--------|
| `schemaVersion` | Must be `1`. Missing/invalid file fails `loadGameData()` (no embedded fallback). |
| `globals` | Optional shared AI knobs: `steerRate`, `seekArriveRadius`, `stuckThreshold`, `stuckMinDisp`. |
| `enemies[]` | Each row is an [`EnemyArchetype`](../src/game/enemy_archetype.hpp) keyed by **`id`**. Map **`ENEMY x y type`** uses this id (e.g. `imp`, `hellhound`, `warden`). |

**Per-enemy fields:** `displayName`, `behavior` (`ranged_kiter` / `melee_chaser` / `mid_bruiser` / `dreg_swarmer`), `tint` `[r,g,b,a]`, `spriteSize`, `hp`, `xp`, `agitationRange`, `calmDownDelay`, plus one tuning block:

- **`ranged`** — for `ranged_kiter`: shoot range/cooldown, movement bands, projectile damage/speed/range.
- **`melee`** — for `melee_chaser` **and `dreg_swarmer`**: `chaseSpeed`, `meleeDamage`, `meleeRange`, `meleeCooldown`.
- **`bruiser`** — for `mid_bruiser`: positioning, line-slam, and close-range push ability numbers.

`dreg_swarmer` reuses the `melee` chase/attack loop but is meant for fragile swarm units (e.g. the **Dreg**) that mob and surround the player; Node-spawned Dregs additionally receive a temporary `EnemySpeedBuff` and `EnemyAI.permanentAggro` at spawn (set in code, not JSON).

**Runtime:** Spawner fills `ecs::EnemyAI` (+ `ecs::WardenTuning` for bruisers) from the catalog; `enemy_ai_system` reads components, not `config.hpp`. New entries can reuse an existing `behavior` with different stats without new code.

## Recipes (`assets/data/recipes.json`)

| Field | Notes |
|--------|--------|
| `schemaVersion` | Must be present and `1`; missing files fall back to built-in seeds (with a warning). |
| `recipes[]` | Each recipe has **`id`**, **`kind`** (`forge` / `disassemble`), **`inputs`**, **`outputs`**, optional **`conditions`**, optional **`sideEffects`**. |

**Conditions:** Implemented kinds include **`always`** (or empty), **`playerLevelAtLeast`** (uses **`value`** as integer level threshold). Unknown condition kinds produce a warning; matching fails for guarded recipes until implemented.

**Execution:** Matches from [`crafting.hpp`](../src/game/crafting.hpp) feed **`TransformPlan`** → **`executeTransform`**, which checks conditions, runs **`applyRecipeMatch`** (inventory mutations via **`InvCtx`**), then processes **`sideEffects`** (handlers log until implemented).

## World interactables (`Interactable`)

Use **`interactionKind`** on [`ecs::Interactable`](../src/ecs/components.hpp) (`Generic`, **`Anvil`**, **`OldCasket`**). F-key handling calls [`dispatchInteractablePrimaryUse`](../src/game/world_interaction.hpp) instead of comparing display **`name`** strings. `OldCasket` covers all loot-casket tiers; the per-entity `ecs::CasketLoot` holds the **rolled** contents (catalog ids, passed to the loot drop as a contiguous string range plus count).

## Casket loot tiers (`CasketTier`)

Caskets are **not** pre-filled in the editor. Each placement carries a **tier** (`CASKET cx cy <old|sealed|wrought>`); contents are rolled once per session in [`rollCasketLoot`](../src/game/game_data.hpp) when the casket entity spawns. Bridged rarity names share one pool — **Tarnished/Clouded**, **Cursed/Lucid**, **Abyssal/Absolute** — and **`Special`**-rarity items are never rolled.

| Tier | 1 item | 2 items | 3 items | T/Clouded | Blighted | Cursed/Lucid | Dread | Abyssal/Absolute | Guarantee |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Old** | 65% | 25% | 10% | 45% | 30% | 18% | 6% | 1% | — |
| **Sealed** | 35% | 45% | 20% | 30% | 30% | 25% | 12% | 3% | ≥1 Blighted+ |
| **Wrought** | 10% | 40% | 50% | 15% | 25% | 30% | 20% | 10% | ≥1 Cursed/Lucid+ |

## Map geometry & spawners (`.map` lines)

`MapData` (`src/game/map_data.hpp`) parses one space-separated record per line: `PLAYER_SPAWN`, `WALL`, `LAVA`, `SOLID`, `ENEMY x y type`, `ITEM x y kind`, `ANVIL cx cy`, **`NODE cx cy`**, **`CASKET cx cy <tier>`** (`tier` = `old`/`sealed`/`wrought`; legacy item-id tokens are ignored and default to `old`). A **`NODE`** spawns a stationary `NodeSpawner` (150 HP, `Enemy`-tagged but `Immovable`, 65 XP on death) whose Dreg-burst / sustain / dormancy tuning is fixed in the [`NodeSpawner`](../src/ecs/components.hpp) component defaults; the map only stores its position. The editor exposes Nodes under the **Elements** tab and Dregs under the **Units** tab.

## Combat resolution

Environmental, melee, and projectile paths should use **`ecs::resolveDamage`** with an appropriate **`DamageCategory`**. Passive **`OnTakeDamage`** item triggers are consulted when **`CombatResolutionOpts::victimInventory`** is set. **`applyHeal`** clamps healing to **`Health::max`** for shared heal math.
