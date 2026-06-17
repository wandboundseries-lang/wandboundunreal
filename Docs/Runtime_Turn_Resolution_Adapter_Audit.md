# Runtime Turn Resolution Adapter Audit

Date: 2026-06-16

## Files Inspected

- `Source/WandboundCore/Public/WBSelectedActionExecutor.h`
- `Source/WandboundCore/Private/WBSelectedActionExecutor.cpp`
- `Source/WandboundCore/Public/WBTurnController.h`
- `Source/WandboundCore/Private/WBTurnController.cpp`
- `Source/WandboundCore/Public/WBAction.h`
- `Source/WandboundCore/Private/WBAction.cpp`
- `Source/WandboundCore/Public/WBActionCodec.h`
- `Source/WandboundCore/Private/WBActionCodec.cpp`
- `Source/WandboundCore/Public/WBRules.h`
- `Source/WandboundCore/Private/WBRules.cpp`
- `Source/WandboundCore/Public/WBEffectRunner.h`
- `Source/WandboundCore/Private/WBEffectRunner.cpp`
- `Source/WandboundTests/Private/`

## Current WBSelectedActionExecutor Behavior

`WBSelectedActionExecutor::ApplySelectedAction` is a pure C++ selected-action gateway. It delegates selected player actions to existing core execution paths:

- `Move` -> `WBEffectRunner::ApplyMove`
- `EndTurn` basic -> `WBEffectRunner::ApplyEndTurn`
- `EndTurn` full transition -> `WBTurnController::ApplyTurnCommand`
- `PassResponse` -> `WBEffectRunner::ApplyPassResponse`
- `Pass` -> `WBEffectRunner::ApplyPass`

The full-transition `EndTurn` branch requires an explicit `NextPlayerExplicitMPRoll` in `FWBSelectedActionExecutionContext`. It does not generate random rolls.

## Current WBTurnController Behavior

`WBTurnController` supports:

- `BasicEndTurn`
- `DeterministicFullTransition`

`BasicEndTurn` validates through `WBRules::CanEndTurn` and delegates to `WBEffectRunner::ApplyEndTurn`.

`DeterministicFullTransition` validates through `WBRules::CanApplyDeterministicTurnTransition` and delegates to `WBEffectRunner::ApplyDeterministicTurnTransition`.

The controller does not generate rolls. It consumes the explicit roll already present on `FWBTurnCommand`.

## Current Legal Action Generation Behavior

`WBRules::GenerateLegalActionsForPlayer` still emits player legal actions only.

Normal turn ordering remains:

1. deterministic movement actions
2. `EndTurn`

Response phase emits `PassResponse` for the priority player.

Legal generation does not emit:

- `DeterministicFullTransition`
- `turn_transition`
- MP roll actions
- draw
- status ticks
- resource setup

## Current WBActionCodec Behavior

`WBActionCodec` encodes and decodes player legal actions only:

- `Move`
- `EndTurn`
- `Pass`
- `PassResponse`

`EndTurn` still encodes as `end_turn:pN`. Runtime context, command mode, and MP roll are not part of the action ID or serialized `FWBAction`.

## Current replay apply_action Behavior

Replay `apply_action` still uses:

```text
WBActionCodec -> WBEffectRunner::ApplyAction
```

That path keeps `EndTurn` basic and does not call `WBSelectedActionExecutor` or `WBTurnController` full transition behavior.

## Why This Adapter Is Needed

The selected-action executor can already expand a selected `EndTurn` into a full deterministic transition, but it requires an explicit MP roll in its context.

A runtime-facing adapter should supply that context from a deterministic roll source while preserving the player-selected action as `FWBAction::EndTurn`. This gives future runtime/UI code one place to translate selected actions into deterministic execution context without changing legal actions or action IDs.

## Why Explicit Deterministic Roll Sources

This pass adds fixed and queued roll sources so tests can prove runtime end-turn resolution end-to-end without randomness.

The roll source is intentionally deterministic:

- no random dice generation
- no default fallback roll
- no silent basic-end-turn fallback when full transition is requested
- no game state mutation by the roll source

## Out of Scope

- random MP roll provider
- UI/player-controller wiring
- Blueprint gameplay
- 3D runtime
- draw
- NPC phase
- card triggers
- attacks
- card effects
- full death/prevention
- network replay

## Open TODOs

- Add a random roll source after deterministic adapter behavior is proven.
- Decide how UI/runtime code owns and passes a roll source.
- Add runtime/UI integration for selected actions in a later pass.
- Keep `apply_action` replay unchanged until a dedicated replay migration pass.
- Continue using Godot reference behavior as read-only canon for future turn phases.
