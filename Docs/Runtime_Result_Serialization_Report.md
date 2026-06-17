# Runtime Result Serialization Report

Date: 2026-06-17

## Purpose

This pass adds deterministic JSON serialization for `FWBRuntimeSelectedActionResult`.

The serializer is reporting-only. It does not mutate game state, change legal action generation, change action IDs, or alter replay `apply_action` behavior.

## Serializer

Added:

```text
WBRuntimeResultSerializer
Source/WandboundCore/Public/WBRuntimeResultSerializer.h
Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp
```

Schema version:

```text
WBRuntimeResultSerializer::RuntimeSelectedActionResultSchemaVersion = 1
```

Public API:

```text
RuntimeSelectedActionResultToJsonObject(Result)
RuntimeSelectedActionResultToJsonString(Result, OutJson)
```

## JSON Fields

The serialized shape is:

```text
schema_version
selected_action.type
selected_action.action_id
result.ok
result.reason
mp_roll.consumed
mp_roll.value
traces
final_public_turn_summary
final_public_board_summary
```

All serialized field names use snake_case.

## Action ID Stability

The serializer reads `FWBRuntimeSelectedActionResult::SelectedActionId`.

It does not recompute action IDs and does not include runtime context, MP roll values, or full-transition wording in action IDs.

`WBActionCodec` is unchanged.

## MP Roll Fields

The serialized `mp_roll` object includes:

- `consumed`
- `value`

For basic `EndTurn`, `Move`, and `PassResponse`, `consumed` is false and `value` remains 0.

For full-transition `EndTurn`, the deterministic consumed roll is reported.

## Trace Serialization

`WBReplayTrace::TraceEventToJsonObject` is now public and reused by `WBRuntimeResultSerializer`.

Trace generation is unchanged. The serializer only converts existing trace events to JSON.

## Final Public Turn Summary

The serialized final summary includes:

- `current_player_id`
- `priority_player_id`
- `turn_number`
- `phase`
- `game_over`
- per-player `player_id`
- per-player `remaining_mp`
- per-player `last_mp_roll`
- per-player `walls_left`
- per-player `wall_removals_left`

It does not include deck, hand, discard, hidden markers, or pending choices.

## Final Public Board Summary

The serialized final public board summary includes:

- `board_width`
- `board_height`
- `default_terrain_id`
- visible unit `unit_id`
- visible unit `owner_id`
- visible unit `card_id`
- unit `x` and `y`
- HP/max HP
- ATK/AR
- RL total/used
- attacks left
- public statuses with turns remaining
- public normalized wall edges in `walls`
- sparse public non-default terrain tiles in `terrain_tiles`

Visible unit `card_id` is included because Godot public observations expose visible board unit identity.

Wall JSON fields:

- `ax`
- `ay`
- `bx`
- `by`
- `orientation`

Terrain JSON fields:

- `x`
- `y`
- `terrain_id`

Terrain serialization is sparse and uses `default_terrain_id` for omitted default tiles.

Hidden marker identities are excluded. Unreal does not yet have marker state, and marker visibility must be player-perspective scoped when implemented.

## Hidden Data Exclusions

Tests plant secret tokens in:

- player deck
- player hand
- player discard

The serialized runtime result is asserted not to contain those tokens.

Visible unit card ids are public and are asserted to serialize.

## Unchanged Systems

Unchanged:

- legal action generation
- `WBActionCodec`
- replay `apply_action`
- runtime result envelope behavior
- trace generation behavior

Not added:

- random dice generation
- draw
- NPC phase
- card triggers
- attacks
- card effects
- full death/prevention
- UI
- Blueprint gameplay
- 3D runtime

## Fixtures

Added:

- `runtime_result_serialization_full_transition.json`
- `runtime_result_serialization_basic_end_turn.json`
- `runtime_result_serialization_hidden_data_exclusion.json`
- `runtime_result_serialization_public_board_summary_after_move.json`
- `runtime_result_serialization_public_board_summary_after_full_turn.json`
- `runtime_result_serialization_public_wall_terrain_after_move.json`
- `runtime_result_serialization_public_wall_terrain_after_full_turn.json`

Added optional fixture operation:

```text
apply_runtime_selected_action_with_result_and_serialize
```

Existing fixture operations remain unchanged.

## Future TODO

- Add marker summaries once hidden/revealed marker policy is represented in Unreal state.
- Add public wall placement/removal action traces once wall actions exist.
- Add terrain movement/effect rules once terrain gameplay parity begins.
- Add a network replay envelope separate from existing `apply_action` replay.
- Add a deterministic random roll source only after reporting remains stable.
- Add UI/runtime consumers after the C++ JSON contract is proven.
