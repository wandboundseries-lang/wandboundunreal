# Runtime Activation CardDB/Zone Provider Plan

## Purpose

This document defines the future production CardDB/zone-backed activation data provider plan.

The runtime facade remains a consumer. The provider implementation is future work. Runtime must not become the rules owner. Provider output must satisfy the existing activation data provider contract.

This plan does not implement a provider, import CardDB, create production zones, add UI, add response windows, or change rules behavior.

## Current Architecture

`IWBRuntimeActivationDataProvider` exposes `GetActivationDecisionData`.

`FWBRuntimeActivationDataProviderRequest` carries request kind, player id, decision point id, selected activation action id, and optional request reason.

`FWBRuntimeActivationDataProviderResult` carries success state, reason, copied request, and `FWBRuntimeActivationDecisionSessionRefreshInput`.

`WBRuntimeActivationDataProviderAdapter` refreshes the activation decision-session facade from provider output and can execute selected activations with provider-supplied post-activation data.

`UWBRuntimeActivationDecisionSessionFacadeComponent` consumes externally supplied normal legal actions, activation legal actions, and public board summaries. It does not generate those values.

`FWBRuntimeActivationDataProviderContractVerifier` is test-only and defines expectations for future provider implementations.

`FWBActivationSessionTestHarness` is test-only and simulates an external rules owner for runtime session coverage.

## Future Provider Responsibility

A future production provider is responsible for:

- receiving a `CurrentDecisionPoint` or `PostActivation` request
- generating or obtaining normal `FWBAction` legal actions from the authoritative rules owner
- generating or obtaining activation legal actions from CardDB/zone-backed activation systems
- obtaining a public board summary from an authorized public-summary builder
- returning `FWBRuntimeActivationDecisionSessionRefreshInput`
- preserving deterministic ordering
- excluding hidden information unavailable to the requesting viewer/player

The provider may coordinate with rules/core systems when it is the authorized production rules/activation owner. `Source/WandboundRuntime` must not generate the data itself.

## Provider Inputs

Planned request fields and sources:

- request kind
- player id
- viewer id
- decision point id
- selected activation action id for post-activation requests
- authorized observation scope
- optional request reason/debug context
- future match id
- future turn id
- future state version id
- future RNG/trace id if needed

The provider interface should not expose raw mutable game state to UI.

A future production implementation may be constructed with read-only access to an authoritative rules owner or snapshot service. The runtime facade should not own that state.

## Provider Outputs

Required output:

- normal legal action list
- activation legal action set
- public board summary

Optional future output:

- current player id
- priority player id
- phase
- presentation statuses
- diagnostics/reason codes
- provider version
- data version

Hidden zones must not be included in provider output unless explicitly scoped to the owning player and not exposed globally.

## Zone Ownership Boundary

### Board

Board units are public.

Visible unit `CardId` is public under current policy.

Board-source activation may use visible board unit `CardId` parity.

### Hand

Own hand may be visible to the owning player's provider response.

Opponent hand must not be exposed.

Hand card ids must not appear in public board summaries.

Future activation actions from hand must be scoped to the owning player.

### Deck

Deck is hidden by default.

Deck activation is unsupported unless a future card-specific rule explicitly permits it.

The provider may expose deck size if a future observation contract allows it.

No deck card ids may appear in public output.

### Discard

Preliminary policy: discard should be treated as public if Wandbound rules intend the Godot asset manifest policy to carry forward. Godot asset manifest documentation says discard is public and top card face is allowed.

Open question: confirm final discard visibility before production provider implementation. If discard is not finalized as public, use owner-scoped discard in provider responses until canon settles it.

No production implementation is added in this pass.

### Equipped

Equipped and wand sources should be tied to board units.

Visible equipment policy must be explicitly decided before production.

Current fixture scaffold supports equipped ownership metadata only. It is not production equipped state.

### Markers

Hidden marker identity remains excluded.

Marker state is not modeled in Unreal yet.

The provider must not expose marker identity until an explicit player-perspective marker model exists.

## Hidden-Information Policy

Public summaries are safe for both players.

Player-scoped provider responses may include own hidden information only if the future UI endpoint is authenticated/scoped.

Provider output must not expose:

- opponent hidden hand
- deck identity
- hidden marker identity
- source effect/debug ids in public presentation
- usage keys in presentation
- fixture zone context in presentation
- raw schema field names
- raw effect type names
- hook names
- cost internals beyond player-facing RL/RR terms where needed

Player-facing wording should use terms such as `Activate`, `Choose Unit`, `Choose Tile`, `Choose Wall`, `Available RL`, and `RL Used`.

## CardDB Boundary

Godot CardDB is reference-only until an explicit import milestone.

A future Unreal-owned CardDB should map into `FWBCardDefinition`, `FWBCardEffectDefinition`, or successor data.

Imported effects should first target existing generic payload families:

