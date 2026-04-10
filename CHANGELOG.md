# Changelog

All notable changes to **Dreadcast** will be documented in this file.

## v0.10.1 — 2026-04-10

### Added

- **Ability HUD:** Draft icons **`assets/textures/drafts/1.png`**, **`2.png`**, **`3.png`** for Undead Hunter abilities; **104×104** slots **upper-center**, vertical center aligned with the **mana bar**; key **1 / 2 / 3** in small **black squares** slightly above/outside each icon; **mana cost** in **blue** at lower-right of each icon (toggleable).
- **Consumable HUD (C / V):** **5:4** slots, **half** ability height, **left** of abilities, slightly **lower**; same keybind badge style; **stack count** lower-right inside the slot.
- **Equipment HUD:** Three **7:5**-fitted icons below the mana bar (armor / amulet / ring) with **Runic Shell** cooldown overlay when applicable.
- **Floating damage/heal numbers** at entities (world→screen), toggleable; **Settings → Gameplay:** **Show ability mana cost** (default **on**), **Show damage/heal numbers** (default **off**); persisted in **`settings.cfg`**.
- **`GameSettings`:** **`showAbilityManaCost`**, **`showDamageNumbers`**.

### Changed

- **Pure Blood HOT + Cordial Manic:** HOT **timer keeps running** during Manic; **no healing** while Manic (Cordial still blocks regen/HOT HP as before).
- **Inventory rarity (i) panel:** Wider (**560px**); each tier is **name — description** on **one line** with shortened blurbs.
- **`InventoryUI::drawItemIcon`** is **public** for reuse (gameplay equipment row uses local scaled draw to fit slots).

### Fixed

- **Gameplay HUD:** Ability/consumable hover uses **raw mouse** (`GetMousePosition`) for hit-testing and tooltips; **default cursor** over those HUD elements (aim cursor unchanged elsewhere).

## v0.10.0 — 2026-04-09

### Added

- **Undead Hunter abilities (keys 1–3):** **Lead Fever** (scatter pellets + knockback, HUD status), **Deadlight Snare** (net, backward dash, pull + **Stunned**), **Calamity Slug** (1s aim, piercing 50 damage, sideways knockback). Lower-right HUD ability bar with cooldown overlay and hover tooltips.
- **`ecs::StunnedState`**, **`LeadFeverEffect`**, **`SnareProjectile`**, **`SlugProjectile`**, **`PierceHitRecord`**, **`SlugAimState`**, **`SnareDashState`**; **`Projectile::pierce`**, **`knockbackOnHit`**.
- **Stacking status HUD:** multiple vial/ability effects with **right = oldest**, **left = newest**; icons use **center-cropped** item art (or placeholder fill) plus timer ring.
- **`LORE.md`** template for non-mechanical world/character notes.
- **`src/game/ability.hpp`** — ability definitions for the Undead Hunter.

### Changed

- **Cordial Manic:** no longer removes **Pure Blood** HOT; **Pure Blood** can be drunk during Manic (no HP from HOT while Manic); **Runic Shell** shockwave still fires but **does not heal** under Manic.
- **Inventory:** rarity **info** control polished (fill, double ring, hover); draw order fixed so **item tooltips render on top** of the info button and rarity panel.
- **Settings → Controls:** documents abilities **1 / 2 / 3**.
- **Fog of war:** composite overlay shader **blurs the fog mask** (5×5 tap) so the visibility boundary fades gradually (Dota-style soft edge). Tunable via **`FOG_EDGE_SOFTEN_PIXELS`** in `config.hpp` (**0** = previous hard edge).

### Fixed

- **Fog of war (RT + shader path):** Visibility “island” could disappear entirely (full-screen fog) while the CPU still built a valid polygon — the mask render target stayed white because **backface culling** and a single huge **`DrawTriangleFan`** batch did not reliably produce visible geometry when drawing into an FBO under **`BeginMode2D`**. Mask fill now uses **per-wedge `DrawTriangle`** with **winding chosen for Y-down space**, **culling disabled** for that pass, **scissor/depth** cleared defensively, **batch flush** before ending 2D mode, and **separate `src` rects** for scene vs mask in the composite blit. **F3** toggles an optional debug overlay (RT previews + stats).

