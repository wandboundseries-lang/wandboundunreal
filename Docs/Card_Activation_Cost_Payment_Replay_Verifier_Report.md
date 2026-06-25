# Card Activation Cost Payment Replay Verifier Report

## Scope

This pass adds verifier-only coverage for fixture-owned card activation cost payment. It does not change gameplay semantics, card activation application, legal action generation, `FWBAction`, `WBActionCodec`, runtime code, UI, Blueprints, `.uasset`, or `.umap` files.

## Audit Summary

The audit confirmed that existing cost-payment behavior already pays through `FWBCardActivationCostPaymentCommit`, validates through `WBRules::CanApplyCardActivationCommand`, mutates `RLUsed` only on a working copy in `WBEffectRunner::ApplyCardActivationCommand`, and commits only after full activation success.

`card_activation_cost_paid` traces already serialize public-safe payment fields only:

- player id
- source unit id
- cost amount
- previous and new `RLUsed`
- available RL before and after payment
- cost kind

Hidden source card id, source effect id, debug activation id, private zones, private choices, and usage keys are not serialized into replay traces.

## Test-Only Verifier Helpers

Added test-only helpers under `WandboundTests`:

- `FWBExpectedActivationCostPaymentState`
- `FWBCardActivationCostPaymentVerifier::VerifyUnitRLState`
- `FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace`
- `FWBCardActivationCostPaymentVerifier::ContainsTraceKind`
- `FWBCardActivationCostPaymentVerifier::TraceJsonExcludesForbiddenSubstrings`

The helpers live in `Source/WandboundTests/Private/` only. Source-guard tests assert `WandboundCore` and `WandboundRuntime` do not reference the verifier.

## Fixture Expected Checks

`WBReplayFixtureTestUtils` now supports optional expected fields:

- `expected.rl_state`
- `expected.cost_paid_trace`
- `expected.forbidden_trace_substrings`
- `expected.cost_payment_commit` for card activation legal action generation

These checks are opt-in. Existing fixtures without these fields continue to pass.

The checks are wired for:

- `apply_card_activation_command`
- `build_and_apply_card_activation_command`
- `generate_card_activation_legal_actions`

## Golden Scenarios

Added replay verifier fixtures:

- `card_activation_replay_cost_paid_trace_parity.json`
- `card_activation_replay_final_rl_used_parity.json`
- `card_activation_replay_payment_not_paid_on_failure.json`
- `card_activation_replay_payment_rollback_on_effect_failure.json`
- `card_activation_replay_payment_and_usage_success_order.json`
- `card_activation_replay_payment_metadata_hidden.json`
- `card_activation_replay_legal_action_preserves_payment_commit.json`
- `card_activation_replay_payment_not_in_fwbaction_codec.json`

## Confirmations

- Cost trace parity: fixture checks assert the emitted `card_activation_cost_paid` trace fields.
- Final RL parity: fixture checks assert final `RLUsed` and derived available RL.
- Failure rollback: failed validation/effect scenarios leave `RLUsed` unchanged and emit no payment trace.
- Usage order: successful payment traces precede effect traces, and usage marking follows effect success.
- Hidden metadata: trace JSON excludes hidden source card/effect/debug/usage-key strings.
- Legal generation: activation legal actions preserve payment commits without paying costs.
- Selected activation action ids can now drive replay verification through the activation legal action family.
- Codec boundary: payment remains outside `FWBAction` and `WBActionCodec`; core legal action ids still do not include activation ids.

## Out Of Scope

- overflow and wand fallout
- production CardDB import
- real card zones
- response windows
- effect negation
- passives and wands
- card-specific costs and replacement effects
- activation in `FWBAction` or `WBActionCodec`
- UI/runtime presentation

## Validation

Build:

```text
Result: Succeeded
Total execution time: 12.27 seconds
```

Wandbound automation:

```text
succeeded=575
succeededWithWarnings=0
failed=0
notRun=0
```

An initial build exposed existing anonymous helper name collisions in `WBReplayTrace.cpp` under Unity compilation. The replay-trace private helpers were renamed locally; the final build and automation passed.
