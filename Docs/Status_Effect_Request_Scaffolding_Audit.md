# Status Effect Request Scaffolding Audit

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/apply_status_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/cleanse_all_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/apply_status_if_target_on_terrain_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Status Shape

Godot stores unit statuses in `statuses_by_unit[unit_id][status_id] = StatusInstance`.
Each instance carries `status_id`, `timing`, `ticks_total`, `ticks_remaining`, `has_started`, `applied_on_global_turn`, and source metadata.

`apply_status` rejects missing unit ids, missing unit records, empty status ids, zero ticks, and ticks below `-1`. Known status ids are lower-case behavior ids such as `burn`, `poison`, `root`, `stun`, `no_attack`, and `frozen`.

Godot status timing is status-defined:

- `burn`: end turn
- `poison`: start turn
- `root`: end turn duration decay
- `stun`: end turn duration decay
- `no_attack`: end turn duration decay
- `frozen`: end turn duration decay

## Apply, Refresh, Duration

Godot does not stack duplicate status entries. Reapplying an existing status refreshes the instance. For timed statuses, Godot keeps the greater of the old and incoming total/remaining tick counts. Reapplying poison also performs an immediate poison tick before refresh when the unit is not Frozen.

Godot uses `ticks_total == -1` / `ticks_remaining == -1` as an untimed convention for some paths. The Unreal migration request for this pass explicitly uses `Duration == 0` as permanent/untimed, matching the existing `FWBUnitState::AddStatus` helper behavior where non-positive duration removes duration metadata.

For this pass, Unreal generic `ApplyStatus` refreshes the status to the incoming duration rather than extending it. This keeps fixture-owned scaffolding simple and deterministic until card-specific Godot import rules are introduced.

## Remove And Cleanse

Godot `remove_status` returns void and is a no-op when the unit has no status map or the requested status is absent. Unreal mirrors that for `RemoveStatus` and `CleanseStatus`: missing status is a successful no-op.

Godot `cleanse_all_handler.gd` collects all active status ids for a unit, sorts them, calls `clear_statuses_for_unit`, and emits a `status_cleanse_all` event. Unreal `CleanseAllStatuses` removes all active statuses, clears duration metadata, and returns deterministic sorted public status names in `RemovedStatuses`.

## Effect Payload Shape

Godot `apply_status_handler.gd` accepts `status_id` or `status`, supports burn shorthand from `type == "burn"`, and reads duration from `ticks`, `duration_ticks`, or `duration`. The handler resolves a target, builds source metadata, and calls `gs.apply_status`.

This pass does not import Godot CardDB. Unreal-owned fixtures provide generic status payloads through `FWBEffectRequest`.

## Existing Unreal Status Shape

Unreal currently stores status state on `FWBUnitState`:

- `Statuses`: active public status ids
- `StatusTurnsRemaining`: positive remaining duration metadata
- helper aliases for `Rooted/root`, `Stunned/stun`, `Poison/poison`, `Burn/burn`, and `Frozen/frozen`

Existing start/end status tick scaffolding already consumes the status set and duration map for Burn, Poison, Rooted, Stunned, and Frozen.

## Unreal Operation Names

This pass adds `EWBStatusEffectOp` operations:

- `ApplyStatus`
- `RemoveStatus`
- `SetStatusDuration`
- `AddStatusDuration`
- `ReduceStatusDuration`
- `CleanseStatus`
- `CleanseAllStatuses`

Payload and trace operation names are snake_case:

- `apply_status`
- `remove_status`
- `set_status_duration`
- `add_status_duration`
- `reduce_status_duration`
- `cleanse_status`
- `cleanse_all_statuses`

Canonical public status names are `Burn`, `Poison`, `Rooted`, `Stunned`, and `Frozen`. Alias input accepts `burn`, `poison`, `root/rooted`, `stun/stunned`, and `frozen`.

## Out Of Scope

- Godot CardDB import
- JSON card database loading
- real card activation timing
- target selection UI
- response windows
- effect negation
- passives
- wands
- card-specific status behavior
- status tick behavior changes
- Burn/Poison tick effects inside status application
- Frozen break during status application beyond existing attack damage behavior
- runtime presentation, Blueprints, VFX, or assets