## v0.9.1 — 2026-04-03

### Added

- **Runic Shell** armor (`runic_shell` map kind): **Cursed** rarity; **+25 Max HP**; when HP drops below **30%**, releases an energy shockwave dealing **30 damage** and knockback to nearby enemies, then heals **30 HP**. **30s** cooldown with timer overlay on the inventory icon. Expanding ring VFX on activation. Icon: `assets/textures/items/runic_shell_icon.png`.
- **Vial visual effects:** **Vial of Pure Blood** (HOT active) — pulsing crimson glow ring and orbiting particles around the player. **Vial of Cordial Manic** — golden flickering energy aura with trailing speed lines while moving.
- **`ecs::KnockbackState`** component: enemies skip AI and decelerate during knockback; melee strikes now set velocity directly for a visible, consistent push.
- **`ecs::RunicShellCooldown`** component for tracking the triggered armor ability.
- **Enemy AI — stuck detection:** Enemies track displacement over time; when stuck against a wall for **0.25s**, AI steers perpendicular to slide around the obstruction via wall-avoidance raycasts.
- **Enemy rule — damage boosts aggro:** Any player damage (melee or projectile) to an enemy increases its **agitation range by 100** (permanent per hit).
- **Equip slot icons:** Empty Armor, Amulet, and Ring slots display centered **512×512** PNG icons (black-on-transparent, tinted to match UI) instead of text labels. Icons: `assets/textures/ui/armor_slot.png`, `amulet_slot.png`, `ring_slot.png`.
- **Bag stack count:** Stackable consumables in bag slots now show their stack count in the lower-right corner (same style as consumable slots).

### Changed

- **Melee knockback:** Base knockback force increased from **200** to **350**; knockback now **sets** enemy velocity (not additive) and applies a `KnockbackState` that freezes enemy AI for **0.25s** with friction decay, making the push visually clear and preventing enemies from immediately resuming movement.
- **Imp speed:** `IMP_ADVANCE_SPEED` increased from **75** to **100**; `IMP_KITE_SPEED` increased from **120** to **145**.
- **Editor item picker:** Dropdown now includes **Runic Shell** (5 item kinds total).

### Fixed

- **Item pickup cursor mismatch:** Loot hover detection now uses the virtual aim cursor position (`aimScreenPos_`) instead of the raw OS mouse, matching where the crosshair is visually drawn. Closing the inventory syncs `aimScreenPos_` to the current mouse position, preventing items on the ground from becoming un-hoverable or having shifted hit zones after inventory use.

## v0.9.0 — 2026-04-03

### Added

- **Settings → Gameplay:** **Save** writes `settings.cfg` (mouse sensitivity + FPS toggle); **Reset** restores default `GameSettings`. Settings load from disk when the game starts (`ResourceManager` constructor).
- **Barbed Tunic** armor (`barbed_tunic` map kind): **Blighted** rarity; **+0.3 HP/s** and **10%** incoming damage reflected instantly to the attacker (projectiles track source entity; hellhound melee reflects to self). Icon: `assets/textures/items/barbed_tunic_icon.png`.
- **Vial of Cordial Manic** (`vial_cordial_manic`): **Lucid** rarity (stack **3**); **7s** effect — **2×** move speed, **invulnerability** vs enemies, **no HP regen** (class + gear + heal-over-time blocked), linear **40% max HP** self-drain; unusable below **40%** current HP (**red X** on HUD + inventory icon). HUD status icon with timer. Icon: `assets/textures/items/vial_cordial_manic_icon.png`.
- **`ecs::ManicEffect`** player component; **`Projectile::source`** for reflect attribution.
- **`GameCamera::syncFollowFromCamera()`** for editor grip / direct target edits.

### Changed

