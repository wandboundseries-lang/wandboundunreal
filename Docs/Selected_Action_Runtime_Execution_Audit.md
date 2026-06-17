# Selected Action Runtime Execution Audit

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
- `Source/WandboundCore/Public/WBTurnController.h`
- `Source/WandboundCore/Private/WBTurnController.cpp`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.h`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`
- `Source/WandboundTests/Private/`

## Existing Action Application

`WBEffectRunner` already has a central player-action dispatcher:

```text
WBEffectRunner::ApplyAction(State, Action)
```

The dispatcher routes:

- `Move` to `ApplyMove`
- `EndTurn` to `ApplyEndTurn`
- `Pass` to `ApplyPass`
- `PassResponse` to `ApplyPassResponse`

Unsupported action types fail with `unsupported_action_kind` and do not mutate state.

## Existing Move Path

`ApplyMove` validates through `WBRules::QueryMove`, mutates the unit tile, spends player-level `RemainingMP`, mirrors legacy unit MP, and emits one `move` trace event.

This is already deterministic and does not require any turn-command context.

## Existing EndTurn Path

`ApplyEndTurn` validates through `WBRules::QueryEndTurn`, calls `FWBGameStateData::AdvanceTurnBasic`, and emits one `end_turn` trace event.

It does not run:

- end-turn status ticks
- start-turn status ticks
- turn-start resource setup
- draw
- hooks
- NPC phase
- death/prevention

## Existing Pass/PassResponse Path

`ApplyPass` and `ApplyPassResponse` are still player-action mutations. `PassResponse` validates through `WBRules::QueryPassResponse`, returns priority to the current player, sets phase to normal turn, and emits one `pass_response` trace event.

Neither path should involve full turn transition behavior.

## Legal Action Generation

`WBRules::GenerateLegalActionsForPlayer` still emits player legal actions only.

Normal turn order:

1. legal movement actions in deterministic unit/direction order
2. `EndTurn`

Response phase:

1. `PassResponse` for the priority player

Other priority-only states may emit `Pass`.

Legal action generation does not emit:

- full transition commands
- status ticks
- resource setup
- dice roll
- draw
- controller commands

## Replay apply_action Behavior

`apply_action` replay fixtures still decode `FWBAction` through `WBActionCodec` and apply through `WBEffectRunner::ApplyAction`.

That means replayed `EndTurn` remains the basic `ApplyEndTurn` player action and does not run full deterministic transition behavior.

## Turn Command Fixture Behavior

`apply_turn_command` fixtures parse `FWBTurnCommand` and call `WBTurnController::ApplyTurnCommand`.

This is separate from `apply_action`, separate from legal action generation, and separate from `WBActionCodec`.

`WBTurnController` supports:

- `BasicEndTurn`
- `DeterministicFullTransition`

`DeterministicFullTransition` delegates to `WBEffectRunner::ApplyDeterministicTurnTransition`, which composes end-turn status ticks, basic end turn, start-turn status ticks, and turn-start resource setup.

## Chosen Runtime Gateway

This pass should add:

```text
WBSelectedActionExecutor::ApplySelectedAction(State, Action, Context)
```

The gateway is for runtime-selected player actions. It preserves the selected action as `FWBAction::EndTurn`, then optionally expands that selected end-turn action into a full deterministic turn command when the caller explicitly supplies:

- `bUseFullTurnTransitionForEndTurn = true`
- valid `NextPlayerExplicitMPRoll` from `1` to `6`

## Why Action Codec and Legal Generation Stay Unchanged

`FWBAction` is the player legal-action surface and `WBActionCodec` is the stable player action ID/serialization layer. Adding full transition to either would change replay IDs and player-visible legal action semantics.

The selected-action executor is a runtime execution gateway, not a new action kind. It lets runtime code choose full orchestration after a player-selected `EndTurn` without turning full orchestration into a player action.

## Open TODOs

- Wire UI/runtime selection code to call the selected-action executor in a later pass.
- Decide where deterministic MP roll input comes from before replacing explicit test rolls with random runtime rolls.
- Keep network replay separate until selected-action and command replay semantics are settled.
- Add draw, NPC phase, card triggers, attacks, and full death/prevention in separate rules-kernel passes.
- Keep Godot reference files read-only.
