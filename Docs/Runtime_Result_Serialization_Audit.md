# Runtime Result Serialization Audit

Date: 2026-06-17

## Files Inspected

- `Source/WandboundCore/Public/WBRuntimeTurnResolutionAdapter.h`
- `Source/WandboundCore/Private/WBRuntimeTurnResolutionAdapter.cpp`
- `Source/WandboundCore/Public/WBPublicTurnSummary.h`
- `Source/WandboundCore/Private/WBPublicTurnSummary.cpp`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundCore/Public/WBActionCodec.h`
- `Source/WandboundCore/Private/WBActionCodec.cpp`
- `Source/WandboundCore/Public/WBGameStateData.h`
- `Source/WandboundCore/WandboundCore.Build.cs`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.h`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`
- `Source/WandboundTests/Private/WBRuntimeTurnResultEnvelopeTests.cpp`

## Existing Trace Serialization

`WBReplayTrace` already serializes trace events to JSON strings through `SerializeEvent` and `SerializeEvents`.

The object conversion helper currently exists in `WBReplayTrace.cpp` as a private helper. It serializes existing machine-readable trace fields only:

- `kind`
- `ok`
- `player_id`
- `action_id`
- unit, player, turn, MP, wall, status, HP, status-duration, tile, and reason fields when present
- `expired_status` and `at_or_below_zero_hp` flags when true

This pass should expose the trace event object helper publicly and keep all trace field logic in `WBReplayTrace`.

## Existing Fixture JSON Helpers

`WBReplayFixtureTestUtils` already builds `FWBGameStateData` from GodotCanon fixtures, parses runtime selected actions, applies `ApplyRuntimeSelectedActionWithResult`, and validates envelope expectations.

The fixture utility already supports:

- `apply_action`
- `apply_turn_command`
- `apply_runtime_selected_action`
- `apply_runtime_selected_action_with_result`

Adding a serialization fixture operation is optional and can reuse the existing runtime selected-action parsing path.

## Json Module Dependency

`WandboundCore.Build.cs` already has `Json` as a private dependency.

`WandboundTests.Build.cs` also already has `Json` as a private dependency.

No new module dependency is required.

## Chosen Serializer Names

Add a dedicated pure C++ serializer:

```text
Source/WandboundCore/Public/WBRuntimeResultSerializer.h
Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp
WBRuntimeResultSerializer
```

Schema version constant:

```text
WBRuntimeResultSerializer::RuntimeSelectedActionResultSchemaVersion = 1
```

## Chosen JSON Schema

The serialized runtime selected-action result will use:

```text
schema_version
selected_action.type
selected_action.action_id
result.ok
result.reason
mp_roll.consumed
mp_roll.value
traces
final_public_turn_summary.current_player_id
final_public_turn_summary.priority_player_id
final_public_turn_summary.turn_number
final_public_turn_summary.phase
final_public_turn_summary.game_over
final_public_turn_summary.players[]
```

All field names are snake_case.

## Hidden-Information Exclusions

The serializer must not include:

- deck contents
- hand contents
- discard contents
- private card IDs
- hidden marker identities
- hidden pending choices
- hidden response options
- raw internal object pointers or resources
- runtime context fields
- file paths

`FWBPublicTurnSummary` currently contains only public turn and resource counters. It does not include `Deck`, `Hand`, `Discard`, unit `CardId`, or runtime context.

## Why Action IDs Remain Unchanged

The serializer reads `FWBRuntimeSelectedActionResult::SelectedActionId`. That value is already produced by `WBActionCodec::MakeActionId` inside the runtime result envelope.

This pass does not alter `WBActionCodec`, does not recompute action IDs from runtime context, and does not add MP roll values or full-transition wording to action IDs.

## Why Replay `apply_action` Remains Unchanged

Replay verification still uses:

```text
WBActionCodec -> WBEffectRunner::ApplyAction
```

Runtime result serialization is reporting-only and does not participate in replay `apply_action` execution.

## Open TODOs

- Add a future public board summary after the hidden-information policy is explicit for unit and marker fields.
- Add a future network replay envelope separate from current `apply_action` replay verification.
- Add a random roll source only after deterministic runtime reporting remains stable.
- Add UI/runtime consumers after C++ serialization is proven.
