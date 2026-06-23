# Card Definition Activation Expansion Report

## Scope

This pass adds fixture-driven Unreal-owned card definitions that expand into existing `FWBCardActivationCommand` values.

Implemented:

- `FWBCardDefinition`
- `FWBCardEffectDefinition`
- `EWBCardDefinitionKind`
- `EWBCardEffectTargetRequirement`
- `FWBCardActivationExpansionRequest`
- `FWBCardActivationExpansionResult`
- `WBCardActivationExpansion::BuildActivationCommand`
- `WBCardActivationExpansion::BuildActivationCommandCandidates`
- fixture operations `build_card_activation_command` and `build_and_apply_card_activation_command`
- fixture-driven card definition activation scenarios

Not implemented:

- Godot CardDB import
- production card JSON loading
- legal activation action generation
- card ownership, costs, timing, or once-per-turn checks
- UI target selection, activation windows, or response windows
- effect negation, prevention, passives, wands, or card-specific behavior
- Blueprints, assets, `.uasset`, or `.umap` work

## Behavior

The builder validates:

- card id is non-empty
- requested effect id is non-empty
- source player id is 0 or 1
- requested effect id matches exactly one activated effect
- effect target requirement is satisfied
- payloads are non-empty
- payload operations are one of armor, status, damage, or heal

On success, it builds one `FWBCardActivationCommand` using:

- card id as source card id
- effect id as source effect id
- request player/source unit
- request target
- authored generic payloads in definition order
- request debug activation id

Candidate generation is deterministic: effects are visited in definition order, and candidate targets are visited in supplied order. Invalid target mismatches are skipped by omission.

Payload source and target ids remain authored. When they are `-1`, the existing card activation/effect request path fills them from the activation command and request target during application.

## Fixtures

Added golden scenarios:

- `card_definition_build_damage_activation_command.json`
- `card_definition_build_heal_activation_command.json`
- `card_definition_build_status_activation_command.json`
- `card_definition_build_armor_activation_command.json`
- `card_definition_build_and_apply_damage_activation.json`
- `card_definition_build_and_apply_mixed_activation.json`
- `card_definition_missing_effect_fails.json`
- `card_definition_ambiguous_effect_fails.json`
- `card_definition_missing_target_fails.json`
- `card_definition_source_metadata_not_public_trace.json`

These fixtures are Unreal-owned canon scaffolding and do not import or runtime-load Godot CardDB data.

## Guardrails

`WBCardActivationExpansion` does not include or call `WBRules`, `WBEffectRunner`, `GenerateLegalActions`, or `ApplyCardActivationCommand`.

Tests cover:

- direct command build for armor/status/damage/heal
- build-and-apply through the existing effect runner
- failure reasons for missing, ambiguous, and untargeted effects
- no-target build-only behavior
- deterministic candidate ordering
- invalid candidate target skipping
- build-only no-mutation behavior
- source guard
- unchanged legal action generation and action ids
- fixture-driven build/build-and-apply scenarios
- source metadata and hidden-zone card ids excluded from traces/runtime serialization

## Follow-Up - Candidate Generation

Card activation candidates can now be generated from fixture-owned `FWBCardDefinition` values and externally supplied candidate targets via `WBCardActivationCandidateGenerator`.

The candidate generator wraps `FWBCardActivationCommand` values, preserves source/effect/target ordering, and keeps existing `FWBAction` legal generation and `WBActionCodec` unchanged.

## Follow-Up - Activation Legal Action Family

The fixture flow is now:

```text
FWBCardDefinition -> FWBCardActivationCandidate -> FWBCardActivationLegalAction
```

`WBCardActivationLegalActionGenerator` converts candidates into a separate activation legal action family for future UI/runtime selection while preserving explicit application through `WBEffectRunner::ApplyCardActivationCommand`.
