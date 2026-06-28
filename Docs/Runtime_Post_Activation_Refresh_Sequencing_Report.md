# Runtime Post-Activation Refresh Sequencing Report

## Purpose

This pass adds `WBRuntimePostActivationRefreshSequencer`, a pure C++ runtime helper for the common post-activation flow.

It executes a selected activation id through the existing owner activation execution path, then refreshes normal decision-point presentation and activation presentation from externally supplied post-action data.

## Flow

`ExecuteActivationAndRefreshFromExternalData` takes:

- mutable external `FWBGameStateData&`
- selected activation action id
- `UWBRuntimeDecisionPointOwnerComponent*`
- externally supplied normal `FWBAction` legal actions
- externally supplied `FWBCardActivationLegalActionSet`
- externally supplied `FWBPublicBoardSummary`
- refresh options

The sequence is:

1. call `Owner->ExecuteActivationActionId`
2. record `FWBRuntimeActivationExecutionResult`
3. if configured after success, call `Owner->RefreshDecisionPointFromExternalData`
4. if configured after success, call `Owner->RefreshActivationPresentationFromExternalData`
5. report execution and refresh status without generating any rules data

## Result Fields

`FWBRuntimePostActivationRefreshResult` reports:

- `bOk`
- reason code
- whether activation execution was attempted
- whether activation execution succeeded
- whether any post-action refresh was attempted
- whether normal decision-point refresh succeeded
- whether activation presentation refresh succeeded
- activation execution result
- normal decision refresh result
- activation presentation refresh result

## Refresh On Success

By default, successful activation execution refreshes both:

- normal decision-point presentation from supplied `PostActionPrecomputedLegalActions`
- activation presentation from supplied `PostActionActivationActionSet`

Both refreshes use the same supplied `PostActionPublicBoardSummary`.

`bOk` is true only when activation execution succeeds and every enabled success refresh succeeds.

If both success refresh options are disabled, activation execution can still return `bOk = true`; no post-action runtime presentation is refreshed.

## Refresh On Failure

By default, failed activation execution does not refresh presentation.

Failure refresh can be enabled independently for normal decision-point presentation and activation presentation. Even when failure refresh succeeds, the overall sequencer result remains failed because activation execution failed.

## Stale-State Policy

The sequencer never computes fresh post-action data.

If the caller supplies stale post-action legal actions, stale activation legal actions, or a stale public board summary, the runtime presentation reflects that supplied stale data. This is intentional because runtime is not the rules owner.

Tests verify presentation counts come from supplied post-action data, not from the mutated `FWBGameStateData`.

`FWBActivationSessionTestHarness` now adds higher-level session coverage over the facade and sequencer together. It verifies execute plus external post-action refresh across consecutive decisions, and it proves once-per-turn and RR-cost changes are visible only after externally rebuilt post-action data is supplied.

## External-Data Ownership Policy

Runtime callers are responsible for producing the post-action normal legal actions, activation legal action set, and public board summary through the appropriate rules owner.

The sequencer does not call:

- `WBRules::GenerateLegalActions`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBActionCodec`
- `WBEffectRunner` directly

## Hidden-Information Policy

The sequencer accepts public/runtime-safe input surfaces and does not inspect deck, hand, discard, equipped zones, fixture zone context, hidden marker identity, or private card data.

Tests cover hidden source effect ids, usage keys, debug activation ids, and cost metadata staying out of public activation presentation and activation traces.

## Owner Convenience

`UWBRuntimeDecisionPointOwnerComponent::ExecuteActivationActionIdAndRefreshFromExternalData` delegates to the sequencer.

The owner still stores the activation execution result through its existing activation execution path. Normal refresh status is stored through existing owner refresh storage. Activation refresh updates the existing activation status counts.

The later runtime activation decision-session facade wraps this sequencer path for future UI integration. The facade still receives all normal legal actions, activation legal actions, and public summaries externally.

The production activation execution handoff adapter now performs a provider-specific post-action refresh after successful execution by creating a fresh `FWBProductionActivationDataProvider` from updated state, repository, and viewer id. This is a narrow provider refresh path and does not change the generic external-data sequencer contract.

## Separation From FWBAction

Activation legal actions remain a separate action family.

No activation action type was added to `EWBActionType`, no activation ids were added to `WBActionCodec`, and normal `WBRules::GenerateLegalActions` output remains unchanged.

## Out Of Scope

- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
- overflow destruction
- wand destruction or redirect
- card-specific prevention or replacement
- death triggers
- counters
- random dice
- NPC phase
- visual animation
- VFX
- sound
- Blueprints, `.uasset`, or `.umap` changes
