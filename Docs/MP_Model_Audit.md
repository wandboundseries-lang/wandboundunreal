# MP Model Audit

Date: 2026-06-16

## Scope

This audit covers the deterministic turn-start resource setup pass for WandboundCore. Godot reference files were inspected as read-only source material; no Godot files were modified, imported, compiled, or loaded by Unreal.

## Godot Source Files Inspected

- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/shared/action_schema.gd`
- `Reference/GodotProject/godotcanon/Wandbound_Rules_Bible_v2.txt`
- `Reference/GodotProject/godotcanon/Wandbound_Canonical_Glossary.txt`
- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotCanon/GoldenScenarios/`

## Godot MP Model

Godot models MP as a player-level pool:

- `scripts/sim/game_state.gd` defines `mp_pool: Array = [0, 0]`.
- `autoload/Game.gd:start_turn_impl` rolls MP and writes the result to `state.mp_pool[current_player]`.
- `scripts/sim/rules.gd` movement application checks `gs.mp_pool[player_id]` against the move cost and decrements `gs.mp_pool[player_id]` after movement.
- Godot unit state contains attack budget fields such as `attacks_left` and `max_attacks`, but movement MP is not a canonical per-unit spend pool.

The Rules Bible also describes turn start as rolling `1d6 MP` and movement as spending MP, without assigning MP to individual units.

## Current Unreal Baseline Before This Pass

The Unreal baseline contained both:

- player `FWBPlayerStateData::RemainingMP`, added as future storage in the previous turn-flow pass
- unit `FWBUnitState::MPRemaining`, used by the movement baseline and early GodotCanon fixtures

Movement legality and spending previously used `FWBUnitState::MPRemaining`.

## Mismatch

There was a real mismatch:

- Godot runtime canon: player-level `mp_pool`
- Unreal baseline and early fixtures: unit-level `mp_remaining`

The fixture `mp_remaining` fields are treated as migration scaffolding for existing movement/replay scenarios, not as the long-term rules truth.

## Chosen Unreal Direction

WandboundCore now treats player `RemainingMP` as the rules truth for movement legality and spending.

Compatibility retained for this pass:

- `FWBUnitState::MPRemaining` remains as a legacy fixture mirror.
- `AddUnitForTest` seeds a missing test player state from a legacy unit `MPRemaining` value.
- `ApplyMove` decrements the legacy unit mirror after spending player `RemainingMP` so existing replay fixture final-state checks remain stable.

This keeps existing movement fixtures useful while moving rules behavior toward Godot's player-pool model.

## Migration Impact

- Movement legality now fails with `insufficient_mp` when the acting player's `RemainingMP <= 0`.
- Legal action generation omits move actions when the acting player has no remaining MP.
- Existing tests and fixtures continue to pass through the legacy fixture bridge.
- New turn-start fixtures use player `remaining_mp`, `last_mp_roll`, `walls_left`, and `wall_removals_left`.

## TODOs

- Remove or stop asserting the legacy unit `mp_remaining` mirror once all movement/replay fixtures have migrated to player `remaining_mp`.
- Add full turn-start sequencing later: draw, status ticks, start-of-turn triggers, MP modifiers, and NPC phase.
- Confirm whether wall removal unlock timing remains `turn_number >= 30` in every future mode, or whether modes need a separate policy field.
