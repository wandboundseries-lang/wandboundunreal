# CardDB Unreal Schema Plan

## Purpose

This document defines the future Unreal-owned CardDB schema.

This is not the importer. This schema is the target for future CardDB import and mapping work. Future imported card data should map into existing Unreal activation scaffolding:

- `FWBCardDefinition`
- `FWBCardEffectDefinition`
- `FWBCardActivationSourceGateDefinition`
- `FWBCardActivationCostGateDefinition`
- `FWBGenericEffectPayload`

Unsupported effects must fail closed.

## Non-Goals

- no importer in this pass
- no production JSON loader
- no real zones
- no UI
- no response windows
- no effect negation
- no passives/wands
- no activation `FWBAction`
- no `WBActionCodec` changes

## Schema Versioning

Top-level schema fields:

```json
{
  "schema_version": 1,
  "carddb_version": "dev",
  "source_version": "manual",
  "migration_notes": [],
  "metadata": {},
  "cards": []
}
```

Policy:

- `schema_version` is required.
- `carddb_version` is required for production data.
- `source_version` identifies the source data build or import batch.
- `migration_notes` records authoring migrations.
- unknown top-level fields fail in strict mode.
- optional authoring metadata is allowed only under `metadata`.
- unknown fields inside known objects fail in strict mode unless explicitly listed under `metadata`.

Strict fixture validation is opt-in for the current test-only validator. Default validation remains non-strict until a production importer boundary exists.

Allowed card metadata fields:

- `author`
- `notes`
- `source`
- `version`
- `test_only`

## Future Bundle Shape

Future production-shaped CardDB data should use a top-level bundle wrapper:

```json
{
  "bundle_schema_version": 1,
  "carddb_version": "test_bundle_v1",
  "source_version": "fixture",
  "migration_notes": "",
  "metadata": {
    "notes": "fixture-only"
  },
  "cards": []
}
```

Bundle policy:

- `bundle_schema_version` is required and must equal `1`.
- `carddb_version` is required and must be a non-empty string.
- `cards` is required, must be an array, and must not be empty.
- each `cards[]` entry validates through the same root-card schema behavior.
- duplicate non-empty `card_id` values fail closed with `card_id_duplicate`.
- strict mode rejects unknown bundle top-level fields with `unknown_top_level_field`.
- strict mode rejects unknown bundle metadata fields with `unknown_metadata_field`.
- production import and loading remain future work.

## Card Definition Shape

Future JSON-like shape:

```json
{
  "schema_version": 1,
  "card_id": "example_card",
  "public_name": "Example Card",
  "kind": "character",
  "public_text": "Activate: Deal 2 damage.",
  "tags": [],
  "stats": {},
  "activated_effects": []
}
```

Mapping:

- `card_id` maps to `FWBCardDefinition.CardId`
- `public_name` maps to `FWBCardDefinition.PublicName`
- `kind` maps to `FWBCardDefinition.Kind`
- `activated_effects` maps to `FWBCardDefinition.ActivatedEffects`

Card kinds:

- `character`
- `wand`
- `action`
- `terrain`
- `wall`
- `fixture`
- `unknown` is unsupported and fails closed

## Public Text and Label Policy

`public_name` is player-facing.

`public_text` is player-facing.

Effect `public_label` is player-facing.

Labels must not contain internal terms.

Forbidden in player-facing labels:

- `EffectRunner`
- `Rules`
- raw effect type names
- schema fields
- hook names
- `from_tile`
- `to_tile`
- `chosen_tile`
- player ids

Allowed examples:

- `Activate`
- `Deal 2 damage`
- `Heal 2 HP`
- `Apply Burn`
- `Restore Armor`
- `Choose Unit`
- `Choose Tile`
- `Choose Wall`

## Stats Schema

Optional stats for unit cards:

```json
{
  "stats": {
    "hp": 6,
    "max_hp": 6,
    "atk": 2,
    "ar": 1,
    "base_rl": 4,
    "current_rl": 4,
    "rr": 1
  }
}
```

Policy:

- `hp` and `max_hp` seed unit HP where relevant.
- `atk` maps to attack value.
- `ar` maps to attack range.
- current engine state uses `RLTotal` and `RLUsed`.
- current engine policy treats `RLTotal` as Current RL for affordability.
- future schema should prefer a clear Base RL / Current RL distinction.
- `RL Used` is runtime state, not static card schema.
- `rr` may describe a static activation/equip requirement, but individual activation effects should declare their own cost gates when needed.
- strict fixture validation rejects unknown stats fields with `unknown_card_field`.

## Activated Effects Schema

Future JSON-like shape:

```json
{
  "effect_id": "deal_2",
  "public_label": "Deal 2 damage",
  "target_requirement": "unit",
  "source_gate": {},
  "payloads": []
}
```

