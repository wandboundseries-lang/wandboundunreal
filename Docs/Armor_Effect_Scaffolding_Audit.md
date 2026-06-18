# Armor Effect Scaffolding Audit

## Scope

This audit inspected the Godot reference for armor-modifying effect behavior before adding Unreal C++ scaffolding. The Unreal pass is limited to generic armor operations and does not implement card-specific armor cards, card database import, targeting UI, effect activation timing, responses, passives, wands, or runtime presentation.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/effect_registry.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/modify_stat_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/stat_adjustment_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/directives/handlers/modify_stat_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/characters.json`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/wands_equip.json`
- `Reference/GodotProject/godotcanon/tools/ci/validate_carddb.gd`

Search terms included `current_armor`, `max_armor`, `add armor`, `gain armor`, `grant armor`, `set_armor`, `modify_armor`, `restore armor`, `shield`, `gain_stat`, `modify_stat`, `stat_adjustment`, `apply_effect`, `EffectRunner`, and CardDB armor entries.

## Godot Findings

Godot armor effect type names found:

- `restore_armor`
- `modify_stat` with `stat = "armor"`, `"current_armor"`, or `"max_armor"`
- `stat_adjustment` / `stat_adjust` with armor stat aliases for temporary stat effects

`effect_registry.gd` routes both `modify_stat` and `restore_armor` through `modify_stat_handler.gd`. That handler maps `restore_armor` to `current_armor` and maps the alias `armor` to `current_armor`.

`game_state.gd` provides the armor primitives:

- `set_base_armor` sets both `max_armor` and `current_armor`.
- `set_current_armor` clamps current armor from `0` to `Rules.get_effective_max_armor`.
- `restore_armor` adds an amount to current armor and clamps at max armor.
- `increase_max_armor` / `decrease_max_armor` call `bump_max_armor`.
- `bump_max_armor` clamps max armor to at least zero and re-clamps current armor afterward.

Current armor clamps to max armor. Reducing max armor clamps current armor down to the new max. Armor cannot be restored above max.

Increasing max armor does not automatically increase current armor unless the effect carries `restore_current_by_delta` / `also_restore_armor`. For this Unreal pass, max-armor increases will not auto-fill current armor.

Armor effects are public because Godot public unit snapshots include `current_armor` and `max_armor`.

CardDB examples found:

- Officer Repairer uses `restore_armor` for a chosen friendly unit.
- Armor Plate uses `modify_stat` on `max_armor` with `restore_current_by_delta = true`.
- Officer stat lines and auras use `armor` / `max_armor`.

Those examples are card-specific behavior and remain out of scope.

## Chosen Unreal Operations

Unreal will add generic operations:

- `AddCurrentArmor`
- `ReduceCurrentArmor`
- `SetCurrentArmor`
- `AddMaxArmor`
- `ReduceMaxArmor`
- `SetMaxArmor`
- `RestoreArmorToMax`

These are lower-level scaffolding operations, not card IDs or card activation rules.

Chosen clamp behavior:

- current armor is always `0..MaxArmor`
- max armor is always `>= 0`
- if max armor is zero, current armor is zero
- reducing max armor clamps current armor
- adding max armor does not auto-fill current armor in this generic operation
- restoring armor sets current armor to max armor

## Out of Scope

- card-specific armor cards
- Officer Repairer
- Armor Plate
- target selection
- effect activation timing
- responses
- counters
- passives
- wands
- card database import
- runtime/UI presentation
- Blueprints, `.uasset`, `.umap`, VFX, audio, or 3D runtime
