# Runtime Activation Execution Integration Audit

## Context

This pass starts after runtime activation presentation storage, selected activation id resolution, and the not-implemented activation execution handoff stub. Runtime can now store externally supplied activation legal actions, resolve a selected activation action id, and produce a handoff result that preserves the internal activation action and public presentation entries.

## Current Handoff Stub Behavior

`WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff` accepts an `FWBRuntimeActivationSelectionResolution`.

When selection fails, it preserves the selected activation id and selection failure reason without attempting execution.

When selection succeeds, it copies:

- selected activation action id
- internal `FWBCardActivationLegalAction`
- public activation presentation entry when present
- public target presentation entry when present

It still returns `bExecutionImplemented = false`, `bExecutionAttempted = false`, and `Reason = activation_execution_not_implemented`. The handoff remains a selection-to-payload step, not an execution step.

## Current Selection Resolver Behavior

`WBRuntimeActivationSelectionResolver` resolves selected activation action ids from the activation presentation model's externally supplied `FWBCardActivationLegalActionSet`.

It fails on:

- missing activation presentation model
- no current activation presentation state
- missing selected activation id
- duplicate selected activation id

On success it returns the matching internal activation legal action plus public activation and target presentation entries. It does not generate candidates, generate legal actions, inspect rules state, or execute commands.

## Current Core ApplyCardActivationCommand Behavior

`WBEffectRunner::ApplyCardActivationCommand` is the existing deterministic core execution path for activation commands. It:

- calls `WBRules::CanApplyCardActivationCommand`
- fills missing effect request source fields from the command source
- applies cost payment on a working copy
- applies the effect request on that working copy
- validates and marks usage on that working copy
- assigns the working copy back to the caller's state only after success
- returns `FWBCardActivationCommandResult`

The returned command result includes the filled command, effect result, and trace events.

## Cost Payment Behavior

Cost payment is represented by `FWBCardActivationCostPaymentCommit`.

When `bPayCostOnSuccess` is true, the core path builds an `FWBCardActivationCostPaymentRequest` and calls `WBCardActivationCostPayment::PayCost` on the working copy. This validates player id, source unit, source ownership, nonnegative RR, available RL, and removed/defeated state.

Successful payment increments `RLUsed` and emits a `card_activation_cost_paid` trace after final success. The trace includes public numeric payment fields and `CostKind`; it does not include hidden usage keys, source effect ids, debug activation ids, hidden zones, or CardDB data.

## Usage Marking Behavior

Usage marking is represented by `FWBCardActivationUsageCommit`.

When `bMarkUsageOnSuccess` is true, the core path validates player id and nonempty usage key, rejects already-used keys, and marks the usage key on the working copy only after the effect request succeeds. The `card_activation_usage_marked` trace records player id only and intentionally omits the usage key.

## Trace Behavior

Successful activation execution emits:

- `card_activation_resolved`
- optional `card_activation_cost_paid`
- effect request and child effect traces
- optional `card_activation_usage_marked`

Core activation traces intentionally avoid hidden source effect ids, debug activation ids, usage keys, fixture zones, hand/deck/discard contents, and CardDB/private card data.

## Rollback Behavior

`ApplyCardActivationCommand` mutates a copied `FWBGameStateData` and commits the copy back only after cost payment, effect application, and usage marking all succeed. Failed payment, failed effect requests, invalid usage commits, or already-used usage keys leave the caller's original state unchanged.

## Why Add Runtime Execution Integration

The runtime path now has a resolved handoff payload but no explicit execution step. This pass adds a narrow runtime bridge so selected activation ids can flow through:

1. runtime presentation model
2. selection resolver
3. handoff result
4. runtime execution bridge
5. `WBEffectRunner::ApplyCardActivationCommand`

The bridge gives future runtime/UI code a clear, tested execution boundary without merging activations into normal `FWBAction` decisions.

## Why Delegate To EffectRunner

Rules validation, cost payment, effect mutation, usage marking, traces, and rollback already live in the core rules/effects layer. Duplicating that logic in runtime would risk drift from deterministic rules behavior. The runtime bridge should only verify the handoff shape and delegate to `WBEffectRunner::ApplyCardActivationCommand`.

## FWBAction And Codec Boundary

Activation legal actions remain a separate action family. This pass must not add activation to `EWBActionType`, modify `WBActionCodec`, or modify `WBRules::GenerateLegalActions`.

Runtime activation execution consumes an already-resolved activation legal action and command. It does not generate normal legal actions or activation legal actions.

## Hidden-Information Boundary

Internal `FWBCardActivationCommand` data may contain metadata needed by core execution. That data is not public presentation data.

Runtime execution must not expose hidden metadata through public presentation entries, reason strings, serialized traces, logs, or summaries. Tests must continue to verify that trace JSON excludes source effect ids, usage keys, debug activation ids, fixture zones, and hidden hand/deck/discard data.

## Deferred Work

- production card zones
- CardDB import
- UI widgets
- real target picking
- response windows
- effect negation
- passives and wands
- overflow handling
- wand destruction/fallout
