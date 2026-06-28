# Production Activation Target Selection Bridge Report

Date: 2026-06-28

## Purpose

This pass adds a deterministic C++ target-selection bridge for production activation data.

`FWBProductionActivationTargetSelectionBridge` consumes provider-supplied activation entries and one provider-supplied unit target option, then returns a target-bound `FWBCardActivationCommand` selection result for later handoff/execution wiring.

## Bridge Input

The bridge is configured from externally supplied `FWBCardActivationLegalAction` entries.

`FWBProductionActivationTargetSelectionRequest` identifies the activation entry by action id and safe source/effect context, then optionally supplies one selected `FWBCardActivationTargetOption`.

The bridge does not inspect `FWBGameStateData`, repositories, hidden zones, or Godot CardDB data.

The bridge output now feeds `FWBProductionActivationExecutionHandoff`, which resolves internal execution metadata separately from the safe bridge result.

## Bridge Output

`FWBProductionActivationTargetSelectionResult` returns:

- stable `bOk`, result code, and reason string
- a copied `FWBCardActivationCommand`
- safe source card id, effect id, and selected unit id for tests

Returned commands are copied from provider entries, target-bound, and scrubbed of debug activation ids and usage keys at this selection-output boundary.

## Unit-Target Selection

For `Unit` target requirements:

- a selected target is required
- selected type must be `Unit`
- selected unit id must be valid
- selected unit id, owner, tile, and visible card id must match a provider-supplied target option
- success binds `EffectRequest.Target.TargetUnitId` and target tile on the copied command

Board-source and own-hand activation entries both support unit target selection.

## No-Target Behavior

For `None` target requirements:

- no selected target is required
- success returns a copied command with an empty target binding
- providing a target fails with `target_provided_for_no_target_effect`

## Deferred Targets

Tile and wall-edge requirements fail closed with `unsupported_target_requirement`.

No tile picker, wall picker, target geometry, or UI selection path was added.

## Rejection Behavior

Stable result strings:

- `success`
- `bridge_not_configured`
- `activation_entry_missing`
- `target_required_but_missing`
- `target_not_allowed`
- `target_type_mismatch`
- `target_stale_or_missing`
- `target_provided_for_no_target_effect`
- `unsupported_target_requirement`
- `hidden_target_rejected`
- `command_build_failed`

Failure results do not echo hidden or raw request values.

## Hidden-Info Policy

The bridge consumes only provider action entries and selected provider options.

Tests cover exclusion of opponent hand identity, deck identity, hidden marker identity, hidden target metadata, debug activation ids, and usage keys from bridge output/reasons.

Visible board unit id and visible board card id remain allowed when already present in provider target options.

## Runtime Harness Behavior

A C++ runtime harness test refreshes the existing activation decision-session facade from `FWBProductionActivationDataProvider`, configures the bridge from the same activation action set received by the presentation model, and binds a selected unit target.

The runtime session still does not compute target options internally.

The next production handoff adapter now consumes the same bridge result in C++ to rebuild the internal command, execute through the existing runtime execution bridge, and refresh provider data afterward.

## Boundaries

This pass did not add:

- UI or Blueprint target picking
- response windows
- effect execution changes
- `WBEffectRunner` calls from the bridge
- `FWBGameStateData` mutation
- `FWBCardDefinitionRepository` mutation
- activation `FWBAction` creation
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- Godot CardDB import
- draw, discard movement, summon, equip, marker reveal, NPC phase, passives, wands, or RL overflow
- `.uasset` or `.umap` changes

## Next Planned Pass

Add deterministic draw/hand/discard movement needed for played-card lifecycle now that the C++ provider -> selection -> handoff -> refresh path exists.
