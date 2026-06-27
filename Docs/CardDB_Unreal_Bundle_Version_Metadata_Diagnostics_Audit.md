# CardDB Unreal Bundle Version Metadata Diagnostics Audit

## Scope

This audit covers test-only CardDB bundle source-versioning and metadata diagnostics under `Source/WandboundTests/Private/`.

No production CardDB importer, production loader, production zones, runtime activation behavior, rules behavior, action codec behavior, UI, response windows, Blueprints, `.uasset`, or `.umap` files are part of this pass.

## Current Bundle-Level Version Fields

Bundle fixtures currently allow:

- `bundle_schema_version`
- `carddb_version`
- `source_version`
- `migration_notes`
- `metadata`

`bundle_schema_version` is required and must equal `1`.

`carddb_version` is currently required as a non-empty string, but it has no length or character policy beyond the missing-field diagnostic.

`source_version` and `migration_notes` are accepted top-level fields, but they currently have no type, length, allowed-character, or hidden-token validation.

## Current Metadata Allow-List Behavior

Bundle metadata uses the existing metadata allow-list:

- `author`
- `notes`
- `source`
- `version`
- `test_only`

The current allow-list is enforced only when strict unknown-field rejection is enabled.

Before this pass, bundle metadata value types and lengths were not validated, and metadata hidden-token checks were not applied.

## Current Strict And Non-Strict Policy

Default validation remains non-strict. Unknown fields are ignored unless another validation path rejects malformed data.

Strict validation rejects unknown bundle metadata fields with `unknown_metadata_field`.

That strict unknown-field policy should remain unchanged; this pass adds test-only value-shape diagnostics alongside it.

## Current Export Snapshot Behavior

Bundle expected export snapshots use deterministic condensed JSON with:

- `ok`
- clean-file-name `source_path`
- `card_count`
- `dependency_order_card_ids`
- sanitized diagnostics

Exports omit diagnostic messages, full source JSON, public text, public labels, payload bodies, metadata values, and hidden referenced values.

## Missing Validation

Missing before this pass:

- `carddb_version` format and max length
- `source_version` type, allowed characters, and max length
- `migration_notes` type, max length, and hidden-token policy
- bundle metadata object/type validation
- bundle metadata field value types and max lengths
- bundle metadata hidden-token policy

## Proposed Metadata Type Policy

Bundle metadata should remain optional. When present, it must be an object.

Value policy:

- `author`: string, max 64
- `notes`: string, max 512
- `source`: string, max 128
- `version`: string, max 64
- `test_only`: boolean

Unknown metadata fields still use strict-mode `unknown_metadata_field`.

Malformed metadata shape or value type uses `metadata_malformed`.

Hidden tokens in metadata string values fail with `hidden_info_policy_violation`.

## Why This Remains Test-Only

The validator parses fixture JSON and serializes fixture validation results for automation tests only.

It does not load CardDB data into runtime, execute effects, mutate game state, generate activation candidates or actions, or expose production parser/export APIs.

## Why Production Importer/Loader Remains Deferred

Production import still needs separate design for data ownership, packaging, source versioning semantics, migration compatibility, zone ownership, hidden-information observation, runtime provider input, and EffectRunner-backed payload resolution.

These diagnostics are planning artifacts for that future work.

## Hidden-Information Policy

Diagnostics and exports must not echo hidden source values, metadata values, source snippets, referenced ids, labels, logs, or unsafe tokens such as `SECRET`.

Hidden values should produce generic diagnostics with stable JSON paths only.
