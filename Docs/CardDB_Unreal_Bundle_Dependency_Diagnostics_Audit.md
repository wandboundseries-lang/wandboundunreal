# CardDB Unreal Bundle Dependency Diagnostics Audit

## Scope

This audit covers a test-only extension to CardDB bundle fixture validation under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Bundle Validation Behavior

`FWBCardDBSchemaFixtureValidator::ValidateBundleJsonString` validates the bundle wrapper, then validates each `cards[]` entry through the root-card fixture validator.

Current bundle validation covers:

- malformed bundle JSON
- missing or unsupported `bundle_schema_version`
- missing `carddb_version`
- missing, malformed, or empty `cards`
- duplicate card ids
- strict unknown bundle top-level and metadata fields
- root-card validation for every card object
- cross-card reference shape and missing-reference diagnostics

Valid bundle cards may map into `FWBCardDefinition` values for validation assertions only. That mapping is not a production loader.

## Current Cross-Reference Schema

Bundle fixtures may declare authoring/test references at card, effect, or payload level:

- `references.card_ids[]`
- `references.effect_refs[].card_id`
- `references.effect_refs[].effect_id`
- `references.metadata`

Root-card validation checks reference shape. Bundle validation resolves references against a bundle-local card id index and effect id index.

The `references` object does not map into `FWBCardDefinition` and has no gameplay semantics.

## Current Diagnostic Context Fields

`FWBCardDBSchemaValidationDiagnostic` carries:

- `Code`
- `Message`
- `CardId`
- `EffectId`
- `CardIndex`
- `BundleCardId`
- `JsonPath`

Bundle card diagnostics use zero-based `cards[]` indices, safe public card ids when available, existing effect id context when safe, and stable paths such as `$.cards[0].activated_effects[0].references.card_ids[0]`.

Root-card validation outside bundle mode keeps bundle context absent.

## Current Duplicate Card Handling

Duplicate non-empty card ids fail with `card_id_duplicate`.

The duplicate diagnostic reports the second duplicate index and `$.cards` path. Current cross-reference resolution is skipped when duplicate ids exist because the card id index would be ambiguous.

Dependency ordering should follow the same policy and leave dependency order empty when duplicate ids exist.

## Current Missing-Reference Handling

Missing card references fail with `missing_card_reference`.

Missing effect references fail with `missing_effect_reference`.

Missing-reference diagnostics use generic messages and owner context only. They do not echo referenced ids or full JSON snippets.

Dependency ordering should run only after cross-reference validation succeeds without missing references. Missing-reference diagnostics already explain the authoring problem.

## Why Dependency Ordering Diagnostics Are Useful

Future production CardDB import will need deterministic load/validation ordering before definitions can reference one another safely.

Adding test-only dependency ordering now gives authoring fixtures stable feedback for:

- acyclic load order
- self-dependencies
- dependency cycles
- deterministic tie-breaking

This lets the project prove diagnostics and fixture policy before adding production import surfaces.

## Why Cycle Diagnostics Are Authoring Diagnostics Only

Cycles are schema/authoring problems in fixture bundles. They do not imply runtime behavior and should not be repaired by runtime systems.

This pass should fail closed with stable diagnostics and no gameplay side effects.

## Why Dependency Graph Output Remains Test-Only

The graph is derived from fixture-only `references` data. It does not represent production card loading, zone state, activation availability, target legality, or effect execution.

`DependencyOrderCardIds` should exist only on the test-only bundle validation result and should not be consumed by `WandboundCore` or `WandboundRuntime`.

## Hidden-Information Policy

Diagnostics must not include hidden values, raw referenced ids with hidden tokens, or full JSON snippets.

Card/effect context should use safe identifiers only. Missing-reference and cycle diagnostics should use generic messages such as `dependency cycle detected`.

## Why Production Importer/Loader Remains Deferred

Production import still needs separate design for data ownership, packaging, source versioning, hidden-information observation, zone ownership, runtime provider input, and EffectRunner-backed execution.

This pass only derives deterministic authoring diagnostics from Unreal-owned test fixtures.
