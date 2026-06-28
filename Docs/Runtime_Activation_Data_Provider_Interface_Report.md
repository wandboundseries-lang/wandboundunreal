# Runtime Activation Data Provider Interface Report

## Purpose

This pass adds a production-safe external activation data provider interface shell. The shell gives future systems a runtime-safe way to supply precomputed activation decision-session data to `UWBRuntimeActivationDecisionSessionFacadeComponent`.

`FWBProductionActivationDataProvider` now implements this interface as the first production provider skeleton.

## Provider Request

`FWBRuntimeActivationDataProviderRequest` contains:

- request kind: `CurrentDecisionPoint` or `PostActivation`
- player id
- decision point id
- selected activation action id
- optional request reason/context

The provider request does not contain `FWBGameStateData`.

## Provider Result

`FWBRuntimeActivationDataProviderResult` contains:

- success flag and reason
- copied request
- `FWBRuntimeActivationDecisionSessionRefreshInput`

The refresh input carries only the data the runtime facade already accepts:

- normal legal `FWBAction` values
- `FWBCardActivationLegalActionSet`
- `FWBPublicBoardSummary`

## Interface

`IWBRuntimeActivationDataProvider` exposes:

- `GetActivationDecisionData`

The interface is pure C++, runtime-safe, non-UObject, and non-Blueprint. It does not own rules state, accept game state, inspect hidden zones, generate actions, or build summaries.

## Adapter

`WBRuntimeActivationDataProviderAdapter` provides:

- `RefreshSessionFromProvider`
- `ExecuteActivationAndRefreshFromProvider`

Refresh flow:

1. validate the session facade
2. ask the provider for current decision data
3. stop on provider failure
4. pass provider refresh input to the session facade

Execution flow:

1. validate the session facade
2. ask the provider for post-activation decision data
3. stop before execution on provider failure
4. execute selected activation through the session facade with provider-supplied post-action data

The execution method requests post-activation data before execution. This shell models a future external system that has already prepared the post-action decision data. The adapter does not compute it from state.

## Fixed Test Provider

`FWBRuntimeActivationFixedDataProviderForTests` lives under `Source/WandboundTests/Private` only. It returns a fixed result and copies the incoming request into the returned result.

It exists only for automation coverage and is not included by production runtime.

## External-Data Ownership

Provider implementations are responsible for supplying runtime-safe public/action data. Runtime remains a consumer.

`FWBProductionActivationDataProvider` supplies target-deferred activation source/effect decision data from `FWBGameStateData`, `WBCardZoneObservation`, and `FWBCardDefinitionRepository`.

The production provider skeleton does not:

- own or mutate `FWBGameStateData`
- generate normal legal actions
- generate activation candidates
- execute effects
- call `WBEffectRunner` directly
- call `WBActionCodec`

It does emit a target-deferred `FWBCardActivationLegalActionSet` for source/effect presentation.

## Hidden Information

Provider output must remain public/runtime-safe. Tests verify activation presentation refreshed from provider data excludes internal source effect ids, usage keys, debug activation ids, and cost metadata.

## Provider Contract

`FWBRuntimeActivationDataProviderContractVerifier` now defines test-only expectations for future provider implementations. The contract covers accepted request kinds, required normal legal actions, activation legal actions, public board summary units, non-empty activation action ids, deterministic output, hidden-token exclusion, and provider failure policy.

The verifier and contract provider variants live under `WandboundTests` only and are not included by production runtime.

## CardDB/Zone Provider Planning

`Docs/Runtime_Activation_CardDB_Zone_Provider_Plan.md` defines the future CardDB/zone-backed provider plan. The production provider skeleton is now implemented, and runtime remains a consumer of externally supplied normal legal actions, activation legal actions, and public summaries.

## Action Family Boundary

Activation legal actions remain separate from `FWBAction`.

`WBActionCodec` is unchanged. `WBRules::GenerateLegalActions` is unchanged.

## Out Of Scope

- target option enumeration
- production zones
- CardDB import
- UI widgets
- target picking
- response windows
- effect negation
- passives and wands
- visual animation and VFX
- activation `FWBAction` integration
