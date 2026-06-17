# Selected Action Runtime Execution Report

Date: 2026-06-16

## Summary

This pass adds a pure C++ selected-action execution gateway for runtime callers. The selected player action remains `FWBAction::EndTurn`, but the runtime caller can explicitly request that selected end-turn action execute the full deterministic turn transition by supplying a valid next-player MP roll.

## New API

```text
FWBSelectedActionExecutionContext
WBSelectedActionExecutor::ApplySelectedAction(State, Action, Context)
```

Context fields:

```text
bUseFullTurnTransitionForEndTurn
NextPlayerExplicitMPRoll
```

The executor is not an Actor, UObject, Blueprint, UMG widget, or runtime presentation object.

## Behavior

Selected `Move`:

- delegates to `WBEffectRunner::ApplyMove`
- does not use `WBTurnController`
- does not require an MP roll
- emits the existing `move` trace only

Selected `EndTurn` with full transition disabled:

- delegates to `WBEffectRunner::ApplyEndTurn`
- keeps basic player-action behavior
- ignores `NextPlayerExplicitMPRoll`
- emits the existing `end_turn` trace only

Selected `EndTurn` with full transition enabled:

- builds an `FWBTurnCommand`
- uses `EWBTurnCommandMode::DeterministicFullTransition`
- passes `Action.PlayerId` as `ActingPlayerId`
- requires `NextPlayerExplicitMPRoll` from `1` to `6`
- delegates to `WBTurnController::ApplyTurnCommand`
- emits the existing full transition trace sequence

Selected `PassResponse`:

- delegates to `WBEffectRunner::ApplyPassResponse`
- does not use full transition context
- emits the existing `pass_response` trace only

## Replay Boundary

`apply_action` replay fixtures remain unchanged. They still decode `FWBAction` through `WBActionCodec` and apply through `WBEffectRunner::ApplyAction`.

The selected-action executor is a runtime execution gateway. It is not used by existing `apply_action` replay fixtures.

## Legal Action Boundary

Legal action generation remains unchanged:

- movement first
- then `EndTurn`
- `PassResponse` in response phase

Legal action generation does not emit:

- `DeterministicFullTransition`
- `turn_transition`
- `roll`
- `draw`
- status ticks
- resource setup

## Action Codec Boundary

`WBActionCodec` remains unchanged and continues to encode player legal actions only. `EndTurn` still encodes as:

```text
end_turn:p0
```

No command context or MP roll is included in the action ID or serialized `FWBAction`.

## Fixtures Added

- `selected_action_end_turn_basic.json`
- `selected_action_end_turn_full_transition.json`
- `selected_action_end_turn_full_transition_invalid_roll.json`
- `selected_action_move_unchanged.json`
- `selected_action_pass_response_unchanged.json`

The fixture operation is:

```text
operation = apply_selected_action
```

This operation is parsed only by selected-action executor tests and does not alter `apply_action` replay behavior.

## Out of Scope

- UI/runtime wiring
- random MP roll generation
- draw
- NPC phase
- card triggers
- attacks
- combat
- full death/prevention
- network replay
- changing legal action IDs
