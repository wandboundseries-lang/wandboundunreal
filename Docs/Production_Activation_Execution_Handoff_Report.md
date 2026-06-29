# Production Activation Execution Handoff Report

Date: 2026-06-28

## Purpose

This pass adds a narrow production C++ execution handoff adapter for activation decisions.

`FWBProductionActivationExecutionHandoff` connects provider-owned activation entries, target-selection bridge output, existing internal command expansion, existing runtime activation execution, and post-action production provider refresh.

## Handoff Input

`FWBProductionActivationExecutionHandoffInput` accepts:

- mutable `FWBGameStateData*`
- const `FWBCardDefinitionRepository*`
- viewer player id
- const configured `FWBProductionActivationDataProvider*`
- `FWBProductionActivationTargetSelectionRequest`

The adapter does not own runtime/provider/session state.

## Handoff Output

`FWBProductionActivationExecutionHandoffResult` returns:

- `bOk`
- stable result code and reason
- safe source card id
- safe effect id
- safe selected target unit id
- `bExecutionApplied`
- refreshed provider activation entries
- sanitized diagnostic code strings

The result intentionally does not expose internal command payloads, debug activation ids, usage keys, raw fixture data, or hidden zone identities.

## Internal Metadata Rehydration

The target-selection bridge output is public/safe and scrubbed. The handoff adapter therefore resolves internal execution metadata by matching the selection request back to the current provider action entry, then rebuilding the executable command through `WBCardActivationExpansion::BuildActivationCommand` and the const repository definition.

If the provider entry or repository effect cannot be resolved, the adapter fails closed with `activation_entry_missing`, `internal_execution_metadata_missing`, or `command_rebuild_failed`.

## Unit-Target Handoff

Board-source and own-hand unit-target activations are supported when the selected target matches a provider-supplied unit target option.

The adapter hands the rebuilt command into `WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff`. It does not call `WBEffectRunner` directly and does not add effect semantics.

## No-Target Behavior

No-target selection can reach the existing execution bridge without target binding.

Current generic effect validation still requires a target unit for damage/heal/status/armor payloads, so no-target effects with those payloads fail through the existing core command path as `missing_effect_target_unit`. No new no-target payload behavior was added.

## Tile And Wall Behavior

Tile and wall-edge target requirements remain unsupported. The target-selection bridge fails them closed with `unsupported_target_requirement`, and the handoff adapter returns that without mutation.

## Post-Action Provider Refresh

After successful execution, the adapter configures a fresh `FWBProductionActivationDataProvider` with the updated game state, const repository, and viewer player id.

It returns refreshed activation entries from that provider result. A once-per-turn test proves refresh observes updated usage state.

Successful Hand-source activation now moves the used hand source instance from Hand to Discard through `WBCardLifecycle::MoveHandCardToDiscard` before this provider refresh. The refreshed provider snapshot therefore no longer exposes the spent hand card as an own-hand activation source.

Failed selection, failed execution, Board-source activations, and unknown or already-moved hand source instances do not discard a hand card.

## Hidden-Info Policy

Tests cover opponent hand identity, deck identity, hidden marker identity, debug activation ids, and usage keys staying out of handoff result strings, diagnostics, and refreshed provider entries.

Safe result context is limited to visible board/source ids and provider/public source card and effect ids.

## Runtime Harness

A C++ harness test proves the production path:

provider entry -> provider target option -> selection request -> execution handoff -> existing execution bridge -> provider refresh

No UI, Blueprint, widget, or response-window path was added.

## Boundaries

This pass did not add:

- new `WBEffectRunner` behavior
- direct `WBEffectRunner` calls from the production handoff adapter
- activation `FWBAction` creation
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- Godot CardDB import
- UI target picking
- response windows
- shuffle, summon, equip, marker reveal, NPC phase, passives, wands, or RL overflow
- `.uasset` or `.umap` edits

## Next Planned Pass

Add the next production vertical-slice lifecycle/setup step, likely summon/equip/RL foundation or a safe turn-start draw orchestration hook, without adding response windows or UI.