Mapping:

- `effect_id` maps to `FWBCardEffectDefinition.EffectId`
- `public_label` maps to `FWBCardEffectDefinition.PublicLabel`
- `target_requirement` maps to `FWBCardEffectDefinition.TargetRequirement`
- `source_gate` maps to `FWBCardEffectDefinition.SourceGate`
- `payloads` maps to `FWBCardEffectDefinition.Payloads`
- `metadata` is authoring-only and may contain `notes`, `source`, and `test_only`

Target requirements:

- `none`
- `unit`
- `tile`
- `wall_edge`

Target requirement policy:

- target requirement defines authoring shape only.
- target legality is not determined by UI.
- candidate generation filters by gates and supplied candidate targets.
- real UI picking remains future work.

Payload array policy:

- `activated_effects` may be missing or empty for vanilla/passive cards.
- if an activated effect exists, `payloads` must exist.
- `payloads` must be an array.
- `payloads` must not be empty.
- missing payload arrays fail with `payloads_missing`.
- non-array payload fields fail with `payloads_malformed`.
- empty payload arrays fail with `payloads_empty`.
- silent no-op activated effects are not allowed in this schema slice.

## Source Gate Schema

Future JSON-like shape:

```json
{
  "required_zone": "board",
  "timing": "normal_turn_priority",
  "requires_source_unit": true,
  "requires_source_unit_ownership": true,
  "blocked_by_stunned": true,
  "blocked_by_frozen": false,
  "requires_costs_satisfied_externally": false,
  "once_per_turn": false,
  "once_per_turn_key": "",
  "requires_fixture_zone_ownership": false,
  "cost_gate": {}
}
```

Mapping:

- maps to `FWBCardActivationSourceGateDefinition`
- optional `metadata` may contain `notes` and `test_only`

Allowed zones:

- `fixture`
- `board`
- `hand`
- `equipped`
- `discard`
- `deck` is unsupported unless a future explicit card rule permits it

Timing:

- `any`
- `normal_turn_priority`
- `response_window` is unsupported until response system integration

## Cost Gate Schema

Future JSON-like shape:

```json
{
  "requires_external_affordability": true,
  "required_rr": 2,
  "cost_kind": "RR"
}
```

Mapping:

- maps to `FWBCardActivationCostGateDefinition`
- optional `metadata` may contain `notes` and `test_only`

Policy:

- `required_rr` is an authoring requirement.
- affordability query computes `Available RL`.
- payment commit mutates `RL Used` only during successful activation execution.
- no payment occurs during provider output generation.
- overflow and wand destruction are not implemented yet.

## Usage / Once-Per-Turn Schema

Policy:

- `once_per_turn` is a boolean source-gate field.
- `once_per_turn_key` is optional.
- if no explicit key is supplied, the future importer should let the core path derive the default key from player/source/card/effect.
- usage marking occurs after successful activation.
- failed activation does not mark usage.
- response and negation special cases are deferred.

## Generic Payload Mapping

### armor_effect

Example:

```json
{
  "type": "armor_effect",
  "operation": "add_current_armor",
  "amount": 2,
  "target": "selected"
}
```

Mapping:

- `FWBGenericEffectPayload.Operation = ArmorEffect`
- payload maps to `FWBArmorEffectRequest`

Supported operations:

- `add_current_armor`
- `reduce_current_armor`
- `set_current_armor`
- `add_max_armor`
- `reduce_max_armor`
- `set_max_armor`
- `restore_armor_to_max`

### status_effect

Example:

```json
{
  "type": "status_effect",
  "operation": "apply_status",
  "status_id": "Burn",
  "turns_remaining": 2,
  "target": "selected"
}
```

Mapping:

- `FWBGenericEffectPayload.Operation = StatusEffect`
- payload maps to `FWBStatusEffectRequest`

Supported operations:

- `apply_status`
- `remove_status`
- `set_status_duration`
- `add_status_duration`
- `reduce_status_duration`
- `cleanse_status`
- `cleanse_all_statuses`

Status names:

- `Burn`
- `Poison`
- `Rooted`
- `Stunned`
- `Frozen`
- `Cannot Attack` if supported by current aliases/checks

### damage_effect

Example:

```json
{
  "type": "damage_effect",
  "amount": 2,
  "bypass_armor": false,
  "damage_cause": "Effect",
  "target": "selected"
}
```

Mapping:

- `FWBGenericEffectPayload.Operation = DamageEffect`
- payload maps to `FWBDamageEffectRequest`

Policy:

- uses damage resolution.
- can interact with armor.
- lethal damage runs zero-HP cleanup through the current activation execution path.
- death triggers and card-specific replacement are deferred.
- optional payload `metadata` may contain `notes` and `test_only`.

