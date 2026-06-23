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
- card activation legal action generation
- card ownership and zone checks
- costs and RR/RL payments
- timing and once-per-turn gates
- target selection UI
- response windows and effect negation
- passives, wands, and card-specific effects