- **Rarity model:** Single set of names — equippable: **Tarnished, Blighted, Cursed, Dread, Abyssal**; consumable: **Clouded, Lucid, Absolute, Special**. No separate “Common/Uncommon” labels. Consumable stack caps: **Clouded 5**, **Lucid 3**, **Absolute 1**, **Special 1** (`maxStackForConsumableRarity`). Tooltip rarity line is the name only; inventory **(i)** panel lists tiers + blurbs (with stack hints for vials). Item descriptions trimmed to mechanics (no duplicate rarity flavor in item text).
- **Inventory slots:** Rarity-colored **tint** fills slot behind centered **7:5** icons (equippable + consumable colors per plan).
- **Vial of Pure Blood:** **Clouded**; using again while HOT is active **refreshes** the effect; brief **white flash** on the HOT HUD icon.
- **Editor:** Wider **UI hit rect** blocks world placement when clicking dropdowns; **Enemy** and **Item** pickers are **dropdowns** (items: Iron Armor, Pure Blood, Cordial Manic, Barbed Tunic). **Middle-mouse** pan updates the camera target **directly** (no lerp drift).
- **`ItemData`:** `hpRegenBonus`, `damageReflectPercent`; **`InventoryState::totalEquippedHpRegenBonus`**, **`totalEquippedDamageReflect`**.
- **Passive HP regen HUD** shows class + equipment total (hidden during Cordial Manic).

### Fixed

- **Editor:** Toolbar-adjacent **type controls** no longer cause **click pass-through** to the world.

## v0.8.0 — 2026-04-01

### Added

- **High-res item icons:** Iron armor and vial of pure blood use new **PNG** art (`assets/textures/items/*.png`, 7:5, source resolution 1152×823).
- **`game/item_factory.hpp`:** Shared `makeItemFromMapKind()` for casket loot, map spawns, and editor-defined drops.
- **`game/item_rarity.hpp`:** Rarity tiers (**Common** … **Legendary**, **Special**), **gear** style names (Tarnished … Abyssal), **vial** style names (Clouded … Anomalous `[Vial]`), colors, and **`rarityLine()`** for UI. **`ItemData::rarity`** on items.
- **Map item spawns:** `.map` files support **`ITEM x y kind`** lines (`kind` is `iron_armor` or `vial_pure_blood`). Gameplay spawns matching **ground pickups** at load (items are added to the pool and dropped in world space).
- **Editor — place items:** New **Item** tool with **Item: Armor / Item: Vial** type toggle (same pattern as enemies). Item pickups render in the world view; select, drag, **Del**, **Ctrl+D**, **Ctrl+C / Ctrl+V** work like other entities. Status line lists **Items** count.
- **Inventory — Shift+click:** **Shift+left-click** on **bag** slots **equips** gear or consumables (consumables only if a consumable slot is free). **Shift+left-click** on **equipped gear** or **consumable bar** **unequips** to the bag **only when at least one bag slot is empty** (otherwise no-op). Consumables from bag do not steal a full consumable bar (no replace when both slots full).
- **Consumable stack count:** Stack size is drawn in the **lower-right** of **consumable** inventory slots (stackable items).

### Changed

- **Inventory panel:** Larger window (**760×480**); slot tiers (**equip 120×86**, **consumable 108×77**, **bag 98×70**), **7:5**, **equip > consumable > bag**. **Carried** column offset **px + 340**.
- **Equipped layout:** **Armor** centered on row 1; **Amulet** and **Ring** on row 2.
- **Inventory icons:** Fixed **7:5** draw size; **centered** in slots; **no inline names** (tooltips + rarity line).
- **Inventory tooltips:** **Title**, colored **rarity line** (e.g. `Tarnished (Common)` / `Clouded [Vial] (Common)`), multiline **description**, rarity-colored tooltip border; **Alt** still shows slot / stats.
- **Rarity info panel (i):** Replaces placeholder with **gear** and **vial** tier blurbs and matching colors.
- **Item copy:** Iron Armor and Vial of Pure Blood use **Common** rarity and updated **descriptions** (Tarnished / Clouded flavor + stats).
- **Inventory drag:** **Item icon** follows the cursor (shadow + frame), not text.
- **HUD consumable bar:** **7:5** icon fit in the compact slots.
- **Gameplay:** `spawnWorld()` resets **inventory** and applies map **ITEM** spawns; casket loot uses **`makeItemFromMapKind()`**.

