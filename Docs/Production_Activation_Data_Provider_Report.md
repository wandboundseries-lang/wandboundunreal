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
- `FWBCardActivationLegalActionSet` containing target-deferred activation source/effect choices
- `FWBPublicBoardSummary`

Normal `FWBAction` generation remains external and unchanged.

## Board-Source Behavior

Board-source activations are derived from visible board unit summaries.

For each viewer-owned visible board unit:

- the unit `CardId` is looked up in `FWBCardDefinitionRepository`
- activated effects are inspected
- only explicit board source gates are considered
- `WBCardActivationSourceGate::Evaluate` is reused as a read-only source gate check
- a target-deferred activation action is emitted when the source gate passes

Opponent board units are not emitted as viewer actions.

## Hand-Source Behavior

Hand-source activations use `WBCardZoneObservation::BuildObservationForPlayer`.

Only `Observation.OwnHand.Cards` is inspected. Opponent hand, deck identity, and marker internal identity are never inspected or emitted.

For each own-hand card:

- the observed own-hand `CardId` is looked up in the repository
- activated effects are inspected
- only explicit hand source gates are considered
- source gate evaluation remains read-only
- a target-deferred activation action is emitted when the source gate passes

## Hidden-Info Policy

Provider diagnostics are test-facing and sanitized.

Safe context may include visible board card ids, own-hand card ids, own-hand instance ids, and visible board unit ids.

Diagnostics and provider output must not include opponent hand identities, deck identities, or marker internal card ids.

## Target Options Deferred

This pass does not enumerate target options.

Effects with a non-`none` target requirement still emit source/effect decision data, but their `FWBEffectTargetRef` remains unset and the provider records `target_options_deferred`.

Future provider work should add read-only target enumeration and presentation once target legality policy is explicit.

## Diagnostics

Stable diagnostic codes include:

- `provider_not_configured`
- `game_state_missing`
- `repository_missing`
- `invalid_viewer_player`
- `card_definition_not_found`
- `unsupported_source_zone`
- `target_options_deferred`
- `public_label_rejected`

Invalid required inputs return `bOk=false` and empty decision data.

Missing card definitions, unsupported source zones, public-label rejections, and deferred target options are non-fatal diagnostics on otherwise successful provider output.

## Runtime Session Integration

The provider implements the existing runtime provider interface.

`WBRuntimeActivationDataProviderAdapter::RefreshSessionFromProvider` can pass provider output into `UWBRuntimeActivationDecisionSessionFacadeComponent`, which refreshes activation presentation from externally supplied data.

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

Add read-only target option enumeration for provider-emitted activation source/effect choices, still without executing effects or integrating activation into `FWBAction`.
