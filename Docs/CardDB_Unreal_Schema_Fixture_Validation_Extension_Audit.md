# CardDB Unreal Schema Fixture Validation Extension Audit

## Scope

This audit supports an extension to the existing test-only CardDB schema fixture validator. The pass remains under `WandboundTests` and `Reference/GodotCanon/CardDBSchemaFixtures/` only.

No production CardDB importer, production loader, production zones, runtime generation, gameplay behavior, UI, response windows, passives, wands, activation `FWBAction`, `WBActionCodec`, or `WBRules::GenerateLegalActions` changes are part of this pass.

## Files Inspected

- `Source/WandboundTests/Private/WBCardDBSchemaFixtureValidator.h`
- `Source/WandboundTests/Private/WBCardDBSchemaFixtureValidator.cpp`
- `Source/WandboundTests/Private/WBCardDBSchemaFixtureValidationTests.cpp`
- `Reference/GodotCanon/CardDBSchemaFixtures/`
- `Docs/CardDB_Unreal_Schema_Fixture_Validation_Audit.md`
- `Docs/CardDB_Unreal_Schema_Fixture_Validation_Report.md`
- `Docs/CardDB_Unreal_Schema_Plan.md`
- `Docs/CardDB_Unreal_Schema_Examples.md`

## Current Validator Diagnostics

Existing diagnostics cover:

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
- `invalid_rr_cost`
- `invalid_status_id`
- `hidden_info_policy_violation`
- `player_facing_label_contains_internal_term`
- `json_parse_failed`

## Current Valid Fixture Coverage

Current valid fixtures cover:

- damage payload mapping
- heal payload mapping
- Burn status payload mapping
- armor restore payload mapping
- source gate cost and once-per-turn metadata

## Current Invalid Fixture Coverage

Current invalid fixtures cover:

- missing schema version
- missing card id
- missing public name
- missing effect id
- duplicate effect id
- unsupported payload type
- unsupported payload operation
- unsupported target requirement
- unsupported timing
- negative RR cost
- invalid status id
- internal player-facing label terms
- hidden-info policy violation

## Missing Negative Cases

Missing cases identified for this extension:

- unsupported card kind
- unsupported source zone
- malformed JSON
- missing payload array
- payload field present but not an array
- empty payload array on an activated effect
- activated effects present but not an array
- source gate present but not an object
- cost gate present but not an object
- non-numeric or negative amount/duration fields

## Payload Array Policy

Chosen policy:

- `activated_effects` may be missing or empty for vanilla/passive cards.
- if an activated effect exists, `payloads` must exist.
- `payloads` must be an array.
- `payloads` must not be empty.
- missing payload arrays fail with `payloads_missing`.
- non-array payload fields fail with `payloads_malformed`.
- empty payload arrays fail with `payloads_empty`.

Rationale: activated effects in the current schema slice should map to the existing activation scaffolding. Silent no-op activated effects would hide authoring mistakes and must fail closed.

## Malformed Field Policy

Chosen policy:

- missing `activated_effects` is allowed.
- `activated_effects` present but not an array fails `activated_effects_malformed`.
- missing `source_gate` is allowed and uses defaults.
- `source_gate` present but not an object fails `source_gate_malformed`.
- missing `cost_gate` is allowed.
- `cost_gate` present but not an object fails `cost_gate_malformed`.

## Numeric Validation Policy

Chosen policy:

- `required_rr` must be numeric when present.
- negative `required_rr` keeps the existing `invalid_rr_cost` diagnostic.
- payload `amount` fields must be numeric when present.
- payload `amount` fields must be non-negative.
- `turns_remaining` must be numeric when present.
- `turns_remaining` must be non-negative.
- malformed or negative amount/duration fields fail with `invalid_numeric_field`.

## Why This Pass Remains Test-Only

The validator proves the Unreal-owned schema contract with small fixture examples. It should remain isolated from production modules until production CardDB ownership, hidden-information observation, zone state, response windows, and broader effect mapping are designed.

## Why Production Importer Remains Deferred

Production import remains deferred because the current work only validates authoring shape and maps valid examples into `FWBCardDefinition` for test verification. It does not load production data, own card zones, execute effects, generate runtime activation actions, or resolve unsupported CardDB behaviors such as passives, wands, response effects, and card-specific exceptions.
