# Runtime Activation Data Provider Interface Audit

## Scope

This pass adds a production-safe external activation data provider interface shell and adapter. It does not add a real provider implementation, CardDB import, production zones, UI, target picking, response windows, passives, wands, or activation integration into `FWBAction`.

## Current Activation Decision-Session Facade

`UWBRuntimeActivationDecisionSessionFacadeComponent` already consumes externally supplied decision data:

- normal `FWBAction` legal actions
- `FWBCardActivationLegalActionSet`
- `FWBPublicBoardSummary`

The facade refreshes normal decision-point presentation and activation presentation through `UWBRuntimeDecisionPointOwnerComponent`. It resolves selected activation ids, creates handoffs, and executes selected activation ids through the existing owner/sequencer path.

The facade does not generate legal actions, activation candidates, activation legal actions, or public summaries. It does not own or cache `FWBGameStateData`.

## Current Post-Activation Refresh Sequencer

`WBRuntimePostActivationRefreshSequencer` executes a selected activation id through the owner and then refreshes normal and activation presentation from externally supplied post-action data.

It supports success refresh, disabled success refresh, default no-refresh on failure, and optional failure refresh. It does not compute replacement legal actions or summaries.

## Current Test-Only Activation Session Harness

`FWBActivationSessionTestHarness` lives under `Source/WandboundTests/Private` only. It simulates an external rules owner for automation by calling:

- `WBRules::GenerateLegalActionsForPlayer`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBPublicBoardSummary::Build`

That harness proves the runtime facade can consume external data, but it is not production runtime code.

## Required External Data

The session facade requires the same public/action surface for current and post-activation decision points:

- precomputed normal legal actions
- precomputed activation legal actions
- public board summary

This is exactly the data that the provider interface shell will return.

## Why Add A Provider Interface Shell

The runtime needs a stable seam for future systems to supply externally generated activation session data without letting runtime generate or own rules truth. The provider interface names that seam while keeping the implementation deferred.

The provider result mirrors `FWBRuntimeActivationDecisionSessionRefreshInput`, so the adapter can feed provider output directly into the existing facade.

## No Rules-State Ownership

The provider interface does not accept `FWBGameStateData` and does not return hidden state. Any future production provider is responsible for obtaining legal actions, activation action sets, and public summaries outside this runtime shell.

The adapter may pass an externally supplied mutable state reference only to the existing facade execution method, preserving the current execution boundary.

## Provider Implementations Deferred

Real providers need production CardDB, card-zone state, target selection policy, and hidden-information rules. Those systems do not exist yet, so this pass adds only the interface, adapter, and fixed test provider.

## Production Runtime No-Generation Policy

Production runtime must still not call:

- `WBRules::GenerateLegalActions`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBPublicBoardSummary::Build` from the activation session provider path

Runtime remains a consumer of external public/action data.

## Hidden-Information Boundary

Provider output must be runtime-safe:

- public presentation data only
- no hidden zones
- no private card identities beyond visible public summary data
- no internal activation metadata in public snapshots, traces, logs, or docs

Internal command payloads can carry execution metadata, but presentation surfaces must stay sanitized.

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
