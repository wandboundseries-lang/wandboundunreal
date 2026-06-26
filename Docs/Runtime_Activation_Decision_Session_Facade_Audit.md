# Runtime Activation Decision Session Facade Audit

## Current Owner Refresh Behavior

`UWBRuntimeDecisionPointOwnerComponent` already owns the production runtime shell for externally supplied decision-point data.

It can refresh normal decision-point presentation through:

- `RefreshDecisionPointFromExternalData`
- `UWBRuntimeDecisionPointCoordinatorComponent::RefreshDecisionPoint`
- `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`

The refresh path accepts externally supplied normal `FWBAction` legal actions and an externally supplied `FWBPublicBoardSummary`. It updates runtime presentation/status only. It does not generate legal actions, decide legality, inspect hidden zones, or own rules state.

The owner can also refresh activation presentation through `RefreshActivationPresentationFromExternalData`, delegating to the coordinator and `WBRuntimeActivationPresentationRefreshAdapter`.

## Current Activation Presentation Behavior

`UWBRuntimeActivationPresentationModelComponent` stores externally supplied activation legal actions and builds public activation/target snapshots from the supplied public board summary.

It stores:

- `FWBCardActivationLegalActionSet`
- `FWBCardActivationLegalActionPresentationSnapshot`
- `FWBCardActivationTargetPresentationSnapshot`
- `FWBRuntimeActivationPresentationStatus`

It does not generate activation candidates, generate activation legal actions, inspect `FWBGameStateData`, inspect deck/hand/discard/equipped zones, import CardDB data, or perform target picking.

## Current Activation Selection Resolver Behavior

`WBRuntimeActivationSelectionResolver` resolves a selected activation action id against the activation model's current externally supplied action set.

It returns the matching internal `FWBCardActivationLegalAction` plus optional public activation and target presentation entries. It fails deterministically for missing model, missing current state, missing ids, and duplicate ids.

It is read-only and does not mutate rules state, execute commands, or generate data.

## Current Execution Handoff Behavior

`WBRuntimeActivationExecutionHandoff` converts a selection result into an execution handoff payload.

Successful handoffs preserve the internal activation action and public presentation entries. Failed handoffs preserve the selected id and failure reason.

The handoff object is still a selection-to-payload step. It does not execute commands, mutate state, or call `WBEffectRunner`.

## Current Activation Execution Bridge Behavior

`WBRuntimeActivationExecutionBridge` validates a resolved activation handoff and delegates to `WBEffectRunner::ApplyCardActivationCommand`.

Cost payment, usage marking, effect mutation, trace emission, and rollback remain in `WandboundCore`. The bridge does not duplicate those mechanics and does not generate normal or activation legal actions.

## Current Post-Activation Refresh Sequencer Behavior

`WBRuntimePostActivationRefreshSequencer` sequences:

1. owner activation execution
2. optional normal decision-point refresh from externally supplied post-action `FWBAction` legal actions and public summary
3. optional activation presentation refresh from externally supplied post-action activation legal actions and public summary

Refresh-on-success is enabled by default. Refresh-on-failure is opt-in. The sequencer does not compute post-action data, build public summaries from state, generate legal actions, or call `WBEffectRunner` directly.

## Why Add A Facade

Future UI integration needs a small session-facing component that can coordinate the existing runtime pieces without becoming a rules owner.

The facade should provide one place for future UI/controller code to:

- refresh externally supplied normal and activation presentation data
- resolve selected activation ids
- create handoff payloads
- execute selected activation ids through the owner/sequencer path
- expose read-only status

This pass adds that facade instead of adding gameplay behavior, target picking, card database loading, response windows, or UI widgets.

## External Data Requirement

Externally supplied data remains required because runtime is not authoritative for rules.

The facade must consume:

- normal `FWBAction` legal actions
- activation legal action sets
- public board summaries

The facade must not generate those values. If a caller supplies stale data, the facade reflects that stale data by design.

## No Runtime Generation Boundary

The facade must not call:

- `WBRules::GenerateLegalActions`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBActionCodec`
- `WBEffectRunner` directly
- public board summary builders from `FWBGameStateData`

It may mutate an externally supplied `FWBGameStateData&` only by delegating to the existing owner/sequencer activation execution path.

## Activation And FWBAction Boundary

Activation legal actions remain separate from normal `FWBAction`.

The facade must not add activation to `EWBActionType`, change `WBActionCodec`, or merge activation ids into normal legal action generation. Normal action presentation and activation presentation remain separate surfaces.

## Hidden-Information Boundary

The facade should expose only status, reason codes, public presentation data already produced by existing snapshot builders, and internal selection/handoff/execution objects for runtime orchestration.

It must not inspect or expose hidden zones such as deck, hand, discard, equipped card storage, hidden marker identity, fixture zone context, or CardDB-private data.

Source effect ids, usage keys, debug activation ids, cost metadata, raw schema fields, and hook names must not leak into public labels, session status, reason strings, serialized JSON, logs, or docs.

## Deferred Work

- production zones
- CardDB import
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
- overflow handling
- wand destruction or redirect
- visual animation
- VFX
