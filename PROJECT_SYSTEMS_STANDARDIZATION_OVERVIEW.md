# Dreadcast: Systems and Standardization Overview

## Purpose

This document captures the current project setup and a comprehensive high-level inventory of game systems, subsystems, features, scenes, and content elements.  
It also outlines ways these can be standardized, centralized, and transformed as the game expands (new items, effects, status effects, damage types, enemies, map objects, slots/equipment systems, and beyond).

## Current Setup Snapshot

- Engine/runtime stack: C++17, Raylib, EnTT ECS, nlohmann/json, custom audio via miniaudio.
- Build pipeline: CMake + FetchContent (Raylib/EnTT/json), assets copied next to executable post-build.
- Core app flow: `main` -> `Game` loop -> `SceneManager` stack-based scene transitions.
- Data/content approach: mixed model of external JSON/map files plus embedded fallback data in code.
- Authoring model: in-game map editor for map geometry/entities/interactables plus JSON content files for classes/items/abilities.
- Persistence: settings and maps are persisted; run state/progression appears mostly session-scoped.

## Systems Inventory (Comprehensive, High-Level)

## 1) Application and Runtime Orchestration

- Bootstrapping and mode selection (normal play vs editor mode).
- Main frame loop (input snapshot, fixed/variable timing, audio tick, scene update, render, overlay/cursor/fps).
- Scene stack lifecycle and transition control.
- Resource lifecycle (asset loading, caching, runtime access, fallback behavior).

Standardization/centralization options:
- Introduce a single "application composition root" for subsystem ownership and initialization order.
- Define uniform scene lifecycle contracts and transition rules (single scene protocol).
- Centralize frame-stage definitions into explicit pipeline phases with a shared interface.

## 2) Scene Layer

- Menu scene, character select scene, gameplay scene, settings scene, editor scene.
- Overlay/modal behaviors (pause, game over, inventory, map, skill tree, workbench/anvil).
- Scene-specific input handling and focus/state rules.

Standardization/centralization options:
- Standardize scene state model (active, paused, modal-blocked, transitioning).
- Centralize overlay stack behavior into one reusable modal/navigation controller.
- Align scene input ownership rules through a single focus and priority policy.

## 3) ECS Core and Simulation Loop

- Entity/component data model with combat, movement, AI, projectiles, status, item/world references.
- Fixed-step simulation sequencing across gameplay systems.
- System execution order dependencies (input -> AI/combat -> movement/collision -> effects/death -> presentation).

Standardization/centralization options:
- Define a formal system pipeline contract with named stages and clear read/write boundaries.
- Introduce standardized component ownership and mutation rules per stage.
- Centralize system registration/order in one data-driven or declarative pipeline table.

## 4) Combat and Damage Model

- Ranged and melee player combat loops.
- Enemy attack behavior integration.
- Projectile flow, hit resolution, knockback/stun/pull interactions.
- Reflective damage behavior currently present as a mechanic.

Standardization/centralization options:
- Move to a centralized damage event model (single damage transaction schema).
- Introduce a canonical damage taxonomy (physical, elemental, reflective, true, etc.) with shared resist/vulnerability hooks.
- Standardize hit processing through one resolver pipeline (pre-hit modifiers, on-hit effects, post-hit reactions).

## 5) Abilities, Status Effects, and Temporary States

- Class ability set (costs, cooldowns, targeting/activation phases, runtime effects).
- Temporary effects and timed states (stun, dashes, aim states, regen-over-time, ability-specific states).
- HUD icon/timer reflection of active statuses.

Standardization/centralization options:
- Unify status/effect definitions into a central effect framework with common lifecycle hooks.
- Standardize effect stacking, refreshing, priority, immunity, and dispel semantics.
- Centralize ability activation flow into shared ability state machines and reusable cast/resolve contracts.

## 6) Character Progression and Stats

- Character base stats and per-level growth.
- XP gain and leveling loop.
- Skill points and skill tree integration.
- Stat changes sourced from class data, levels, effects, and equipment.

Standardization/centralization options:
- Introduce one canonical stat aggregation pipeline (base + additive + multiplicative + conditional + temporary).
- Standardize progression events (level-up, unlock, passive acquisition) through a centralized progression domain.
- Decouple skill-tree logic into shared progression graph abstractions for multi-class growth.

