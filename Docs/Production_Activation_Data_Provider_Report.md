# Production Activation Data Provider Report

Date: 2026-06-28

## Purpose

This pass adds a production-safe runtime activation data provider skeleton.

`FWBProductionActivationDataProvider` implements `IWBRuntimeActivationDataProvider` and bridges existing Core state surfaces into the existing runtime activation decision-session facade.

The provider is read-only. It emits externally owned activation source/effect decision data and does not execute effects.

## Provider Input

`FWBProductionActivationDataProviderInput` contains:

- const `FWBGameStateData*`
- const `FWBCardDefinitionRepository*`
- `ViewerPlayerId`

The provider does not own or mutate those inputs.

## Provider Config

`FWBProductionActivationDataProviderConfig` controls enabled source families:

- board sources enabled by default
- own-hand sources enabled by default
- discard sources disabled
- equipped sources disabled

Discard and equipped are kept disabled until their visibility and gameplay policies are implemented.

## Provider Output

The provider returns the existing `FWBRuntimeActivationDataProviderResult`.

Successful output fills `FWBRuntimeActivationDecisionSessionRefreshInput` with:

- empty normal legal actions
- `FWBCardActivationLegalActionSet` containing activation source/effect choices and supported unit target options
- `FWBPublicBoardSummary`

Normal `FWBAction` generation remains external and unchanged.

## Board-Source Behavior

Board-source activations are derived from visible board unit summaries.

For each viewer-owned visible board unit:

- the unit `CardId` is looked up in `FWBCardDefinitionRepository`
- activated effects are inspected
- only explicit board source gates are considered
- `WBCardActivationSourceGate::Evaluate` is reused as a read-only source gate check
- an activation action is emitted when the source gate passes
- unit-target effects receive visible board unit target options

Opponent board units are not emitted as viewer actions.

## Hand-Source Behavior

Hand-source activations use `WBCardZoneObservation::BuildObservationForPlayer`.

Only `Observation.OwnHand.Cards` is inspected. Opponent hand, deck identity, and marker internal identity are never inspected or emitted.

For each own-hand card:

- the observed own-hand `CardId` is looked up in the repository
- activated effects are inspected
- only explicit hand source gates are considered
- source gate evaluation remains read-only
- an activation action is emitted when the source gate passes
- unit-target effects receive visible board unit target options

## Hidden-Info Policy

Provider diagnostics are test-facing and sanitized.

Safe context may include visible board card ids, own-hand card ids, own-hand instance ids, and visible board unit ids.

Diagnostics and provider output must not include opponent hand identities, deck identities, or marker internal card ids.

## Target Options

The provider now enumerates unit target options for supported `Unit` target requirements.

Unit target options are visible board units from `FWBPublicBoardSummary`, sorted deterministically by owner, tile, unit id, and card id. Generic unit targeting includes friendly units, enemy units, and self because no stricter target requirement enum exists yet.

`None` target requirements have no target options and no deferred diagnostic.

Tile and wall-edge target requirements remain deferred and record `target_options_deferred`.

## Diagnostics

Stable diagnostic codes include:

- `provider_not_configured`
- `game_state_missing`
- `repository_missing`
- `invalid_viewer_player`
- `card_definition_not_found`
- `unsupported_source_zone`
- `target_options_deferred`
- `unsupported_target_requirement`
- `no_unit_targets_available`
- `public_label_rejected`

Invalid required inputs return `bOk=false` and empty decision data.

Missing card definitions, unsupported source zones, public-label rejections, deferred target options, unsupported target requirements, and no available unit targets are non-fatal diagnostics on otherwise successful provider output.

## Runtime Session Integration

The provider implements the existing runtime provider interface.

`WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider` can pass provider output into `UWBRuntimeActivationDecisionSessionFacadeComponent`, which refreshes activation presentation from externally supplied data, including provider-supplied target options.

The session still does not generate activation data internally.

## Boundaries

This pass did not add:

- Godot CardDB import
- full production CardDB loader
- draw, shuffle, or discard movement
- summon or equip gameplay
- marker reveal
- NPC phase
- response windows
- passives or wands
- RL overflow
- effect execution
- activation resolution changes
- activation `FWBAction` integration
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- UI, Blueprint, `.uasset`, or `.umap` changes

## Next Planned Pass

Add a C++ activation selection bridge that binds a selected unit target option to an activation command for execution, still without UI or response windows.
