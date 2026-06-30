# Summon Execution Report

Date: 2026-06-29

## Purpose

This pass adds deterministic own-Hand Character summon execution from provider-supplied summon options.

It moves a validated Character card instance from the viewer's Hand into a new board unit, emits safe trace data, and refreshes production summon/equip provider data after success.

## Core Summon Execution Helper

Added `WBSummonExecution` in `WandboundCore`.

Primary API:

- `ExecuteCharacterSummonFromHand`
- `SummonExecutionResultToJsonStringForTest`
- `ResultCodeToString`

The helper mutates `FWBGameStateData` only after all validation passes. It does not mutate `FWBCardDefinitionRepository`.

## Runtime Summon Handoff

Added `FWBProductionSummonExecutionHandoff` in `WandboundRuntime`.

The handoff consumes:

- mutable game state
- const card definition repository
- current `FWBProductionSummonEquipDecisionData`
- selected source instance/card id
- selected target tile

It validates the selected source/tile against provider data before calling the core helper.

## Provider-Option Validation

Runtime handoff requires an exact provider option match:

- `SourceInstanceId`
- `SourceCardId`
- `TargetTile` contained in the provider option's `LegalTiles`

Missing source options fail with `summon_option_missing`. Tiles not present in provider legal tiles fail with `target_tile_not_allowed`.

Core execution still independently revalidates player, zones, source card, definition, Hero, unit cap, target bounds, adjacency, occupancy, marker placeholders, and unit id allocation.

## Character Stat Mapping

Character stats map into the created unit as:

- `HP` -> `HP` and `MaxHP`
- `ATK` -> `ATK`
- `AR` -> `AR`
- `RL` -> `RLTotal`
- `RLUsed` -> `0`
- `OwnerId` -> summoning player
- `CardId` -> source card id
- `X/Y` -> selected target tile

The created unit is not a Hero and receives default empty status/passive state.

## UnitId Allocation

This pass uses deterministic allocation:

- scan all existing units
- assign `max(UnitId) + 1`
- fail with `unit_id_allocation_failed` if allocation cannot produce a valid id

No shared production allocator was added yet.

## Hand-To-Board Movement

On success:

- the source card instance is removed from the player's Hand
- remaining Hand entries are sorted deterministically and reindexed `0..N-1`
- the card is not added to Discard
- board card references derive from the created unit

On failure, no unit is created, no hand card is removed, and no success trace event is emitted.

## Trace / Replay Event

Successful execution emits one trace event:

- `event_type`: `summon_unit`
- `player_id`
- `source_instance_id`
- `source_card_id`
- `created_unit_id`
- `target_tile`

The test JSON helper serializes the result and trace events for deterministic snapshot-style assertions.

## Provider Refresh

After successful runtime handoff, `FWBProductionSummonEquipDataProvider` is rebuilt from the updated state and repository.

Refresh behavior proves:

- the used Hand card no longer appears as a summon option
- unit cap changes suppress later summon options
- a newly summoned unit can become eligible for own-Hand Wand equip options when it has enough available RL

Activation-provider refresh is deferred.

## Marker Tile Deferred Policy

Summoning onto a marker placeholder fails with `marker_trigger_deferred`.

Marker reveal, marker trigger, trap behavior, NPC scheduling, and marker-triggered summon behavior remain future work.

## Hidden-Info Policy

Results, trace events, and refreshed provider data do not expose opponent Hand identities, Deck identities, hidden marker internal card ids, raw source JSON, debug activation ids, usage keys, or hidden fixture metadata.

Safe output may include the selected own-Hand source card id/instance id and the created public board unit id/card id.

## Boundary Confirmation

- No equip execution.
- No overflow resolution.
- No summon effects.
- No marker triggers.
- No UI or runtime visual changes.
- No response windows.
- No `FWBAction` creation.
- No `WBActionCodec` changes.
- No `WBRules::GenerateLegalActions` changes.
- No Godot CardDB import.

## Next Planned Pass

Add deterministic equip execution from own Hand to equipped state, including provider-option validation, `RLUsed` mutation, refreshed provider data, and overflow policy still deferred unless explicitly selected.
