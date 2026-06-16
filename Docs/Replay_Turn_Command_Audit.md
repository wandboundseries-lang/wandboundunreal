# Replay Turn Command Audit

Date: 2026-06-16

## Files Inspected

- `Source/WandboundCore/Public/WBAction.h`
- `Source/WandboundCore/Private/WBAction.cpp`
- `Source/WandboundCore/Public/WBActionCodec.h`
- `Source/WandboundCore/Private/WBActionCodec.cpp`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundCore/Public/WBTurnController.h`
- `Source/WandboundCore/Private/WBTurnController.cpp`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.h`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`
- `Reference/GodotCanon/GoldenScenarios/replay_*.json`
- `Reference/GodotCanon/GoldenScenarios/turn_command_*.json`
- `Reference/GodotCanon/GoldenScenarios/turn_transition_*.json`

## Current FWBAction Replay

`FWBAction` replay remains the player legal-action path. Legal actions are encoded and decoded through `WBActionCodec`, then applied through the existing effect runner action path.

Current player action IDs remain:

- `move:<unit>:<x>,<y>`
- `end_turn:pN`
- `pass:pN`
- `pass_response:pN`

`EndTurn` action replay still means the basic player action only. It calls the existing `WBEffectRunner::ApplyEndTurn`, emits the existing `end_turn` trace, advances the active player, and does not run end-turn status ticks, start-turn status ticks, or turn-start resource setup.

## WBActionCodec Scope

`WBActionCodec` currently supports player legal actions only. It intentionally does not encode:

- full deterministic turn transitions
- status tick phase operations
- resource setup phase operations
- controller commands
- dice generation
- draw
- hooks or card triggers

That boundary should remain intact until player replay semantics are explicitly redesigned.

## Existing Fixture Loading

Replay and canon fixtures are loaded from `Reference/GodotCanon/GoldenScenarios/` by the Wandbound automation tests. The fixture utilities now share more of the low-level parsing for:

- initial game state
- fixture operation type
- turn command parsing
- expected trace order
- expected player resources
- expected unit status summaries
- expected final turn state

Existing action fixture behavior is preserved. When a fixture operation is `apply_action`, the fixture uses `WBActionCodec` and the player action application path.

## Controller Command Fixture Support

Fixtures now have an explicit controller-command operation:

```json
{
  "operation": "apply_turn_command",
  "mode": "deterministic_full_transition",
  "acting_player_id": 0,
  "next_player_explicit_mp_roll": 4
}
```

Supported command modes:

- `basic_end_turn`
- `deterministic_full_transition`

`apply_turn_command` fixtures parse an `FWBTurnCommand` and call `WBTurnController::ApplyTurnCommand`. They do not route through `WBActionCodec`, do not appear in legal action lists, and are internal test/replay fixture data only.

## Why Full Transition Stays Outside Player Replay

`DeterministicFullTransition` composes multiple deterministic phase steps:

- end-turn status ticks
- basic end turn
- start-turn status ticks
- turn-start resource setup

Exposing that as a normal player legal action would change action IDs, replay semantics, trace order, and resource timing. This pass keeps it as an explicit controller/test fixture operation so full turn behavior can be replay-tested without changing player-facing action replay.

## Hidden Information Boundary

The new fixtures assert public deterministic state and trace fields only:

- turn ownership
- phase
- resource counters
- unit HP and statuses
- trace event kind/order
- explicit MP roll supplied by the fixture

They do not serialize deck, hand, hidden draw, hidden card identity, or future random generation.

## Open TODOs

- Decide how runtime/UI will call full turn orchestration after a selected player `EndTurn`.
- Add network replay semantics after the local deterministic fixture format stabilizes.
- Add random dice generation only after deterministic explicit-roll coverage remains green.
- Add draw, NPC phase, card triggers, and death/prevention in separate rules-kernel passes.
- Keep `FWBAction` legal action replay unchanged until a dedicated replay migration pass.
