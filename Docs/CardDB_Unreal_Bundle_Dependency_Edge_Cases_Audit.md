# CardDB Unreal Bundle Dependency Edge Cases Audit

## Scope

This audit covers test-only CardDB bundle dependency ordering edge cases under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Dependency Edge Policy

Bundle dependency edges are derived from fixture-only `references` fields:

- card-level `references.card_ids`
- activated-effect `references.card_ids`
- activated-effect `references.effect_refs`
- payload-level `references.card_ids`

Each edge is stored as owner card/effect context plus the referenced card index. Missing referenced ids are diagnosed earlier by cross-reference validation and do not become dependency edges.

## Current `DependencyOrderCardIds` Behavior

`FWBCardDBBundleSchemaValidationResult::DependencyOrderCardIds` is populated only when dependency ordering can complete for all cards.

Current skip-order behavior already covers:

- duplicate card ids
- missing card references
- missing effect references
- malformed card ids that prevent card id indexing
- dependency self-references
- dependency cycles

Self-references and cycles add dependency diagnostics and leave dependency order empty.

## Current Stable Tie-Break Behavior

Topological ordering scans `cards[]` order for the next zero-dependency card. This keeps dependencies before dependents while preserving original input order among currently available independent cards.

Duplicate valid references currently add duplicate internal edges, but the existing dependency count and decrement pass still emits each card id once and keeps a stable valid order.

## Current Missing-Reference / Duplicate-Id Skip-Order Behavior

Missing references and duplicate card ids already leave `DependencyOrderCardIds` empty.

Duplicate card ids report the second duplicate card index and safe duplicate card id. Missing reference diagnostics use safe owner context and do not echo referenced hidden values.

## Current Expected Export Behavior

Expected export snapshots serialize:

- `ok`
- clean-file-name `source_path`
- `card_count`
- `dependency_order_card_ids`
- sanitized diagnostic context

Export JSON omits diagnostic messages, full source JSON, public text, public labels, payload bodies, and hidden referenced values.

## Missing Importer-Ready Edge-Case Coverage

Missing coverage before this pass:

- duplicate references to the same dependency
- mixed card/effect/payload reference levels
- repeated direct and transitive paths
- disconnected dependency components
- independent cards preserving input order
- effect and payload references to the same dependency
- explicit skip-order fixtures for missing refs, duplicate card ids, invalid cards, and strict unknown-field failures
- non-strict unknown fields that still allow ordering
- hidden-token-safe dependency export edge cases

## Current Gap

The existing test-only validator can still populate `DependencyOrderCardIds` when a bundled card has card-level validation diagnostics, such as unsupported payload types or strict unknown card fields.

Importer-ready behavior should fail closed for card-level validation errors and strict validation errors by leaving dependency order empty. This is still test-only validation behavior and does not create a production importer.

## Why This Remains Test-Only

The validator parses fixture JSON and serializes fixture validation results for automation tests only.

It does not load CardDB data into runtime, execute effects, mutate game state, generate activation candidates or actions, or expose production parser/export APIs.

## Why Production Importer/Loader Remains Deferred

Production import still needs separate design for data ownership, packaging, source versioning, zone ownership, hidden-information observation, runtime provider input, and EffectRunner-backed payload resolution.

These edge-case fixtures are planning artifacts for that future work.

## Hidden-Information Policy

Dependency order may include safe card ids from normal fixtures.

Diagnostics and exports must not echo hidden reference values, full JSON snippets, labels, source text, or unsafe tokens such as `SECRET`.
