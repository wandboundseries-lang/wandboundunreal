# Runtime Activation Data Provider Contract Audit

## Scope

This pass adds test-only contract verification for future activation data providers. It does not add a production provider, CardDB import, production zones, runtime rules generation, UI, target picking, response windows, passives, wands, or activation integration into `FWBAction`.

## Current Provider Request And Result Shape

`FWBRuntimeActivationDataProviderRequest` contains:

- request kind: `Unknown`, `CurrentDecisionPoint`, or `PostActivation`
- player id
- decision point id
- selected activation action id
- optional request reason

`FWBRuntimeActivationDataProviderResult` contains:

- success flag and reason
- copied request
- `FWBRuntimeActivationDecisionSessionRefreshInput`

The refresh input contains the runtime facade's existing external-data surface:

- normal legal `FWBAction` values
- `FWBCardActivationLegalActionSet`
- `FWBPublicBoardSummary`

The provider interface does not accept `FWBGameStateData`.

## Current Adapter Behavior

`WBRuntimeActivationDataProviderAdapter` asks a provider for externally prepared data and forwards that data into `UWBRuntimeActivationDecisionSessionFacadeComponent`.

Refresh behavior:

- fail with `session_facade_missing` when the facade is null
- fail with the provider reason when provider output is not ok
- otherwise call `RefreshSessionFromExternalData`

Execution behavior:

- fail with `session_facade_missing` when the facade is null
- ask the provider for post-activation data before execution
- fail with the provider reason before execution when provider output is not ok
- otherwise call `ExecuteActivationActionIdAndRefreshFromExternalData`

The adapter does not generate rules data.

## Current Fixed Test Provider

`FWBRuntimeActivationFixedDataProviderForTests` lives under `Source/WandboundTests/Private`. It returns a fixed provider result and copies the incoming request into that result. It is test-only and must not be included from `Source/WandboundRuntime`.

## Current Session Facade Input Requirements

The activation decision-session facade requires:

- externally supplied normal legal actions
- externally supplied activation legal actions
- externally supplied public board summary

It consumes that data for both current decision-point refresh and post-activation refresh.

## Future Provider Responsibilities

Future CardDB/zone-backed providers must produce runtime-safe provider results without requiring runtime code to inspect private state. A valid provider result must supply the normal legal action list, activation action set, and public summary expected for the requested decision point.

Future providers are also responsible for deterministic ordering, public labels, and hidden-information exclusion.

## Required Provider Output Properties

Provider output must:

- use `CurrentDecisionPoint` or `PostActivation` for successful results
- include a valid player id for successful results
- include a non-empty decision point id for successful results
- include the selected activation action id for successful post-activation results
- include required normal legal actions when the contract expectation requires them
- include required activation legal actions when the contract expectation requires them
- include required public board summary units when the contract expectation requires them
- give every activation action a non-empty activation action id
- keep public labels and debug/test output free of hidden/internal tokens
- be deterministic for the same provider and request

## Why Contract Tests Now

Production CardDB and production zones are not implemented yet. Contract tests let future provider work target a stable public/action surface before real provider internals exist. They also protect the existing runtime boundary: runtime remains a consumer of provider output, not the generator of that output.

## Hidden-Information Boundary

Provider results must not leak hidden zones, private card identities, internal source effect ids, usage keys, debug activation ids, cost metadata, schema details, or raw effect operation names through public snapshots, labels, serialized/debug strings, tests, or docs.

Internal activation commands may carry execution metadata, but public provider output and presentation text must remain safe.

## Runtime No-Generation Policy

Runtime must still not generate normal legal actions, activation candidates, activation legal actions, or public summaries from `FWBGameStateData` through the provider path. It must not own or cache rules state and must not inspect hidden zones.

## Deferred

- production provider implementation
- production CardDB
- production zones
- UI widgets
- target picking
- response windows
- negation
- passives and wands
- visual animation and VFX
