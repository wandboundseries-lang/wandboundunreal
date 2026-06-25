# Card Activation Candidate Generation Report

## Purpose

This pass adds deterministic fixture-driven card activation candidate generation from Unreal-owned `FWBCardDefinition` data and externally supplied candidate targets.

It creates future-safe activation candidates without adding real player-facing activation legal actions yet.

## Added Core Types

- `FWBCardActivationCandidate`
- `FWBCardActivationCandidateSource`
- `FWBCardActivationCandidateGenerationResult`
- `WBCardActivationCandidateGenerator`

Each candidate contains deterministic internal selection id, player/source ids, source card/effect metadata, a public label, target ref, and wrapped `FWBCardActivationCommand`.

## Generator Behavior

`WBCardActivationCandidateGenerator::GenerateCandidates` preserves deterministic order:

1. supplied source order
2. activated effect order
3. supplied target order

Generation is build-only and does not mutate `FWBGameStateData`. Candidates are applied only when a caller explicitly runs the wrapped command through `WBEffectRunner::ApplyCardActivationCommand`.

## Structural Checks

Fatal malformed input fails the generation result: invalid/missing player, missing source unit, owner mismatch, missing card id, missing/ambiguous effect id, empty payloads, unknown payload operations, or unknown target requirements.

Dynamic invalid board entries are skipped: removed/defeated source units, Stunned/Frozen source units, missing/removed/defeated unit targets, and per-target mismatch failures.

`CannotAttack`/`no_attack` does not block activation candidates in this pass.

## Candidate IDs

Candidate ids use the deterministic internal format:

```text
activate:p{player}:u{source}:c{card_id}:e{effect_id}:t{target_unit}:x{x}:y{y}:wa{ax},{ay}:wb{bx},{by}
```

These ids are not `WBActionCodec` ids and are not emitted as public trace ids.

## Public Labels

Candidate labels use `FWBCardEffectDefinition::PublicLabel` when present, otherwise `Activate`.

Labels intentionally avoid raw operation names such as `damage_effect`, `armor_effect`, and hidden source metadata.

## Fixtures

Added fixture operation:

```json
"operation": "generate_card_activation_candidates"
```

Added golden scenarios:

- `card_activation_candidates_damage_effect.json`
- `card_activation_candidates_heal_effect.json`
- `card_activation_candidates_status_effect.json`
- `card_activation_candidates_armor_effect.json`
- `card_activation_candidates_mixed_effects_ordered.json`
- `card_activation_candidates_stunned_source_excluded.json`
- `card_activation_candidates_removed_source_excluded_or_fails.json`
- `card_activation_candidates_removed_target_excluded.json`
- `card_activation_candidates_no_targets_returns_empty.json`
- `card_activation_candidates_candidate_id_stability.json`

## Guardrails

The candidate generator does not import Godot CardDB, production-load card JSON, call `WBRules::GenerateLegalActions`, call `WBEffectRunner`, generate `FWBAction` values, change `WBActionCodec`, open UI/response windows, or implement negation, costs, card zones, passives, wands, counters, or card-specific behavior.

## Hidden Information

Source card/effect ids and debug activation ids remain internal metadata. Tests verify these values do not leak into replay trace serialization or runtime public summaries after the wrapped command is applied.

## Validation

Build:

```text
Result: Succeeded
Total execution time: 4.69 seconds
```

Full Wandbound automation:

```text
succeeded=537
succeededWithWarnings=0
failed=0
notRun=0
```

## Remaining Work

- production CardDB importer
- production card ownership and real zone checks
- costs and RR/RL payments
- response-window activation timing
- target selection UI
- response windows and effect negation
- passives, wands, and card-specific effects

## Follow-Up - Activation Legal Action Family

Card activation candidates can now be converted into a separate activation legal action family through `WBCardActivationLegalActionGenerator`.

That family preserves candidate ordering, uses candidate ids as activation action ids, carries clean public labels for future UI, and remains separate from `FWBAction`, `WBRules::GenerateLegalActions`, and `WBActionCodec`.

## Follow-Up - Source Gates

Candidate generation now honors fixture-driven `FWBCardActivationSourceGateDefinition` checks before expanding each activated effect.

The gates cover fixture source zone metadata, normal-turn priority timing, source ownership, Stunned/Frozen policy, external cost-satisfied flags, and once-per-turn usage keys. Failed gates skip only that effect/source and preserve remaining source/effect/target order.

Existing fixtures without explicit source gates keep their old behavior, including legacy Frozen source exclusion.

## Follow-Up - Usage Commit Preservation

Candidate generation now passes the evaluated `FWBCardActivationSourceGateContext` into `WBCardActivationExpansion`. Once-per-turn effects produce commands with `FWBCardActivationUsageCommit`, and `FWBCardActivationCandidate::Command` preserves that metadata for explicit future application.

Generation remains read-only and does not mark usage.

## Follow-Up - Cost Gate Filtering

Candidate generation now filters fixture effects through detailed source-gate cost affordability when supplied by `source_gate.cost_gate` and `source_gate_context.cost_context`.

The generator still does not pay costs, mutate `RLUsed`, trigger overflow, import CardDB, call `WBEffectRunner`, call `WBRules::GenerateLegalActions`, or emit `FWBAction` activation entries. It simply skips effects whose source gate reports `cost_affordability_missing`, `cost_not_affordable`, `cost_requirement_mismatch`, or `invalid_cost_requirement`.

## Follow-Up - Affordability-Prepared Sources

Candidate sources can now be prepared with `WBCardActivationAffordability::BuildCandidateSourceWithAffordability` before generation. The helper copies the source, queries read-only affordability for effects that require external affordability, and stores effect-specific source-gate contexts in `EffectIdToSourceGateContext`.

Candidate generation consumes the effect-specific context when present and otherwise falls back to the source-level context. This supports multiple activated effects with different RR costs while preserving the generator's read-only, no-payment boundary.

## Follow-Up - Fixture Source-Zone Ownership

Candidate generation now filters through fixture-only source-zone ownership gates when an effect source gate requests them.

The generator supplies `SourceCardId` from `FWBCardDefinition::CardId` when fixture context omits it. Effect-specific source-gate contexts inherit the source-level player id, source unit id, source card id, source zone, and fixture zone context where missing.

This supports fixture Hand, Equipped, and Discard ownership tests without reading or mutating production `Deck`, `Hand`, or `Discard` arrays.

## Follow-Up - Board Source Parity

Candidate generation now filters explicit Board-source activation candidates by visible unit `CardId` parity. Board sources use `FWBUnitState::CardId` rather than `FixtureZoneContext` entries.

`SourceCardId` inheritance applies to Board sources as well: effect-specific context, source-level context, then `FWBCardDefinition::CardId`. Effect-specific overrides can intentionally fail parity when they do not match the visible source unit card id.

For sources whose effects are all explicit Board fixture-ownership gates, missing or wrong-owner source units are handled by source gates as candidate filtering. Generic non-Board malformed-source behavior remains unchanged.
