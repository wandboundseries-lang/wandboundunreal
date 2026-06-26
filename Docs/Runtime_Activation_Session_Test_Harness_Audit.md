# Runtime Activation Session Test Harness Audit

## Scope

This pass adds test-only automation support for activation decision sessions. It does not add production runtime legal generation, activation candidate generation, activation legal action generation, public summary construction, UI, target picking, card database loading, zones, response windows, passives, wands, or visual effects.

## Existing Activation Decision-Session Facade

`UWBRuntimeActivationDecisionSessionFacadeComponent` is a coordinator-facing shell. It accepts externally supplied normal `FWBAction` legal actions, an externally supplied `FWBCardActivationLegalActionSet`, and an externally supplied `FWBPublicBoardSummary`.

The facade:
- refreshes the normal decision point through `UWBRuntimeDecisionPointOwnerComponent`
- refreshes activation presentation through `UWBRuntimeDecisionPointOwnerComponent`
- resolves selected activation action ids through the owner/coordinator path
- creates execution handoffs through the owner/coordinator path
- executes activation action ids through the owner/sequencer path
- post-refreshes only from externally supplied post-action data

The facade does not own or cache `FWBGameStateData`, does not generate legality, does not generate activation actions, and does not build public summaries from state.

## Existing Post-Activation Refresh Sequencer

`WBRuntimePostActivationRefreshSequencer` executes a selected activation id through the decision-point owner and then conditionally refreshes normal and activation presentation from supplied post-action data.

The sequencer supports:
- success refresh of normal and activation presentation
- success refresh disablement, leaving presentation stale
- failure refresh disablement by default
- opt-in failure refresh for externally supplied recovery data

The sequencer does not generate replacement legal actions, activation action sets, or public summaries.

## Existing Test-Only Decision Loop Harness

`FWBDecisionLoopTestHarness` already lives under `Source/WandboundTests/Private`. It simulates an external rules/runtime owner by calling `WBRules::GenerateLegalActionsForPlayer` and `WBPublicBoardSummary::Build`, then feeds that data into runtime presentation/coordinator components.

That pattern is test-only and is not included from `Source/WandboundRuntime`.

## Test-Only External Owner Simulation

The activation session harness may use the following inside `WandboundTests` only:
- `WBRules::GenerateLegalActionsForPlayer`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBPublicBoardSummary::Build`

This is allowed because the helper is proving that runtime consumes external data correctly. It is not a production data source.

## Production Runtime Boundary

Production runtime must continue receiving precomputed data from an external rules owner. It must not generate legal actions, activation candidates, activation legal action sets, or public summaries from `FWBGameStateData`.

The runtime boundary remains:
- rules validate legality
- `WBEffectRunner` mutates state
- runtime/UI select and display only

Activation legal actions remain a separate action family and must not be merged into `FWBAction`, `EWBActionType`, or `WBActionCodec`.

## Hidden Information Boundary

Public presentation, trace output, serialized logs, and docs must not expose hidden card identities or internal activation metadata such as source effect ids, usage keys, debug activation ids, or cost kind internals. Commands may retain metadata internally for execution, but public snapshots and traces must stay sanitized.

## Deferred

- production CardDB import
- production zones
- UI widgets
- target picking
- response windows
- negation
- passives and wands
- visual animation and VFX
