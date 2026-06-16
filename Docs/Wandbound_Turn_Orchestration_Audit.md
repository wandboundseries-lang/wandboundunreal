# Wandbound Turn Orchestration Audit

Date: 2026-06-16

## Files Inspected

- `Source/WandboundCore/Public/WBGameStateData.h`
- `Source/WandboundCore/Private/WBGameStateData.cpp`
- `Source/WandboundCore/Public/WBRules.h`
- `Source/WandboundCore/Private/WBRules.cpp`
- `Source/WandboundCore/Public/WBEffectRunner.h`
- `Source/WandboundCore/Private/WBEffectRunner.cpp`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundCore/Public/WBAction.h`
- `Source/WandboundCore/Private/WBActionCodec.cpp`
- `Source/WandboundTests/Private/`

## Existing Turn APIs

Current available phase/effect APIs:

- `WBEffectRunner::ApplyEndOfTurnStatusTicks`
- `WBEffectRunner::ApplyEndTurn`
- `WBEffectRunner::ApplyStartOfTurnStatusTicks`
- `WBEffectRunner::ApplyTurnStartResourceSetup`
- `WBRules::CanApplyEndOfTurnStatusTicks`
- `WBRules::CanEndTurn`
- `WBRules::CanApplyStartOfTurnStatusTicks`
- `WBRules::CanApplyTurnStartResourceSetup`

## Current ApplyEndTurn Behavior

`WBEffectRunner::ApplyEndTurn` is currently a basic turn-control mutation only.

It validates through `WBRules::QueryEndTurn`, then calls `FWBGameStateData::AdvanceTurnBasic`.

Current `AdvanceTurnBasic` behavior:

- flips `CurrentPlayer` between `0` and `1`
- sets `PriorityPlayer` to the new `CurrentPlayer`
- sets `Phase` to `NormalTurn`
- increments `TurnNumber` by `1`

Current `ApplyEndTurn` trace behavior:

- emits exactly one `end_turn` trace
- records `PlayerId`
- records `FromPlayer`
- records `ToPlayer`
- records the new `TurnNumber`
- sets `bOk = true`

`ApplyEndTurn` does not currently perform:

- end-turn status ticks
- start-turn status ticks
- MP/resource setup
- card draw
- card triggers
- NPC phase
- death/prevention

## Existing Trace Event Kinds

Turn-related trace event kinds before this pass:

- `end_turn`
- `pass`
- `pass_response`
- `end_turn_status_ticks`
- `start_turn_status_ticks`
- `status_tick`
- `status_expired`
- `turn_start_resource_setup`

## Existing Test Assumptions

Existing tests assume:

- `ApplyEndTurn` flips active player and increments `TurnNumber` immediately.
- `ApplyEndTurn` emits a single `end_turn` trace.
- replay verifier tests for `EndTurn` use the basic `ApplyEndTurn` behavior.
- legal action generation appends `EndTurn` after movement in normal turn flow.
- start/end status tick operations are explicit phase operations, not player actions.
- turn-start resource setup is an explicit deterministic operation with an explicit MP roll.

## Chosen Orchestration API

This pass adds:

```text
WBEffectRunner::ApplyDeterministicTurnTransition(State, EndingPlayerId, NextPlayerExplicitMPRoll)
```

and the corresponding rule precheck:

```text
WBRules::CanApplyDeterministicTurnTransition(State, EndingPlayerId, NextPlayerExplicitMPRoll, OutReason)
```

## Chosen Sequence

The deterministic transition sequence is:

1. emit `turn_transition` parent trace
2. apply end-turn status ticks for the ending turn
3. apply the existing basic `EndTurn`
4. read the new current player
5. apply start-turn status ticks for the new current player
6. apply explicit turn-start resource setup for the new current player

The implementation uses a local `FWBGameStateData` copy and only commits it back to the caller state after all substeps succeed.

## Godot Reference Alignment

Godot turn structure performs end-turn status ticks before finishing the turn, and start-turn status ticks before draw/resource work. Unreal still lacks draw, card triggers, death/prevention, and NPC phase, so this pass composes only the deterministic rules-kernel pieces that already exist.

Current mismatch/TODO:

- Godot processes recent deaths after end-turn/start-turn status ticks; Unreal does not yet implement full death/prevention.
- Godot later draws and runs hooks; Unreal intentionally omits draw and triggers in this pass.
- Existing Unreal `TurnNumber` increments on every player switch. This pass preserves that baseline and documents it rather than changing turn-number policy.
- `ApplyEndTurn` remains basic and is not wired to the full transition yet.
