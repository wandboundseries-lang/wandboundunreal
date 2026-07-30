# Production CardDB Runtime

## Ownership

`WandboundCardDB` loads explicit text manifests into an immutable
`FWBProductionCardDatabase`. It validates and maps data into the existing
`FWBCardDefinitionRepository`; it does not mutate match state or execute effects.
`WandboundCore` remains authoritative for legality and mutation.

The CardDB snapshot contains definitions and provenance only. HP, ownership,
zones, deck position, tiles, concealed marker identity, and effect execution
state remain in the per-match Core state.

## Files

- `ProductionManifest.schema.json`: root suite and included manifest schema.
- `ProductionCardDB.schema.json`: definition bundle schema.
- `ProductionMatchSpec.schema.json`: deterministic two-player setup schema.
- `TestFixtures/ProductionPipeline/`: synthetic validation data, never canonical
  production content.

All paths are rooted under `Data/CardDB/`. Build rules stage that tree as NonUFS
data at the same project-relative location. Packaging uses an explicit allowlist
for schemas and `Production/InitialCanonical`; synthetic test fixtures are not
staged. Runtime loading accepts only an explicit root suite and never enumerates
arbitrary JSON files.

## Supported Definitions

The current production snapshot supports:

- `character` and `hero`, mapped to Core Character definitions.
- `wand`, mapped to Core Wand definitions.
- `trap` and `npc`, used by canonical marker setup.
- `action` as a validated identity only; hand Action execution remains
  unsupported and fails closed if an activation is declared.

Unit `ar` maps to canonical attack range. The current rules kernel does not have
a CardDB base-Armor field, so this importer does not invent one.

Supported activation payloads are limited to deterministic damage, healing,
status application, and armor operations already owned by Core. Production
activation sources are Board Characters/Heroes or Equipped Wands. Quick/React
timing, passive scripting, Hybrid rules, and arbitrary effect text are rejected.

## Determinism

Suites declare manifests, manifests declare includes and bundles, and every
definition names its owning manifest. Paths are normalized and checked against
traversal. Cycles, duplicate ownership, duplicate IDs, inconsistent bundle
versions, unknown fields, malformed values, and unsupported behavior produce
deterministically sorted diagnostics.

Records are sorted by lowercase canonical definition ID. The SHA-256 content
digest is calculated from canonical semantic content, independent of filesystem
enumeration and JSON record ordering. A match specification must pin that
digest, explicit deck order, seed, first player, Heroes, and marker placements.
Production suites may additionally pin a `bundle_lock.json`; runtime validates
its manifest list, definition IDs/count, semantic digest, provenance, and
transfer-report digest format before exposing the immutable snapshot.

## Runtime Startup

Development local play remains the default. Production-data mode is explicit:

```text
-WandboundProductionData
-WandboundCardBundle=Data/CardDB/<suite>/root_manifest.json
-WandboundMatchSpec=Data/CardDB/<suite>/match_spec.json
```

Synthetic test bundles additionally require:

```text
-WandboundAllowTestCardBundle
```

The runtime validates the complete bundle and match specification before
initializing the existing match host. Failure produces a named reason and no
partial match. A successful actor restart reuses its validated immutable
snapshot and initialization request.

The first canonical suite has two evidenced Hero candidates but no tracked legal
deck composed only of supported definitions. It therefore ships
`match_status.json` instead of a fabricated match specification. Explicit
production bootstrap loads and validates the suite, then fails with
`production_match_spec_blocked_by_canonical_deck_evidence`.

## Public Data

Visible units expose definition ID, display name, public category, factions,
tags, and Core combat values. The viewer's own hand exposes public definition
metadata and Wand RR. Activation actions expose the validated public effect
label and a generic target prompt.

Opponent hand contents, private deck order, concealed marker identity,
unrevealed cards, and private effect parameters are not added to observations or
presentation structs.

## Character Models

Character-model manifests may validate `card_definition_id` against a supplied
CardDB repository. Missing IDs can warn in exploratory mode or fail in required
mode; non-Character definitions always fail. CardDB loading does not import
models, and model import does not mutate CardDB definitions.