## 7) Inventory, Equipment, Slots, and Itemization

- Item definitions and categories (equipment, consumables, stackable/non-stackable).
- Bag storage, quick-use slots, equipment slots (armor/amulet/ring).
- Drag/drop, merge/split stacks, equip/unequip, consume, world pickup/drop flows.
- Item bonuses influencing combat and survivability.

Standardization/centralization options:
- Centralize item schema and behavior tags (equip rules, usage rules, modifiers, triggers) in one authoritative catalog.
- Standardize slot semantics using generic slot type definitions rather than per-screen special handling.
- Unify inventory operations (move/swap/split/merge/equip/use/drop) into one transaction API to reduce edge-case drift.

## 8) Crafting/Workbench and Transformation Systems

- Anvil/workbench interaction and UI mode.
- Forge/disassemble flows.
- Recipe matching and output generation.
- Return-to-bag/drop fallback handling.

Standardization/centralization options:
- Move recipes and transformation rules to centralized data files (single crafting schema).
- Standardize transformation operations through a generic item processing pipeline (inputs, conditions, outputs, side effects).
- Reuse one interaction and transaction framework between inventory and crafting surfaces.

## 9) AI, Enemy Archetypes, and Behavior Logic

- Enemy archetypes (melee/ranged roles).
- Aggro/calmdown behavior states.
- Line-of-sight checks, steering/avoidance, and movement heuristics.
- Enemy projectile/melee integration with shared combat systems.

Standardization/centralization options:
- Centralize behavior state machine contracts (sensing, decision, action, cooldown).
- Standardize enemy archetypes via data-driven behavior profiles and capability tags.
- Consolidate navigation/avoidance primitives into shared movement decision modules.

## 10) World Simulation and Interactables

- Map-driven world composition (walls, lava/hazards, solids, spawns, enemies, items, anvils, caskets).
- Hazard systems (movement penalty and periodic damage).
- Interactable objects with range and key-based interaction.
- Loot containers and map-authored reward placement.

Standardization/centralization options:
- Introduce a unified interactable framework (prompting, range checks, action execution, cooldown/one-time state).
- Standardize world object schemas with typed behaviors and configurable interaction contracts.
- Centralize hazard logic under a shared environment effect subsystem.

## 11) Map/Data Authoring and Serialization

- Custom `.map` grammar with typed lines for world entities and structures.
- Runtime map parse/serialize lifecycle.
- Default map behavior and fallback mapping.
- Editor-generated and hand-authored map content.

Standardization/centralization options:
- Formalize map schema/versioning with compatibility rules and migration hooks.
- Standardize authoring metadata (IDs, categories, validation constraints) for all placeable objects.
- Centralize map validation so editor/runtime/build all use the same checks.

## 12) UI and Presentation Layer

- HUD systems: health/mana/xp, abilities, status icons, combat cues, contextual shortcuts.
- Modal/panel UX for inventory, map, skill tree, settings, pause, game over.
- Shared UI primitives and visual theme constants.
- Tooltip behaviors and detail modes.

Standardization/centralization options:
- Centralize UI state in a shared UI domain model (single source of open/closed/focused panels).
- Standardize component contracts for tooltips, buttons, slots, and status widgets.
- Define one consistent UI interaction policy (input capture, close rules, escape/back hierarchy).

## 13) Rendering, Camera, Visibility, and Spatial Math

- Isometric conversion utilities and directional/facing helpers.
- Layered render system for world, entities, effects, and overlays.
- Fog-of-war visibility and fallback paths.
- Camera follow and camera control modes.

Standardization/centralization options:
- Standardize rendering passes through a declarative render pipeline.
- Centralize visibility/fog integration as a service consumed by AI, rendering, and debug tooling.
- Consolidate spatial conventions (world/iso/screen transforms) behind one coordinate API.

## 14) Input, Cursor, and Interaction Control

- Input abstraction over keyboard/mouse states.
- Software cursor modes (default/aim/interact/invalid).
- Context-sensitive cursor and interaction prompts.

