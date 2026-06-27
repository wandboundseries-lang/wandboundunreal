# CardDB Unreal Bundle Dependency Diagnostics Report

## Purpose

Added test-only CardDB bundle dependency ordering diagnostics.

This remains fixture validation only. It is not a production CardDB importer, loader, zone model, runtime activation path, gameplay rule, or EffectRunner path.

## Dependency Edge Policy

Existing test-only `references` fields now define dependency edges:

- card-level `references.card_ids[]`: the referencing card depends on the referenced card.
- effect-level `references.card_ids[]`: the owning card depends on the referenced card.
- effect-level `references.effect_refs[]`: the owning card depends on the referenced card.
- payload-level `references.card_ids[]`: the owning card depends on the referenced card.

If card A depends on card B, card B must appear before card A in `DependencyOrderCardIds`.

## Dependency Order Policy

`FWBCardDBBundleSchemaValidationResult::DependencyOrderCardIds` is populated only when dependency ordering is unambiguous and acyclic.

Dependency ordering is skipped when:

- duplicate card ids exist
- missing card references exist
- missing effect references exist
- card ids cannot be read safely from all bundle cards

When skipped, `DependencyOrderCardIds` remains empty. Existing duplicate and missing-reference diagnostics explain the root cause.

## Stable Tie-Break Policy

Acyclic dependency order uses deterministic topological sorting.

When multiple cards are simultaneously available, original `cards[]` order is the tie-break.

Independent cards with no dependencies therefore preserve input order.

## Self-Reference Diagnostic Behavior

Self-dependencies fail with:

```text
dependency_self_reference
```

The diagnostic uses the referencing card index, safe card id, optional effect id, and reference JSON path.

Self-reference is treated as a dependency error and leaves `DependencyOrderCardIds` empty. It is reported as `dependency_self_reference` rather than also emitting a cycle diagnostic.

## Cycle Diagnostic Behavior

Cycles fail with:

```text
dependency_cycle_detected
```

The validator reports the first deterministic cycle-closing edge found by `cards[]`, effect, payload, and reference order. The diagnostic uses generic text and safe owner context only.

Multiple cycles are intentionally not expanded into full cycle lists in this pass so diagnostics remain stable and avoid leaking referenced values.

## Missing-Reference And Duplicate-Id Interaction

Missing references are detected before dependency ordering. If a missing reference exists, dependency order is skipped and remains empty.

Duplicate card ids make the bundle reference index ambiguous. If duplicate card ids exist, dependency order is skipped and remains empty.

## Hidden-Token Diagnostic Safety

Dependency diagnostics do not include full JSON snippets or referenced id values.

Hidden-token fixture coverage verifies diagnostic message, card id, bundle card id, effect id, and JSON path do not leak `SECRET` tokens. A hidden missing reference stops at `missing_card_reference`, and dependency ordering remains empty.

## Valid Fixture Coverage

Added valid dependency fixtures:

- `valid_bundle_dependency_order_simple_chain.json`
- `valid_bundle_dependency_order_reverse_input.json`
- `valid_bundle_dependency_order_diamond.json`
- `valid_bundle_dependency_order_independent_tiebreak.json`

These cover dependency-before-dependent ordering, reverse input order, diamond dependencies with stable tie-breaks, and independent-card input-order preservation.

## Invalid Fixture Coverage

Added invalid dependency fixtures:

- `invalid_bundle_dependency_self_reference.json`
- `invalid_bundle_dependency_cycle_two_cards.json`
- `invalid_bundle_dependency_cycle_three_cards.json`
- `invalid_bundle_dependency_cycle_effect_ref.json`
- `invalid_bundle_dependency_cycle_payload_ref.json`
- `invalid_bundle_dependency_hidden_token_safe.json`

These cover self-dependencies, card-level cycles, effect-reference cycles, payload-reference cycles, and hidden-token safety.

## Test Coverage

Added `Wandbound.Core.CardDBBundleSchemaFixtureValidation.*` coverage for:

- dependency order simple chain
- dependency order reverse input
- dependency order diamond
- dependency order independent tie-break
- dependency self-reference diagnostics
- two-card cycle diagnostics
- three-card cycle diagnostics
- effect-reference cycle diagnostics with effect context
- payload-reference cycle diagnostics with payload path context
- hidden-token safety
- missing-reference dependency-order skip
- duplicate-card dependency-order skip
- stable diagnostic string mappings

Existing cross-reference, bundle schema, root-card schema, and source guard tests still pass.

## Expected Export Snapshots

Dependency diagnostics now have test-only expected JSON export snapshots under:

```text
Reference/GodotCanon/CardDBSchemaFixtures/ExpectedExports/
```

These snapshots serialize dependency order and diagnostic context for future importer planning. They are not production data and do not expose a production export API.

## Confirmations

- this remains test-only
- no production importer was implemented
- no production loader was added
- no production zones were added
- no Core or Runtime source files were changed
- no Godot project files were changed
- no effects are executed
- no activation candidates or activation actions are generated by production code
- no `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` changes were made
- no UI, response windows, Blueprints, `.uasset`, or `.umap` files were changed
