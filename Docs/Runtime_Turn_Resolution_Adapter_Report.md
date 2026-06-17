# Runtime Turn Resolution Adapter Report

Date: 2026-06-16

## Summary

This pass adds a deterministic runtime-facing adapter that supplies explicit MP rolls to selected `EndTurn` execution. The adapter keeps the selected action as `FWBAction::EndTurn`, asks a deterministic roll source for the next player's MP roll, and then delegates to `WBSelectedActionExecutor`.

## New MP Roll Source API

Added:

```text
IWBMPRollSource
FWBFixedMPRollSource
FWBQueuedMPRollSource
```

Roll source rules:

- valid rolls are `1..6`
- fixed invalid rolls fail with `invalid_mp_roll`
- empty queued rolls fail with `mp_roll_queue_empty`
- invalid queued rolls fail with `invalid_mp_roll`
- invalid queued rolls are not popped
- roll sources do not mutate game state
- no randomization is performed

## New Runtime Adapter API

Added:

```text
FWBRuntimeTurnResolutionContext
WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(State, SelectedAction, Context)
```

Context fields:

```text
bResolveEndTurnAsFullTransition
MPRollSource
```

## Selected EndTurn Paths

Basic mode:

- `bResolveEndTurnAsFullTransition = false`
- no roll source required
- delegates to `WBSelectedActionExecutor`
- selected executor delegates to `WBEffectRunner::ApplyEndTurn`
- emits only the existing `end_turn` trace

Full transition mode:

- `bResolveEndTurnAsFullTransition = true`
- requires `MPRollSource`
- requests a deterministic roll for the next player
- delegates to `WBSelectedActionExecutor`
- selected executor delegates through `WBTurnController`
- emits the existing full deterministic transition trace sequence

## Move and PassResponse

Runtime-selected `Move` and `PassResponse` delegate to `WBSelectedActionExecutor` with default selected-action context.

They do not require or consume a roll source.

## Replay and Action Boundaries

Legal action generation remains unchanged.

`WBActionCodec` remains unchanged and continues to encode `EndTurn` as:

```text
end_turn:p0
```

Replay `apply_action` remains unchanged and still uses:

```text
WBActionCodec -> WBEffectRunner::ApplyAction
```

## Fixtures Added

- `runtime_selected_end_turn_full_transition_roll_4.json`
- `runtime_selected_end_turn_basic_no_roll.json`
- `runtime_selected_end_turn_missing_roll_source_fails.json`
- `runtime_selected_end_turn_invalid_roll_no_mutation.json`
- `runtime_selected_move_does_not_consume_roll.json`
- `runtime_selected_pass_response_does_not_consume_roll.json`

New fixture operation:

```text
operation = apply_runtime_selected_action
```

This operation is additive and does not alter `apply_action` or `apply_turn_command`.

## Out of Scope

- random roll source
- UI/runtime player-controller wiring
- Blueprint gameplay
- 3D runtime
- draw
- NPC phase
- card triggers
- attacks
- card effects
- full death/prevention
- network replay
