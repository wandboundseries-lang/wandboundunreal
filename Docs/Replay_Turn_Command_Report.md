# Replay Turn Command Report

Date: 2026-06-16

## Summary

This pass adds deterministic replay fixture coverage for turn commands without changing player action replay. Full deterministic turn transition remains a controller-command/test fixture operation, not a legal player action.

## Fixture Schema Added

New internal fixture operation:

```json
{
  "operation": "apply_turn_command",
  "mode": "basic_end_turn",
  "acting_player_id": 0,
  "initial_state": {},
  "expected": {}
}
```

Supported modes:

- `basic_end_turn`
- `deterministic_full_transition`

`next_player_explicit_mp_roll` is required only for `deterministic_full_transition`.

Expected fields used by the tests include:

- `ok`
- `reason`
- `final_current_player`
- `final_priority_player`
- `final_turn_number`
- `final_phase`
- `player_resources`
- `final_units`
- `expected_trace_order`

## Player Action Replay Boundary

`apply_action` fixtures still use `WBActionCodec` and the existing `FWBAction` path.

`apply_turn_command` fixtures parse `FWBTurnCommand` and call `WBTurnController::ApplyTurnCommand`.

The two paths are intentionally separate:

- player legal action replay stays stable
- command fixture replay can cover full turn orchestration
- legal action generation does not emit full transition commands
- `WBActionCodec` does not encode full transition commands

## Command Fixture Coverage

Added fixtures:

- `replay_turn_command_basic_end_turn_only.json`
- `replay_turn_command_full_transition_burn_poison_setup.json`
- `replay_turn_command_full_transition_invalid_roll_no_mutation.json`
- `replay_turn_command_does_not_change_action_replay.json`

The fixtures cover:

- basic end turn emits only the basic `end_turn` trace
- full transition emits the deterministic parent/substep trace sequence
- invalid explicit MP roll fails without mutation
- player `EndTurn` action replay remains basic

## Trace Order Expectations

Basic command expected order:

```text
end_turn
```

Full transition expected order:

```text
turn_transition
end_turn_status_ticks
status_tick:Burn
end_turn
start_turn_status_ticks
status_tick:Poison
turn_start_resource_setup
```

Invalid command expected order:

```text
<no trace events>
```

## Trace Serialization

The new tests serialize full-transition replay traces and assert that JSON contains the relevant public event data:

- `turn_transition`
- `end_turn_status_ticks`
- `status_tick` for Burn
- `end_turn`
- `start_turn_status_ticks`
- `status_tick` for Poison
- `turn_start_resource_setup`
- explicit MP roll and resulting resource counters

The serialization checks do not require hidden player data.

## Out of Scope

- network replay
- UI/runtime wiring
- random dice generation
- draw
- NPC phase
- card triggers
- attacks
- combat
- full death/prevention
- changing player legal action IDs
- exposing full transition as a legal player action
