# Card Activation Target Selection Presentation Report

## Purpose

This pass adds fixture-only activation target-selection presentation scaffolding for externally supplied activation legal actions.

It gives future UI a safe, read-only presentation model for activation target choices without adding real target picking, activation to `FWBAction`, activation ids to `WBActionCodec`, or activation generation to `WBRules::GenerateLegalActions`.

## Core Types

Added:

- `EWBCardActivationTargetPresentationKind`
- `FWBCardActivationTargetPresentationEntry`
- `FWBCardActivationTargetPresentationSnapshot`
- `WBCardActivationTargetPresentation`

Target kinds are:

- `Unknown`
- `None`
- `Unit`
- `Tile`
- `WallEdge`

`WBCardActivationTargetPresentation::BuildSnapshot` consumes only `FWBCardActivationLegalActionSet` and `FWBPublicBoardSummary`.

## Target Classification

Target kind derives from the already-generated activation legal action target ref:

- valid target unit id -> `Unit`
- valid tile -> `Tile`
- valid wall edge -> `WallEdge`
- no target ref -> `None`
- malformed or partial target ref -> `Unknown`

The builder preserves activation legal action order and does not generate, filter, or validate target legality.

## Public Labels

Public activation labels use the activation legal action public label, falling back to `Activate`.

Public target labels are:

- `Activate` for no target
- `Choose Unit` for unit targets
- `Choose Tile` for tile targets
- `Choose Wall` for wall-edge targets
- `Choose Target` for unknown targets

## Public Data Policy

Source and target public CardIds come only from `FWBPublicBoardSummary::Units`.

Target public CardId lookup happens only for `Unit` targets. Missing public source or target units do not fail snapshot construction; the corresponding public CardId remains empty.

The snapshot does not inspect `FWBGameStateData`, hidden zones, fixture zone context, usage keys, debug activation ids, source effect ids, cost metadata, or CardDB data.

## Fixture Operation

Added the test-only fixture operation:

```json
"operation": "build_card_activation_target_presentation_snapshot"
```

The operation parses fixture activation sources, generates fixture candidates, converts them to activation legal actions, builds a public board summary, builds the target presentation snapshot, and validates expected target presentation fields.

It does not apply activation commands or mutate state.

## Guardrails

Focused tests confirm:

- no `WBRules` or `WBEffectRunner` calls from target presentation
- no `FWBGameStateData` dependency in target presentation
- existing `WBRules::GenerateLegalActions` output remains unchanged
- existing `WBActionCodec` output remains unchanged
- activation target presentation remains separate from `FWBAction`
- duplicate activation action ids make lookup fail
- hidden fixture metadata is excluded from public target presentation fields

## Runtime Handoff

`UWBRuntimeActivationPresentationModelComponent` now stores activation legal-action presentation snapshots and activation target presentation snapshots for future UI/runtime code.

The runtime handoff still consumes externally supplied activation legal actions and `FWBPublicBoardSummary` only. It does not execute activations, generate targets, add activation to `FWBAction`, or add activation ids to `WBActionCodec`.

## Out Of Scope

- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- real target picking
- UI widgets
- response windows
- effect negation
- passives
- wands
- card-specific behavior
- Blueprints, assets, `.uasset`, or `.umap` work
