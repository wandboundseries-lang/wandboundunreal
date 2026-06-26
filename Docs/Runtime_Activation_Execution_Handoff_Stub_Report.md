# Runtime Activation Execution Handoff Stub Report

## Purpose

This pass adds a runtime activation execution handoff stub. It accepts a resolved activation selection result and produces a structured result that preserves future execution data while clearly reporting that activation execution is not implemented yet.

This is not activation execution.

## Input

`WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff` takes:

- `FWBRuntimeActivationSelectionResolution`

The selection result is expected to come from the runtime activation selection resolver or one of the model/coordinator/owner convenience methods.

## Output

`FWBRuntimeActivationExecutionHandoffResult` returns:

- `bHandoffOk`
- `bSelectionResolved`
- `bExecutionImplemented`
- `bExecutionAttempted`
- reason code
- selected activation action id
- internal `FWBCardActivationLegalAction`
- optional public activation presentation entry
- optional public target presentation entry

## Success And Failure Behavior

If the input selection is unresolved:

- `bHandoffOk = false`
- `bSelectionResolved = false`
- `bExecutionImplemented = false`
- `bExecutionAttempted = false`
- `Reason` is the selection reason, or `activation_selection_not_resolved` when the selection reason is empty
- the selected activation action id is retained

If the input selection is resolved:

- `bHandoffOk = true`
- `bSelectionResolved = true`
- `bExecutionImplemented = false`
- `bExecutionAttempted = false`
- `Reason = activation_execution_not_implemented`
- the internal activation action is copied
- public activation/target presentation entries are copied when available

## Flag Semantics

`bHandoffOk` means the selected activation id resolved cleanly into a handoff payload.

`bExecutionImplemented` is always false in this pass.

`bExecutionAttempted` is always false in this pass. The stub does not run activation commands, emit execution traces, pay costs, mark usage, or mutate rules state.

## Convenience Methods

Added:

- `UWBRuntimeActivationPresentationModelComponent::CreateExecutionHandoffForActivationActionId`
- `UWBRuntimeDecisionPointCoordinatorComponent::CreateActivationExecutionHandoff`
- `UWBRuntimeDecisionPointOwnerComponent::CreateActivationExecutionHandoff`

The model resolves the selected activation action id and passes the resolution into the handoff stub.

The coordinator fails with `activation_presentation_model_missing` when no activation presentation model exists.

The owner fails with `decision_point_coordinator_missing` when no coordinator exists.

## Non-Execution Boundary

The handoff stub does not:

- execute activation commands
- call `WBEffectRunner`
- call `WBEffectRunner::ApplyCardActivationCommand`
- inspect `FWBGameStateData`
- mutate rules state
- generate activation candidates
- generate activation legal actions
- require a normal `FWBAction` decision point

## Hidden-Information Policy

The returned `ActivationAction.Command` is internal runtime data and may retain command metadata required for future execution wiring.

Public presentation entries and reason strings must not expose hidden metadata such as:

- `SourceEffectId`
- `UsageKey`
- `DebugActivationId`
- `CostKind`
- fixture zone context
- hidden hand/deck/discard data

Tests verify hidden command metadata remains absent from public presentation text and reason strings.

## Separation From FWBAction

Activation remains separate from:

- `FWBAction`
- `EWBActionType`
- `WBActionCodec`
- `WBRules::GenerateLegalActions`

No activation action IDs were added to `WBActionCodec`, and normal legal action generation remains unchanged.

## Out Of Scope

- activation execution integration
- production zones
- CardDB import
- activation `FWBAction`
- activation codec IDs
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
