# Runtime Activation Execution Integration Report

## Purpose

This pass adds the first narrow runtime activation execution bridge.

Runtime can now take a selected activation action id that was previously stored and resolved through activation presentation, create the existing execution handoff payload, and explicitly execute that payload through the core rules/effects path.

## Execution Bridge

Added `WBRuntimeActivationExecutionBridge`.

`ExecuteResolvedActivationHandoff` accepts:

- mutable external `FWBGameStateData&`
- `FWBRuntimeActivationExecutionHandoffResult`

It returns `FWBRuntimeActivationExecutionResult` with:

- `bOk`
- `bExecutionAttempted`
- `bActivationResolved`
- reason code
- selected activation action id
- copied handoff payload
- core `FWBCardActivationCommandResult`

The bridge fails without mutation when the handoff is unresolved, selection is unresolved, the activation id is missing, or the command payload is structurally missing.

On a valid handoff, it delegates once to `WBEffectRunner::ApplyCardActivationCommand`.

## Convenience Methods

Added execution convenience methods:

- `UWBRuntimeActivationPresentationModelComponent::ExecuteActivationActionId`
- `UWBRuntimeDecisionPointCoordinatorComponent::ExecuteActivationActionId`
- `UWBRuntimeDecisionPointOwnerComponent::ExecuteActivationActionId`

The model creates a handoff from its externally supplied activation presentation state and passes it to the bridge.

The coordinator fails with `activation_presentation_model_missing` if no activation model is assigned.

The owner fails with `decision_point_coordinator_missing` if no coordinator is assigned, otherwise it delegates to the coordinator and stores the latest activation execution result.

## Cost, Usage, Rollback, And Traces

Cost payment, effect mutation, usage marking, trace emission, and rollback remain owned by `WandboundCore`.

The runtime bridge does not duplicate those rules. It relies on `WBEffectRunner::ApplyCardActivationCommand`, which applies activation commands on a working copy and commits only after cost payment, effect resolution, and usage marking all succeed.

Successful activation execution can emit:

- `card_activation_resolved`
- optional `card_activation_cost_paid`
- effect request and child effect traces
- optional `card_activation_usage_marked`

Failed payment, failed effects, or invalid usage commits leave the original state unchanged.

## Stale Refresh Policy

Activation execution does not regenerate activation legal actions, normal legal actions, or public summaries.

The runtime presentation model, coordinator, and owner keep their existing stale-state policy: future callers must refresh activation presentation and decision-point presentation from externally supplied post-action data.

No visual refresh is triggered by activation execution in this pass.

The later runtime post-activation refresh sequencing pass adds an explicit external-data refresh helper that can run after successful activation execution. It refreshes normal decision-point presentation and activation presentation only from caller-supplied post-action legal actions, activation legal actions, and public summaries.

The runtime activation decision-session facade now provides a higher-level external-data session flow over refresh, selection, handoff, execution, and post-action refresh. It still delegates execution through the existing owner/sequencer path and does not call `WBEffectRunner` directly.

The production activation execution handoff adapter now uses this existing execution integration rather than adding a parallel effect path. It rebuilds a provider-selected activation command from the repository, creates a runtime handoff payload, and calls `WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff`.

## Hidden-Information Policy

The bridge carries internal activation command data only as execution input and result data. Public presentation remains the safe display surface.

Tests verify traces and public text exclude hidden metadata such as source effect ids, usage keys, debug activation ids, fixture zones, and hidden hand/deck/discard data.

## Separation From FWBAction

Activation execution remains separate from:

- `FWBAction`
- `EWBActionType`
- `WBActionCodec`
- `WBRules::GenerateLegalActions`
- normal legal action presentation

No activation action type or codec ids were added.

## Fixtures

No new GodotCanon JSON fixtures were added in this pass.

This pass covers transient Unreal runtime components and their explicit delegation to existing core activation command behavior. The cost, usage, rollback, and trace behavior is already covered by core and replay fixture tests.

## Out Of Scope

- production card zones
- CardDB import
- new production handoff effect semantics
- activation `FWBAction`
- activation codec ids
- legal activation generation
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
- camera, animation, VFX, audio
- Blueprints, `.uasset`, or `.umap` changes
