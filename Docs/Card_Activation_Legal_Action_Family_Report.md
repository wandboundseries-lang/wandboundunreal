# Card Activation Legal Action Family Report

## Scope

This pass adds a separate fixture-driven activation legal action family from precomputed `FWBCardActivationCandidate` values.

It does not import Godot CardDB, production-load card JSON, add `EWBActionType::Activate`, add activation to `FWBAction`, modify `WBActionCodec`, add activation candidates to `WBRules::GenerateLegalActions`, add UI, open response windows, implement negation, or add card-specific behavior.

## Core Types

Added:

- `FWBCardActivationLegalAction`
- `FWBCardActivationLegalActionSet`
- `FWBCardActivationLegalActionGenerationResult`
- `WBCardActivationLegalActionGenerator`

Each legal action carries:

- stable activation action id
- player id
- source unit id
- public label
- target reference
- source candidate
- wrapped `FWBCardActivationCommand`

## Generator

`WBCardActivationLegalActionGenerator::GenerateFromCandidates`:

- iterates candidates in supplied order
- preserves ordering exactly
- uses the candidate id as the activation action id
- copies the target, candidate, and command
- uses `Candidate.PublicLabel`, falling back to `Activate`
- fails clearly on malformed candidate input
- does not mutate state
- does not call `WBRules`
- does not call `WBEffectRunner`
- does not call `WBRules::GenerateLegalActions`
- does not call `WBActionCodec`

## Action ID Policy

Activation action ids are separate from `WBActionCodec`.

The generator uses `FWBCardActivationCandidate::ActivationCandidateId` directly. Fixture-generated ids currently use the deterministic `activate:p...` format, but non-`activate:` ids are accepted because this family is not part of `FWBAction` or replay action codec yet.

## Public Label Policy

Labels come from `Candidate.PublicLabel` when present, otherwise `Activate`.

Unsafe labels fail generation if they expose internal strings such as:

- `damage_effect`
- `heal_effect`
- `armor_effect`
- `status_effect`
- `EffectRunner`
- `Rules`
- raw schema or hook names

## Presentation Snapshot

Added activation-only presentation helpers:

- `FWBCardActivationLegalActionPresentationEntry`
- `FWBCardActivationLegalActionPresentationSnapshot`
- `WBCardActivationLegalActionPresentation::BuildSnapshot`
- `WBCardActivationLegalActionPresentation::TryFindEntryByActivationActionId`

This snapshot is separate from `WBRuntimeLegalActionPresentation` for `FWBAction`.

`SourceCardId` and `SourceEffectId` are fixture-visible metadata in this activation-only snapshot. They are not trace/public-board/runtime-public-summary fields.

Duplicate activation ids make lookup fail so future UI selection cannot silently pick an ambiguous entry.

## Fixture Operation

Added fixture operation:

```json
"operation": "generate_card_activation_legal_actions"
```

The operation:

- reuses activation source parsing
- generates candidates through `WBCardActivationCandidateGenerator` after source gate filtering
- converts them through `WBCardActivationLegalActionGenerator`
- validates expected activation action ids and labels when present
- validates expected payment commit metadata when `expected.cost_payment_commit` is present
- does not mutate state
- does not add entries to `FWBAction` legal actions

A later verifier-only fixture operation can select activation legal action ids from this family and replay the wrapped activation command through the existing EffectRunner path:

```json
"operation": "apply_card_activation_legal_action_by_id"
```

This replay path is test-only and keeps activation ids outside `FWBAction` and `WBActionCodec`.

## Golden Scenarios

Added:

- `card_activation_legal_actions_damage_effect.json`
- `card_activation_legal_actions_heal_effect.json`
- `card_activation_legal_actions_status_effect.json`
- `card_activation_legal_actions_armor_effect.json`
- `card_activation_legal_actions_ordered.json`
- `card_activation_legal_actions_public_labels.json`
- `card_activation_legal_actions_malformed_candidate_fails.json`
- `card_activation_legal_actions_separate_from_fwbaction.json`

Follow-up Board-source coverage added focused fixtures for damage, status, armor, heal, deterministic ordering, cost commit preservation, usage commit preservation, selected-id replay, card mismatch failure, `FWBAction` separation, action-id stability, and hidden metadata exclusion.

## Guardrails

Confirmed by tests:

- activation legal actions remain separate from `FWBAction`
- `WBRules::GenerateLegalActions` output remains unchanged
- `WBActionCodec` remains unchanged
- activation source gates, including fixture source-zone ownership gates, affect only the candidate set supplied to this family
- activation legal actions preserve `FWBCardActivationUsageCommit` from their source candidates
- activation legal actions preserve `FWBCardActivationCostPaymentCommit` from their source candidates
- activation legal action ids can be selected and replayed through test-only verifier fixtures
- applying activation remains explicit through `WBEffectRunner::ApplyCardActivationCommand`
- generation emits no traces and mutates no state
- hidden deck/hand/discard and debug activation metadata do not leak into trace serialization
- Board-source activation candidates use the same generic legal action conversion path without Board-specific generator code

## Out Of Scope

- production CardDB
- activation costs
- card zones
- response windows
- effect negation
- passives
- wands
- card-specific behavior
- UI target selection
- Blueprints, assets, `.uasset`, or `.umap` work

## Validation

Build:

```text
Result: Succeeded
Total execution time: 45.74 seconds
```

Full Wandbound automation:

```text
succeeded=545
succeededWithWarnings=0
failed=0
notRun=0
```