## v0.7.0 — 2026-04-01

### Added

- **Gameplay settings tab:** First tab in Settings with **mouse sensitivity** slider (0.1–3.0); aim uses scaled offset from the player’s screen position (persisted in `ResourceManager` settings).
- **Fog of war (visual):** World-space **visibility polygon** (angular raycasts vs `Wall` AABBs, capped by `FOG_OF_WAR_RADIUS`), projected through `worldToIso` + camera; **fog mask** render target matches the scene buffer size. Composite: **draw scene RT** with default shader, then **single-texture** overlay shader (fog mask → black with alpha) so Raylib’s batch only binds `texture0` (no unreliable second sampler). If render targets or the shader fail to load, falls back to the previous **DrawRing** vignette.
- **Editor multi-map workflow:** Scan `assets/maps/*.map`, **Prev / Next** cycle maps, **New map** creates `map_NNN`, Save/Load use the current map name; **Ctrl+C / Ctrl+V** copy-paste selection; wall **edge handles** for mouse resize; **middle-mouse camera grip** (screen-locked world point); **toolbar blocks world clicks**; **Enemy type** button sits to the right of the **Enemy** tool (also when an enemy is selected).
- **Enemy AI:** Steering lerp (no per-tick velocity damping jitter), **calm-down delay** before deaggro, **last-known position** pursuit when LOS breaks, Imp **panic vs chase** bands (`IMP_PANIC_RANGE`).
- **Item icons:** `ItemData::iconPath` drives small **PPM** icons in the inventory grid and HUD consumable slots (e.g. Iron Armor, Vial of Pure Blood under `assets/textures/items/`).
- **Scene cursor hook:** Default `Scene::drawCursor` draws the software cursor; gameplay overrides for aim / interact states (OS cursor hidden globally).

### Changed

- **Cursors:** Software-drawn sprites from `assets/cursors/`; no **invalid** cursor for low mana / full inventory; **Interact** cursor also on loot hover.
- **Main menu:** **Esc** no longer quits the game.
- **Video settings:** FPS toggle moved further right; Controls text notes aim sensitivity lives under Gameplay.
- **Melee (RMB):** **Three-hit** string with hit windows, hold to chain swings and full **Recovery** between combos, release finishes the current swing; **per-swing damage/knockback** and **animated cone** width/sweep.
- **Fog culling:** **Mana shards** hidden without line-of-sight like enemies (radius + wall LOS in `render_system`).

## v0.6.0 — 2026-03-27

### Added

- **Hellhound enemy type:** New melee-only enemy with **60 HP**, **15 damage**, **1.0s** melee cooldown, chase behavior, and dedicated map/editor enemy type support (`imp` / `hellhound`).
- **Unit collision:** Player and enemies now physically separate from each other to prevent overlapping/stacking inside one another.
- **Fog of war:** Gameplay now uses a visibility radius around the player with wall-based line-of-sight gating for enemies and enemy projectiles, plus a darkened outer area.
- **Editor ALT overlays:** Holding **Alt** in editor now shows enemy aggro ranges, unit collision bounds, and player fog-of-war radius preview.
- **Editor enemy placement type:** Enemy placement tool can now switch between spawning **Imp** and **Hellhound** entries in map data.
- **Editor camera drag-pan:** Camera can now be panned by holding mouse buttons and dragging (in addition to WASD).

### Changed

