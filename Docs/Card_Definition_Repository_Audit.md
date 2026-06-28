# Card Definition Repository Audit

Date: 2026-06-28

## Current Card Definition Shape

`FWBCardDefinition` is a pure WandboundCore data struct with:

- `CardId`
- `PublicName`
- `Kind`
- `ActivatedEffects`

Each `FWBCardEffectDefinition` contains:

- `EffectId`
- `PublicLabel`
- `TargetRequirement`
- `Payloads`
- `SourceGate`

This shape already supports activation expansion and candidate generation without requiring a CardDB importer.

## Current Activation Dependency On Definitions

`WBCardActivationExpansion` consumes one `FWBCardDefinition` and an effect id, then builds an `FWBCardActivationCommand` when the requested effect is present, target requirements match, and payloads are supported.

`WBCardActivationCandidateGenerator` consumes externally supplied candidate sources that already contain `FWBCardDefinition` values. It produces activation candidates from supplied targets and source-gate context. It does not own CardDB loading or zone observation.

`WBCardActivationLegalActionGenerator` converts candidates into a separate activation legal-action family. It remains separate from `FWBAction`, `WBActionCodec`, and `WBRules::GenerateLegalActions`.

## Current Test-Only CardDB Validator Status

The project already has extensive test-only CardDB schema, bundle, dependency, manifest, and suite validation scaffolding under `WandboundTests` and `Reference/GodotCanon`.

That scaffolding can validate fixture JSON and may map valid fixture data into `FWBCardDefinition` for tests only. It is not a production importer, loader, repository, runtime provider, or gameplay path.

## Why A Repository Shell Is Needed Now

Card zones now model card instances, and card zone observation can expose safe player-perspective references. The next production step needs a stable place to hold static Unreal-owned card definitions so future provider work can bridge:

```text
card instance id / card id -> static FWBCardDefinition -> activation candidate data
```

The repository shell gives future loader/provider work a deterministic query boundary without adding file loading or runtime behavior in this pass.

## Chosen Repository Data Shape

The repository is a separate pure C++ value:

- repository id
- source version
- array of `FWBCardDefinition` values

It is not stored on `FWBGameStateData`.

`FWBGameStateData` owns mutable game state and card instances. The repository owns static card definitions. A future provider or loader should bridge those two concepts.

## Validation Policy

Repository validation fails closed on:

- missing repository id
- missing card id
- duplicate card id
- missing public card name
- missing effect id
- duplicate effect id within a card
- player-facing labels containing internal terms
- unsupported target requirement values

Activated effects may be empty for vanilla cards. Payload and source-gate validation remains minimal here because the test-only schema validator and activation expansion path already cover richer payload behavior.

## Deterministic Lookup And Order Policy

Lookup is exact-match by `CardId`.

Public query helpers do not depend on `TMap` iteration order. Card id and definition enumeration sort by `CardId` ascending.

Build helpers copy input definitions into repository storage and then validate the resulting repository.

## Public Label Policy

`PublicName` and `PublicLabel` are player-facing strings.

The repository rejects labels containing internal/dev terms such as:

- `EffectRunner`
- `Rules`
- raw effect type names like `damage_effect`
- schema fields like `from_tile`, `to_tile`, and `chosen_tile`
- player id phrasing
- `schema`
- `hook`

This keeps public presentation text from inheriting authoring or engine implementation vocabulary.

## Why This Pass Does Not Implement File Loading Or Import

Production CardDB import still needs explicit decisions about packaging, source-version compatibility, diagnostics, fixture/runtime separation, dependency ordering, and hidden-information policy.

This pass only creates the in-memory repository boundary that a future loader can populate.

## Why Runtime And Provider Integration Are Deferred

Runtime should continue to consume externally supplied legal actions and public summaries. It should not own definitions, inspect hidden zones, generate legal actions, or load CardDB data.

Provider integration is deferred until a production data provider can consume card zones, card observations, and repository definitions together while preserving hidden-information boundaries.

## Risks And Open Questions

- Future production loader diagnostics must align with repository validation reasons without duplicating every test-only schema diagnostic.
- Public label policy may need expansion once real cards include longer public text.
- Static definition storage may later need dependency/version metadata beyond repository id and source version.
- Future provider work must avoid using repository lookup to bypass player-perspective zone observation.