### heal_effect

Example:

```json
{
  "type": "heal_effect",
  "amount": 2,
  "target": "selected"
}
```

Mapping:

- `FWBGenericEffectPayload.Operation = HealEffect`
- payload maps to `FWBHealEffectRequest`

Policy:

- healing clamps to Max HP.
- no revive unless a future explicit card-specific mechanic supports it.

## Unsupported Payload Policy

Fail-closed behavior:

- unsupported type fails import/validation.
- unsupported operation fails import/validation.
- unsupported target shape fails import/validation.
- unsupported timing fails import/validation.
- malformed JSON fails import/validation.
- malformed `activated_effects`, `source_gate`, `cost_gate`, and `payloads` fields fail import/validation.
- malformed or negative payload numeric fields fail import/validation.
- no silent fallback to no-op.
- diagnostics may include non-hidden card id/effect id in developer logs only.
- public UI should show a safe generic failure.

Unsupported categories for now:

- passives
- wands
- response/negation effects
- replacement/prevention effects
- death triggers
- draw/search/deck manipulation
- marker manipulation
- terrain effects beyond reporting
- summon effects
- wall placement/removal effects unless already supported elsewhere
- random effects
- multi-choice effects
- conditional branches
- modal effects
- complex until-end-of-turn modifiers if not represented

## Target Binding Policy

Documented target references:

- selected unit
- selected tile
- selected wall edge
- source unit
- self
- friendly unit
- enemy unit

Near-term supported policy:

- document selected target and source/self as near-term.
- complex target filters are future work.
- provider/candidate generation supplies candidate target refs.
- UI target picking remains future work.
- CardDB schema may declare target requirement, but not actual selected target.

## Zone and Visibility Policy

CardDB authoring policy:

- source zone requirement belongs in `source_gate`.
- production zone visibility rules belong to the future zone provider.
- CardDB card definitions should not encode hidden-state policy directly.
- board card ids are public under current policy.
- hand/deck hidden policy remains provider/observation responsibility.

## Import Mapping Pipeline

Future flow:

```text
CardDB JSON
-> schema validation
-> Unreal FWBCardDefinition
-> source gates
-> candidate generation
-> activation legal action family
-> runtime provider output
-> runtime session facade
-> selection/resolution/execution
```

Policy:

- importer should not execute effects.
- importer should not generate runtime decision state directly.
- importer should not mutate rules state.

## Diagnostics and Fail-Closed Errors

Diagnostic categories:

- `schema_version_missing`
- `card_id_missing`
- `public_name_missing`
- `effect_id_missing`
- `effect_id_duplicate`
- `unsupported_card_kind`
- `unsupported_target_requirement`
- `unsupported_source_zone`
- `unsupported_timing`
- `unsupported_payload_type`
- `unsupported_payload_operation`
- `payloads_missing`
- `payloads_malformed`
- `payloads_empty`
- `activated_effects_malformed`
- `source_gate_malformed`
- `cost_gate_malformed`
- `invalid_rr_cost`
- `invalid_status_id`
- `invalid_numeric_field`
- `unknown_top_level_field`
- `unknown_card_field`
- `unknown_effect_field`
- `unknown_source_gate_field`
- `unknown_cost_gate_field`
- `unknown_payload_field`
- `unknown_metadata_field`
- `carddb_version_missing`
- `cards_missing`
- `cards_malformed`
- `cards_empty`
- `card_id_duplicate`
- `bundle_schema_version_missing`
- `bundle_schema_version_unsupported`
- `missing_card_reference`
- `missing_effect_reference`
- `reference_malformed`
- `unknown_reference_field`
- `dependency_cycle_detected`
- `dependency_self_reference`
- `hidden_info_policy_violation`
- `player_facing_label_contains_internal_term`

## Fixture Strategy

Fixture strategy:

- use small Unreal-owned fixture card definitions before production import.
- include one fixture per supported payload family.
- include one fixture per source zone policy.
- include cost/usage fixtures.
- include hidden-info negative fixtures.
- include unsupported payload fail-closed fixtures.
- keep GoldenScenarios for deterministic core behavior.
- keep C++ contract tests for provider/runtime interface behavior.

## Test-Only Fixture Validation

Test-only schema fixture validation now exists under `WandboundTests`.

The fixture validator reads Unreal-owned schema fixtures from `Reference/GodotCanon/CardDBSchemaFixtures/`, validates fail-closed diagnostics, and may map valid fixture data into `FWBCardDefinition` for validation only.

The validator is not a production importer, does not load production CardDB data, does not create production zones, does not execute effects, and does not generate production runtime activation candidates or actions.