- `armor_effect`
- `status_effect`
- `damage_effect`
- `heal_effect`

Unsupported effect types should fail closed with clear diagnostics.

No card-specific behavior should silently bypass `WBEffectRunner`.

Godot files must not be runtime-loaded by Unreal.

## Activation Candidate Generation Ownership

Activation candidates are generated outside `Source/WandboundRuntime`.

The provider implementation may call Core candidate/legal action systems when it is the authorized production rules/activation owner.

`UWBRuntimeActivationDecisionSessionFacadeComponent` must only consume provider output.

Activation remains separate from `FWBAction` until an explicit future milestone changes that.

## Affordability / Cost / Usage Policy

The provider may use read-only affordability queries.

Cost gates may filter unaffordable activations.

Payment remains in the `ApplyCardActivationCommand` execution path.

The provider must not pay costs while building provider output.

Usage keys may filter once-per-turn activations.

Usage marking happens only after successful activation execution.

## Response Window / Negation Policy

Response windows are not implemented yet.

The provider should not pretend response windows exist.

Activation execution currently resolves directly.

A future response-window milestone must adjust provider output and decision-point types before response activations are exposed.

## Fixture Strategy

Near-term fixture strategy:

- keep using Unreal-owned fixture card definitions
- add small sample sets before CardDB import
- cover source zone
- cover target requirement
- cover cost gate
- cover usage gate
- cover effect payload
- cover hidden-info expectation
- prefer C++ contract tests for runtime provider behavior
- prefer GoldenScenarios for deterministic core activation/cost/trace scenarios

No fixture files are added in this planning pass.

## Contract Test Extension Plan

Future provider contract categories:

- valid provider result from board-source card
- valid provider result from hand-source card
- hidden opponent hand exclusion
- deck identity exclusion
- discard visibility policy tests
- equipped/wand source tests
- unsupported CardDB effect fails closed
- deterministic ordering
- post-activation provider refresh after cost/usage mutation
- response-window pending activation once implemented

## Unreal CardDB Schema Planning

Unreal-owned CardDB schema planning docs now exist:

- `Docs/CardDB_Unreal_Schema_Audit.md`
- `Docs/CardDB_Unreal_Schema_Plan.md`
- `Docs/CardDB_Unreal_Schema_Examples.md`

Future production providers should consume card definitions only after importer work maps validated schema data into Unreal-owned `FWBCardDefinition` and `FWBCardEffectDefinition` values.

The test-only schema fixture validator proves the current fail-closed validation contract before production provider work. Future providers should consume only schema-validated card definitions and should not perform schema validation, CardDB import, effect execution, or hidden-zone inspection themselves.

The provider remains an external-data boundary. It should consume already-loaded definitions, zone observations, normal legal actions, activation legal actions, and public summaries; it should not import CardDB data, mutate rules state, generate legal action families internally, or expose hidden card identity.

Future providers should consume only bundles that satisfy importer-readiness criteria:

- bundle validation passes
- source-version compatibility passes
- dependency order is available
- ordered validated `FWBCardDefinition` values are available
- diagnostics are empty

The current importer-readiness helper is test-only and does not load definitions into runtime. A future production provider should treat equivalent readiness as a prerequisite, not as provider-owned parsing behavior.

For failed bundles, the future provider/import pipeline should surface grouped diagnostics suitable for logs or tooling: readiness reason counts, diagnostic code counts, affected bundle counts, and affected card-context counts. Grouped diagnostics should not expose hidden values or raw source JSON.

Future provider/import pipeline work should accept only bundle sets that pass the future production equivalent of batch readiness. Batch reporting should happen before runtime provider consumption and before any zone-backed activation data is exposed.

Future provider/import pipeline work should accept only manifest-approved batch outputs. Manifest approval should validate duplicate names, safe relative paths, compatibility overrides, metadata, and hidden-information policy before any provider receives loaded CardDB definitions.

Future provider/import pipeline work should accept only suite-approved manifest and bundle outputs. Suite approval should validate manifest aliases and paths before importer/provider code receives any manifest-approved bundle set.

## Implementation Sequence Recommendation

1. Draft Unreal-owned CardDB schema docs only.
2. Add read-only production zone state structs.
3. Add player-perspective observation/scope model.
4. Add CardDB fixture sample importer, not Godot import yet.
5. Add production provider skeleton behind `IWBRuntimeActivationDataProvider`.
6. Extend provider contract tests for zones.
7. Add CardDB import mapper.
8. Add response-window activation declaration model.
9. Decide whether activation joins `FWBAction`/`WBActionCodec` or remains separate.

## Out Of Scope

- no production provider in this pass
- no CardDB import
- no production zones
- no UI widgets
- no target picking
- no response windows
- no effect negation
- no passives/wands
- no activation `FWBAction`
- no `WBActionCodec` changes
- no source code changes
- no tests
- no fixtures