Standardization/centralization options:
- Centralize input action mapping into an action/intent layer (not direct key checks throughout systems).
- Standardize interaction intents (attack/interact/use/confirm/cancel) across scenes and UI.
- Treat cursor mode as derived UI state from a centralized interaction context service.

## 15) Audio and Feedback

- Audio engine init/update/device management.
- One-shot and channelized sound playback.
- Settings-driven audio controls and persistence.
- Combat and AI event-triggered SFX.

Standardization/centralization options:
- Centralize audio event routing (game event -> audio cue mapping).
- Standardize cue definitions via data manifest with categories, priorities, and mixing groups.
- Unify gameplay/UI feedback orchestration for visual + audio coherence.

## 16) Configuration, Content Data, and Persistence

- Runtime settings file with gameplay/video/audio preferences.
- JSON-driven characters/items/abilities.
- Mixed fallback strategy (external files + embedded data blobs).
- Session vs persisted state boundaries.

Standardization/centralization options:
- Establish single-source-of-truth policy per domain (content, settings, defaults, saves).
- Standardize schema governance and validation for all data files.
- Centralize persistence boundaries (what is session-only vs long-term profile vs world/save data).

## 17) Editor Tooling and Content Pipeline

- In-game editor tools for map geometry/entities/objects.
- Selection/placement/transform/copy/undo style workflows.
- Save/load/new-map and unsaved-change guards.
- Item and loot assignment behaviors from within editor flows.

Standardization/centralization options:
- Align editor data access to the same runtime catalogs used by gameplay (no hardcoded editor-only lists).
- Centralize tooling capabilities into reusable editor modules (selection, transform, validation, placement constraints).
- Introduce content pipeline contracts so editor/runtime/tests consume identical schemas and validators.

## Cross-Cutting Growth Pressure Areas

- New damage types and effect categories will multiply conditional edge cases without a unified damage/effect model.
- New items and slots will magnify inventory/crafting/equipment inconsistencies without a shared item transaction framework.
- New enemies and behaviors will increase AI branching complexity without standardized behavior contracts.
- New interactables/map objects will fragment interaction logic without a unified interactable system.
- New classes/abilities/statuses will accelerate duplication without centralized ability/effect schemas.
- More content files and authored maps will drift without schema/version governance.

## High-Level Transformation Directions

## A) Data-First Domain Model

- Treat data catalogs as authoritative for items, abilities, enemy profiles, interactables, recipes, and UI/audio manifests.
- Minimize in-code duplicated fallback payloads.
- Adopt schema/version rules for forward evolution and validation.

## B) Event-Driven Gameplay Core

- Shift key gameplay outcomes to normalized events (damage, heal, apply-status, level-up, item-transferred, interaction-triggered).
- Route subsystems through consistent event payloads and hooks rather than ad hoc direct mutations.
- Improve extensibility for new mechanics and edge-case handling.

## C) Unified Transaction Layers

- Inventory/equipment/crafting/world pickup/drop all use shared transaction semantics.
- Combat/effect application use shared resolution semantics.
- Interactions (player <-> world object) use shared action semantics.

## D) Shared State and Contract Boundaries

- Explicitly separate domain states: world simulation, player progression, UI state, authored content, user settings.
- Define ownership and synchronization boundaries between ECS, scene logic, UI, and data loaders.
- Reduce hidden coupling and mode-specific behavior divergence.

## E) Authoring and Validation Discipline

- Validate content at author time and load time with the same validators.
- Maintain one reference vocabulary for IDs, tags, slot types, damage/effect types.
- Keep design and implementation aligned as feature count scales.

## Standardization Outcomes to Target

- Predictable behavior across systems when adding new content/mechanics.
- Fewer edge-case regressions from duplicated logic and inconsistent state transitions.
- Lower cost of introducing new items/effects/damage types/enemy archetypes/map objects.
- Better maintainability through clear ownership, schemas, and reusable subsystem contracts.
- Faster iteration in both runtime gameplay and editor/content workflows.

## Closing Perspective

Dreadcast already has strong foundational pieces: scene architecture, ECS-driven runtime, data files, and an in-engine editor.  
The highest leverage now is not adding isolated mechanics one-by-one, but converging around shared domain contracts (damage/effects, item transactions, interaction flows, and content schemas).  
That transformation keeps current momentum while making future expansion substantially safer and faster.
