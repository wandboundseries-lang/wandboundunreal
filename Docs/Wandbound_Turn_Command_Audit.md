# Wandbound Turn Command Audit

Date: 2026-06-16

## Files Inspected

- `Source/WandboundCore/Public/WBAction.h`
- `Source/WandboundCore/Private/WBAction.cpp`
- `Source/WandboundCore/Public/WBActionCodec.h`
- `Source/WandboundCore/Private/WBActionCodec.cpp`
- `Source/WandboundCore/Public/WBEffectRunner.h`
- `Source/WandboundCore/Private/WBEffectRunner.cpp`
- `Source/WandboundCore/Public/WBRules.h`
- `Source/WandboundCore/Private/WBRules.cpp`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundTests/Private/`

## Existing EndTurn Action Behavior

`EWBActionType::EndTurn` remains the player/action-level end-turn action. It is encoded by `WBActionCodec` as `end_turn:pN` and is applied by `WBEffectRunner::ApplyEndTurn`.

Current `ApplyEndTurn` behavior:

- validates with `WBRules::QueryEndTurn`
- calls `FWBGameStateData::AdvanceTurnBasic`
- flips `CurrentPlayer`
- sets `PriorityPlayer` to the new current player
- normalizes `Phase` to `NormalTurn`
- increments `TurnNumber`
- emits exactly one `end_turn` trace

It does not apply status ticks, resource setup, draw, hooks, NPC phase, attacks, card effects, or death/prevention processing.

## Existing Full Transition Behavior

`WBEffectRunner::ApplyDeterministicTurnTransition` is the full deterministic rules-kernel sequence currently available.

The sequence is:

1. `ApplyEndOfTurnStatusTicks`
2. `ApplyEndTurn`
3. `ApplyStartOfTurnStatusTicks`
4. `ApplyTurnStartResourceSetup`

It emits the existing `turn_transition` parent trace followed by the substep traces. It validates first and applies through a copied state so invalid input does not partially mutate the caller state.

## Current Legal Action Generation

Legal action generation still emits player actions only:

- `Move`
- `EndTurn`
- `Pass`
- `PassResponse`

It does not emit `turn_transition`, resource setup, status ticks, rolls, draw, or any full-transition command. Normal-turn generation keeps movement first and appends `EndTurn` after movement.

## Why Add a Command Layer

The rules kernel now has both:

- a basic player/action end-turn mutation
- a full deterministic turn sequence for tests and later runtime orchestration

`WBTurnController` makes that choice explicit without changing existing action IDs or player-facing legal action semantics. This gives tests and future runtime code a deterministic gateway for full turn transitions while preserving current replay and legal action behavior.

## Why Player-Facing EndTurn Is Not Rewired Yet

Rewiring `EndTurn` directly to the full transition would change existing action/replay behavior, trace output, and resource timing. This pass intentionally keeps `EndTurn` basic until UI/runtime integration and replay semantics are explicitly tested.

## Open TODO

Future UI/runtime integration should decide when a selected player `EndTurn` action should be followed by, or translated into, a full deterministic turn transition. That future pass must preserve the boundary:

- Rules validate legality.
- EffectRunner mutates state.
- UI selects only.