- **Imp size + behavior:** Imps are now smaller (closer to player size) and use kiting movement to keep sight and maintain ranged spacing instead of getting cornered easily.
- **Map enemy format:** Map enemy lines now store enemy type (`ENEMY x y type`), while remaining backward compatible with old maps that omit type.
- **Passive regen tuning:** Undead Hunter passive regen is now **+0.5 HP/s** and **+0.5 Mana/s**.
- **Max HP scaling:** Equipping/unequipping max-HP items now preserves current HP percentage (e.g., 90/100 -> 99/110), rather than treating increased max HP as missing HP.
- **Settings layout:** FPS on/off toggle in **Settings -> Video** is positioned further right from its label for clearer spacing.
- **Status timer shape:** Consumable status timer outline now depletes as a square path around the status icon, matching icon shape.
- **Editor wall feedback:** Selected walls now display their current width/height values while editing.

### Fixed

- **False damage feedback on max-HP changes:** Max-HP manipulation from equipment no longer triggers damage-style HP-loss feedback.

## v0.5.0 — 2026-03-27

### Added

- **Editor mode:** Launch with `--editor` to open an in-game map editor that supports placing/moving/resizing walls, setting player spawn, placing enemies, moving the Old Casket, deleting/duplicating selected objects, and saving/loading map data.
- **Map persistence:** Map layout now loads from `assets/maps/default.map` during normal gameplay, so maps authored in editor mode are used when running the game without editor mode.
- **Inventory rarity info widget:** Inventory panel now has an info icon in the upper-right that opens an overlay with a placeholder rarity-system explainer.

### Changed

- **HUD scale/layout:** Portrait, HP bar, and mana bar are larger; portrait is scaled up relatively more and centered better against the bar stack to account for consumable/status space.
- **Status effect timer:** Consumable heal status icon now uses a constant-width radial progress stroke that depletes clockwise from 12 o'clock instead of animating border thickness.
- **Tooltips:** Inventory and world item tooltips are cursor-relative and render above the cursor; holding **Alt** expands tooltip content vertically upward for extended details.
- **Inventory interactions:** Consumables now support drag-and-drop into/out of consumable slots (including slot swap and drag-drop to ground).
- **Settings UI:** FPS toggle control is now compact and aligned on the same row as the "Show FPS Counter" description.

## v0.4.1 — 2026-03-25

### Added

- **FPS counter:** Upper-right overlay visible on main menu, pause overlay, character selection, and gameplay; toggle in **Settings -> Video**.
- **Passive regen (classes):** Characters gain passive HP/mana regen; **Undead Hunter** is `+1.0 HP/s` and `+2.0 Mana/s`. Selector shows the regen values.
- **HUD consumables:** Two equipped consumable slots shown under the HP/MP bars with remaining stack counts and keybinds (**C** / **V**).

### Changed

- **Vial rename + behavior:** `Vile of Pure Blood` renamed to **`Vial of Pure Blood`** and treated as a true consumable (not armor).
- **Consumable keybinds:** Consumable usage moved from `1 / 2` to **`C / V`**.
- **Menu/UI:** Main menu class name no longer overlaps its portrait; HUD portrait and HP/MP bars are bigger.
- **Loot UX:** Old Casket loot drops are spawned in the direction away from the player’s opening position, not on top of the casket.
- **Font quality:** UI text rendering improved (larger base font load + mipmaps/trilinear filtering) to reduce jaggies at small sizes.
- **Combat input:** Right-click melee now has a 1-second re-engage cooldown after releasing RMB to prevent click-spam.

### Fixed

- **Inventory context menus:** Carried vials get `Use / Equip / Drop`; vials in equipped consumable slots get `Use / Unequip / Drop`.
- **Inventory drop picking:** Drag-and-drop can drop items outside the inventory panel; overlapping drops are prevented by nudging to nearby positions.
- **Shooting through walls:** Player and imp projectiles now cannot pass through walls (projectiles are destroyed on wall contact).
- **Agitation + LOS:** Imp agitation/attacks only activate when the player is within range and there is direct line of sight (no walls in between).

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

### Fixed

- **Build:** `menu_scene.cpp` and `gameplay_scene.cpp` now include `core/input.hpp` so `InputManager` is a complete type when calling `isKeyPressed` (fixes incomplete-type errors with strict toolchains, e.g. Clang).