# Card Activation Cost Payment Replay Verifier Audit

## Scope

This audit was completed before adding replay/verifier coverage. It inspected current Unreal cost-payment, replay trace, and fixture utility behavior only. Godot reference files remain read-only and were not modified, imported, compiled, or runtime-loaded.

## Current Cost-Payment Behavior

`FWBCardActivationCommand` carries optional `FWBCardActivationCostPaymentCommit` metadata. When `bPayCostOnSuccess` is true, `WBRules::CanApplyCardActivationCommand` validates payment through `WBCardActivationCostPayment::CanPayCost`.

`WBEffectRunner::ApplyCardActivationCommand` pays on a working copy before effect resolution, applies the effect request, marks usage if requested, and commits the working state only after successful activation. Failed payment, failed effect validation, and failed effect resolution leave original `RLUsed` and usage state unchanged.

Payment mutates only source unit `RLUsed`. Overflow, wand destruction, equipped-wand fallout, production zones, CardDB import, response windows, negation, passives, and card-specific behavior are not implemented.

## Existing Fixture Operations

`WBReplayFixtureTestUtils` already supports:

- `apply_card_activation_command`
- `build_card_activation_command`
- `build_and_apply_card_activation_command`
- `generate_card_activation_candidates`
- `generate_card_activation_legal_actions`

Direct activation command fixtures already parse optional `cost_payment_commit`. Legal action fixtures already validate activation action count, ids, and labels.

## Existing Trace Fields

Replay traces currently serialize:

- `card_activation_resolved`
- `card_activation_cost_paid`
- `effect_request_resolved`
- child effect traces such as `heal_effect_resolved` and `damage_effect_resolved`
- `card_activation_usage_marked`

`card_activation_cost_paid` carries public-safe payment fields:

- player id
- source unit id
- cost amount
- previous and new `RLUsed`
- available RL before and after payment
- cost kind

Trace serialization does not include source card id, source effect id, debug activation id, deck, hand, discard, private choices, or usage keys.

## Existing State Fields

Fixture state and final-unit expectations already support `rl_total` and `rl_used`. Available RL is not stored on units; it is derived as:

```text
max(0, RLTotal - RLUsed)
```

## Hidden-Data Policy

Existing cost-payment tests check serialized trace JSON for hidden source metadata. Public board summaries expose public unit/card/combat/status fields only; payment internals and usage keys are not public summary fields.

## Missing Verifier Coverage

Before this pass, coverage proved cost payment behavior directly, but did not provide reusable verifier helpers for:

- final `RLUsed` plus derived available RL parity.
- stable `card_activation_cost_paid` trace field parity.
- missing cost-paid trace failures.
- fixture-level forbidden trace substring checks.
- legal action family preservation of `CostPaymentCommit` through shared fixture utilities.
- source guards proving verifier helpers stay test-only.

## Audit-Driven Additions

The implementation pass should remain verifier-only and cover:

- final unit `RLUsed` and derived available RL through reusable test helpers.
- exact public `card_activation_cost_paid` trace fields.
- no-payment and rollback failure paths.
- hidden metadata exclusion from serialized trace JSON.
- activation legal action preservation of `FWBCardActivationCostPaymentCommit`.
- explicit confirmation that payment metadata is not added to `FWBAction` or `WBActionCodec`.

## Why This Pass Is Verifier-Only

The payment behavior already exists and passed automation. This pass adds deterministic fixture and helper coverage around that behavior without changing the gameplay rules, effect runner semantics, action codec, or legal action generation boundary.

## Deferred Work

- overflow and wand fallout
- production CardDB import
- real card zones
- response windows
- effect negation
- passives and wands
- card-specific costs and replacement effects
- UI/runtime presentation
