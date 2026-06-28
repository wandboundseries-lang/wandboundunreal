# Production Activation Target Options Report

Date: 2026-06-28

## Purpose

This pass adds deterministic, read-only unit target-option enumeration to `FWBProductionActivationDataProvider`.

Target options are provider-owned decision data carried through the existing `FWBCardActivationLegalActionSet` refresh path. They are not effect execution, target picking UI, response-window legality, or normal `FWBAction` legal actions.

## Data Shape

Added Core activation decision data:

- `EWBCardActivationTargetOptionType`
- `FWBCardActivationTargetOption`
- `FWBCardActivationLegalAction::TargetRequirement`
- `FWBCardActivationLegalAction::TargetOptions`

The only supported option type is `Unit`.

Each unit option contains:

- target unit id
- target owner player id
- target tile
- visible board card id
- safe public label

## Supported Behavior

For `Unit` target requirements, the provider enumerates all visible valid board units from `FWBPublicBoardSummary`.

Generic unit targeting includes:

- the source unit itself
- friendly units
- enemy units

Removed and defeated units are excluded through the public board summary policy and a defensive target-option validity check.

Board-source and own-hand effects both receive the same visible board unit target options.

## Deferred Behavior

`None` target requirements produce empty target options and no deferred diagnostic.

`Tile` and `WallEdge` requirements still produce empty target options and `target_options_deferred`.

Unknown target requirement values produce empty target options and `unsupported_target_requirement`.

Unit-target effects with no visible unit targets still emit the activation decision entry with empty target options and `no_unit_targets_available`.

## Deterministic Ordering

Target options sort by:

1. target owner player id
2. tile Y
3. tile X
4. target unit id
5. target card id

Activation decision entry ordering is unchanged.

## Hidden-Info Policy

Provider output and diagnostics still exclude opponent hand identity, deck identity, and hidden marker identity.

Visible board `CardId` may appear in target options because visible board unit card identity is already public.

## Runtime Session Behavior

Runtime sessions receive target options through externally supplied `FWBCardActivationLegalActionSet` data.

The session facade, owner, coordinator, and activation presentation model do not enumerate targets. They only store and present provider-supplied data. Target presentation entries now recognize actions with unit target options as unit-target choices and expose the option count.

Provider target options can now be selected through the C++ `FWBProductionActivationTargetSelectionBridge`, which binds one provider-supplied unit option into a copied activation command without adding UI or effect execution.

That selected target path now participates in the provider -> selection -> production execution handoff -> provider refresh C++ test path. Target options remain externally supplied provider data; runtime still does not compute them internally.

## Boundaries

This pass did not add:

- effect execution
- activation resolution
- target picking UI
- response windows
- draw, discard movement, summon, or equip gameplay
- Godot CardDB import
- activation `FWBAction` integration
- `WBRules::GenerateLegalActions` changes
- `WBActionCodec` changes
- Blueprint, `.uasset`, or `.umap` changes

## Next Planned Pass

Add deterministic draw/hand/discard movement needed for played-card lifecycle, still without UI or response windows.