Strict fixture validation exists as an explicit option for unknown-field rejection. It rejects unknown top-level/card fields, unknown stats fields, unknown effect/source gate/cost gate/payload fields, and unknown metadata fields. The default validator path remains non-strict for compatibility with earlier schema fixtures.

Test-only bundle validation also exists for future production-shaped `cards[]` fixtures. It validates bundle fields, aggregates root-card diagnostics, rejects duplicate card ids, and maps valid bundle cards into `FWBCardDefinition` values for validation only.

Bundle diagnostic reporting now records stable context for fixture tests:

- zero-based card index for `cards[]` entries
- safe bundle card id
- effect id when available
- stable JSON path

Future production importer diagnostics should preserve equivalent context without logging hidden values or full JSON snippets. Bundle-level diagnostics should appear before card-level diagnostics when detected first, and card-level diagnostics should remain in `cards[]` order.

Test-only bundle reference validation now exists for `references` objects on cards, activated effects, and payloads.

Policy:

- root-card validation checks reference shape only.
- bundle validation resolves `card_ids` and `effect_refs`.
- reference data does not map into `FWBCardDefinition`.
- duplicate card ids skip reference resolution because the bundle index is ambiguous.
- missing references use generic diagnostics and safe owner context only.
- strict mode rejects unknown reference fields with `unknown_reference_field`.

Test-only bundle dependency validation now derives `DependencyOrderCardIds` from validated references. Future importer work may use a dependency order after validation, but production import is still deferred.

Policy:

- dependencies must appear before dependents.
- original `cards[]` order breaks ties.
- duplicate card ids and missing references skip dependency ordering.
- self-references and cycles fail closed before import.
- dependency diagnostics are authoring diagnostics only.

Test-only dependency export snapshots now exist for future importer planning. They serialize dependency order and diagnostic context into expected JSON fixtures without exposing production parser or export APIs.

Test-only bundle-level diagnostic export snapshots now preserve the same export shape for malformed bundle fields. Future importer diagnostics should keep the stable fields `ok`, clean-file-name `source_path`, `card_count`, `dependency_order_card_ids`, and sanitized diagnostic context while continuing to omit messages, full source JSON, and hidden referenced values.

Importer planning should preserve the test-only dependency ordering policy:

- dependencies appear before dependents
- original `cards[]` order breaks ties among currently available zero-dependency cards
- duplicate valid references do not duplicate emitted card ids
- card, effect, and payload references all create dependency edges
- invalid bundles skip dependency ordering and fail closed before import

Bundle version and metadata planning should preserve the test-only policies:

- `carddb_version` is required, max 64 characters, and uses letters, numbers, underscore, hyphen, and dot
- `source_version` is optional, max 64 characters, and may also use slash
- `migration_notes` is optional string metadata, max 512 characters, and cannot contain hidden-information tokens
- bundle metadata fields have fixed value types and max lengths
- hidden-information tokens in metadata fail closed without echoing the values

Source version compatibility planning should preserve the opt-in test-only matrix policy:

- compatibility validation is disabled by default
- a target source version is required when compatibility validation is enabled
- a bundle source version equal to the target passes
- directly supported source versions pass without migration
- explicit source-to-target transitions pass without transforming data
- missing required source versions fail closed with `source_version_missing`
- unsupported source versions fail closed with `source_version_unsupported`
- unsupported transitions fail closed with `source_version_transition_unsupported`
- malformed matrix policy fails closed with `source_version_compatibility_matrix_malformed`

This compatibility policy is validation only. It does not implement schema migration, production import, production loading, production zones, runtime activation generation, or `FWBAction` integration.

## Future CardDB Import Milestones

1. Add schema validation docs/examples.
2. Add JSON schema fixture examples only.
3. Add C++ schema parser in test-only mode.
4. Add fail-closed import diagnostics tests.
5. Add production Unreal-owned CardDB loader.
6. Add provider skeleton that consumes loaded definitions.
7. Add real zone-state observation model.
8. Add CardDB mapping for additional payload types.
9. Add response-window activation declaration model.
10. Decide activation `FWBAction`/`WBActionCodec` integration.

## Open Questions

- discard visibility canon
- equipped/wand public visibility
- marker identity/player perspective
- response-window timing for activation declaration vs resolution
- negation cost/usage/payment timing
- whether activation remains separate from `FWBAction` long-term
- how card-specific exceptions are represented

## Out of Scope

- Godot CardDB importer
- production CardDB loader
- production JSON parser
- production zones
- hand/deck/discard/equipped state
- target picking UI
- response windows
- effect negation
- passives
- wands
- card-specific behavior
- overflow and wand destruction
- activation `FWBAction`
- activation `WBActionCodec` ids
- AI or ML code
- Blueprints
- `.uasset` or `.umap` edits
