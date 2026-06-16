# Status Model Audit

Date: 2026-06-16

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`

## Godot Status Representation

Godot keeps unit statuses in `GameState.statuses_by_unit`.

```text
statuses_by_unit[unit_id][status_id] = StatusInstance
```

The status instance includes:

- `status_id`
- `timing`: `turn_start`, `turn_end`, or `immediate`
- `ticks_total`
- `ticks_remaining`
- `has_started`
- `applied_on_global_turn`
- `source`

Godot status ids are lowercase in the runtime model: `burn`, `poison`, `root`, `stun`, `frozen`.

## Status Timing

`autoload/Game.gd:start_turn_impl` calls:

```text
state.tick_statuses("turn_start", current_player)
```

The current `tick_statuses` implementation notes that `active_player_id` is reserved for future rules and calls `_process_statuses_for_timing(timing)` without filtering by owner. For this pass, Unreal mirrors the current Godot behavior: a valid start-turn status tick pass applies to all units with matching `turn_start` statuses.

Godot timing found in `game_state.gd:apply_status`:

- `poison`: `turn_start`
- `burn`: `turn_end`
- `root`: `turn_end`
- `stun`: `turn_end`
- `frozen`: `turn_end`
- `no_attack`: `turn_end`

## Start-Turn Statuses

Poison is the only implemented start-turn periodic status in this pass.

Godot `_apply_status_tick` applies Poison at `turn_start` by calling `decrease_max_hp(unit_id, 1, 1)`. `bump_max_hp` clamps MaxHP to at least `1` and clamps current HP down if it exceeds the new MaxHP.

Godot also pauses Poison entirely while a unit is Frozen: no Poison tick and no duration decay.

## End-Turn Statuses

Burn is explicitly an end-turn status. Godot applies one damage at `turn_end`, bypassing this start-turn pass. Root and Stun have no periodic damage/effect; their durations decay at their `turn_end` timing boundary.

## End-Turn Status Tick Audit

Godot files inspected for the end-turn pass:

- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`

`autoload/Game.gd:end_turn_impl` calls:

```text
state.tick_statuses("turn_end", current_player)
_process_recent_deaths()
```

As with start-turn status ticks, `game_state.gd:tick_statuses` currently ignores `active_player_id` and processes all statuses matching the requested timing. For this pass, Unreal mirrors the current Godot behavior: a valid end-turn status tick pass applies to all units with matching `turn_end` statuses.

End-turn behavior confirmed:

- Burn ticks at `turn_end`.
- Burn calls `apply_damage(unit_id, 1, "burn")`.
- Godot `apply_damage` treats Burn as armor-bypassing damage.
- Burn duration decays at end turn.
- Root duration decays at end turn.
- Stun duration decays at end turn.
- Frozen duration decays at end turn.
- Poison does not tick or decay at end turn because Poison timing is `turn_start`.
- Godot `set_hp` clamps HP to a minimum of zero.
- Godot `apply_damage` can remove a unit and record `recent_deaths` after replacement/prevention checks.
- Godot processes recent deaths after `tick_statuses("turn_end", current_player)`.

Chosen Unreal behavior for this pass:

- `WBEffectRunner::ApplyEndOfTurnStatusTicks` is a deterministic phase operation, separate from the player `EndTurn` action for now.
- Burn deals one HP damage, clamps HP at zero, and does not change MaxHP.
- Full death/prevention and unit removal are intentionally not implemented.
- Burn, Rooted, Stunned, and Frozen timed durations decrement at end turn and expire at zero.
- Untimed/permanent statuses with duration `0` do not decay.
- Poison remains unchanged at end turn.

## Duration Behavior

Godot decrements `ticks_remaining` only when the status instance timing matches the processed timing and after `_apply_status_tick` runs. A negative duration is persistent. A duration that reaches zero removes the status.

For this pass:

- Poison duration decrements during `ApplyStartOfTurnStatusTicks`.
- Poison with one turn remaining applies its MaxHP loss, then expires.
- Burn, Rooted, Stunned, and Frozen do not decrement during start-turn ticks because their Godot timing is `turn_end`.
- Burn, Rooted, Stunned, and Frozen duration decrements during `ApplyEndOfTurnStatusTicks`.

## Current Unreal Representation Before This Pass

`FWBUnitState` used:

```text
TSet<FName> Statuses
```

Movement rules checked lowercase `root`/`stun` and legacy canonical `Rooted`/`Stunned` directly.

## Chosen Unreal Representation

`FWBUnitState::Statuses` remains authoritative for active status presence.

This pass adds:

```text
TMap<FName, int32> StatusTurnsRemaining
```

Helper methods now provide status access:

- `HasStatus`
- `AddStatus`
- `RemoveStatus`
- `GetStatusTurnsRemaining`
- `SetStatusTurnsRemaining`
- `GetSortedStatusIdsForTrace`

Canonical Unreal names for this pass are:

- `Burn`
- `Poison`
- `Rooted`
- `Stunned`

Compatibility aliases preserve existing lowercase fixture behavior:

- `root` aliases `Rooted`
- `stun` aliases `Stunned`
- `burn` aliases `Burn`
- `poison` aliases `Poison`
- `frozen` aliases `Frozen`

`TurnsRemaining <= 0` is treated as untimed/permanent metadata in Unreal, matching the practical meaning needed for fixtures and helper use. `Statuses` still decides whether a status is active.

## Unreal Behavior Implemented

`WBRules::CanApplyStartOfTurnStatusTicks` validates:

- game is not over
- player id is valid
- player state exists
- requested player is the current player

`WBEffectRunner::ApplyStartOfTurnStatusTicks`:

- emits a parent `start_turn_status_ticks` trace
- ticks all units with active Poison
- skips Poison on Frozen units
- reduces MaxHP by one to a minimum of one
- clamps HP down to MaxHP
- decrements Poison duration when timed
- removes Poison and emits `status_expired` when duration reaches zero

`WBRules::CanApplyEndOfTurnStatusTicks` validates:

- game is not over
- player id is valid
- player state exists
- requested player is the current player

`WBEffectRunner::ApplyEndOfTurnStatusTicks`:

- emits a parent `end_turn_status_ticks` trace
- ticks all units with active Burn
- subtracts one HP for Burn, clamped to zero
- does not mutate MaxHP for Burn
- decrements timed Burn, Rooted, Stunned, and Frozen durations
- removes timed statuses and emits `status_expired` when duration reaches zero
- marks Burn trace events with `at_or_below_zero_hp` when HP reaches zero

## Open Questions/TODOs

- Full death/prevention resolution after status ticks remains TODO.
- End-turn status ticks are not yet wired into `ApplyEndTurn`; they remain a separate deterministic phase operation until replay/turn orchestration is extended.
- Draw, random MP, start-turn hooks, card triggers, and NPC phase remain TODO.
- Godot currently ignores `active_player_id` in status ticks. If the canon later changes to owner-filtered ticks, update Unreal fixtures and tests.
