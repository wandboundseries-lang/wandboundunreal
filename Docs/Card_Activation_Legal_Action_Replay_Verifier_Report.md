# Card Activation Legal Action Replay Verifier Report

## Purpose

This pass adds verifier-only coverage for selecting activation legal action ids from an externally generated activation legal action family, then applying the selected activation command through the existing EffectRunner path.

No gameplay behavior was added. Activation remains separate from `FWBAction`, and `WBActionCodec` remains unchanged.

## External Decision Input

Activation legal action ids are treated as external decision inputs. The selected id is resolved only against the supplied `FWBCardActivationLegalActionSet`.

Selection behavior:

- no matching id fails with `activation_action_id_not_found`.
- multiple matching ids fail with `activation_action_id_ambiguous`.
- exactly one matching id copies the selected legal action.

Failed selection does not mutate state and does not call `WBEffectRunner`.

## Apply-Selected-ID Flow

The test-only verifier helper:

1. Resolves the selected activation action id.
2. Fails before mutation if the id is missing or ambiguous.
3. Calls `WBEffectRunner::ApplyCardActivationCommand` only after a unique selection.
4. Returns the existing `FWBCardActivationCommandResult`.

All state mutation still happens through the existing EffectRunner command path.

## Fixture Operation

Added fixture operation:

```json
"operation": "apply_card_activation_legal_action_by_id"
```

The fixture operation:

- parses `activation_sources`.
- generates candidates through `WBCardActivationCandidateGenerator`.
- generates activation legal actions through `WBCardActivationLegalActionGenerator`.
- validates expected activation action ids, labels, and payment commit metadata.
- resolves `selected_activation_action_id`.
- applies the selected command only after unique resolution.
- reuses existing RL state, cost-paid trace, forbidden trace substring, trace order, and final unit checks.

## Coverage

Added coverage for:

- successful selected activation action id replay.
- missing selected id failure without mutation.
- ambiguous selected id failure without mutation.
- cost payment through selected activation action ids.
- final `RLUsed` and derived Available RL parity.
- `card_activation_cost_paid` trace parity.
- payment plus usage trace ordering.
- payment failure without mutation.
- effect failure rollback behavior.
- hidden metadata exclusion from serialized trace JSON.
- activation ids remaining outside `FWBAction` and `WBActionCodec`.
- production core/runtime source guard for the test-only replay verifier.

## Confirmations

- `WBRules::GenerateLegalActions` output remains unchanged.
- `WBActionCodec` output remains unchanged.
- Activation ids remain external activation-family ids.
- The replay verifier helper lives under `WandboundTests` only.
- No Godot CardDB import, production zones, response windows, negation, passives, wands, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Out Of Scope

- overflow
- equipped wand fallout
- production CardDB
- real hand/deck/discard/equipped zones
- response windows
- effect negation
- passives and wands
- card-specific costs and replacement effects
- UI target selection

## Validation

Build:

```text
Result: Succeeded
Total execution time: 31.52 seconds
```

Wandbound automation:

```text
succeeded=584
succeededWithWarnings=0
failed=0
notRun=0
```
