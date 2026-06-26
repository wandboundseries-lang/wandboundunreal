# Runtime Activation Selection Resolver Report

## Purpose

This pass adds read-only runtime activation selection resolution.

Future UI/runtime code can now resolve a selected activation action id from `UWBRuntimeActivationPresentationModelComponent` and receive the matching internal activation legal action plus public presentation entries.

## Input

`WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId` takes:

- `UWBRuntimeActivationPresentationModelComponent*`
- selected activation action id string

The model must already hold an externally supplied `FWBCardActivationLegalActionSet` and public snapshots from a previous activation presentation refresh.

## Output

`FWBRuntimeActivationSelectionResolution` returns:

- `bOk`
- reason code
- selected activation action id
- matching `FWBCardActivationLegalAction`
- optional `FWBCardActivationLegalActionPresentationEntry`
- optional `FWBCardActivationTargetPresentationEntry`

The activation legal action is internal runtime selection data. The presentation entries are the safe public display surface.

## Missing And Ambiguous Policy

Failure reasons are stable reason codes:

- `activation_presentation_model_missing`
- `no_current_activation_presentation_state`
- `activation_action_id_not_found`
- `activation_action_id_ambiguous`
- `decision_point_coordinator_missing` for owner-level missing coordinator

Reason strings do not include source effect ids, usage keys, debug activation ids, fixture zone data, or cost metadata.

## Convenience Methods

Added:

- `UWBRuntimeActivationPresentationModelComponent::ResolveSelectedActivationActionId`
- `UWBRuntimeDecisionPointCoordinatorComponent::ResolveSelectedActivationActionId`
- `UWBRuntimeDecisionPointOwnerComponent::ResolveSelectedActivationActionId`

The coordinator delegate does not require a normal `FWBAction` decision point to exist. The owner delegate only requires a coordinator.

Resolved activation selections can now be converted into an execution handoff result through `WBRuntimeActivationExecutionHandoff`.

The later runtime execution integration pass can execute that handoff through `WBRuntimeActivationExecutionBridge`, while keeping selection resolution itself read-only.

## No Activation Execution Inside Resolver

The resolver does not execute activation commands.

It does not call `WBEffectRunner`, emit traces, consume costs, mark usage, modify HP, modify armor, apply statuses, or mutate rules state.

Execution is an explicit later step through the runtime execution bridge and core `WBEffectRunner::ApplyCardActivationCommand`.

## No Candidate Or Legal-Action Generation

The resolver only scans the activation action set already stored in the runtime activation presentation model.

It does not call:

- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBRules`
- `WBRules::GenerateLegalActions`

## No Game-State Inspection

The resolver does not inspect `FWBGameStateData`, deck, hand, discard, equipped zones, hidden marker identity, or private state.

## Hidden-Information Policy

The returned `FWBCardActivationLegalAction` may retain internal command metadata because it is internal runtime selection data.

Presentation entries must not expose that metadata. Tests cover source effect ids, usage keys, debug activation ids, and cost metadata remaining absent from public activation and target presentation entries.

Public CardIds come only from `FWBPublicBoardSummary` through the existing presentation snapshot builders.

## Separation From FWBAction

Activation selection resolution remains separate from:

- `FWBAction`
- `EWBActionType`
- `WBActionCodec`
- `WBRules::GenerateLegalActions`
- normal `WBRuntimeLegalActionPresentation`

Existing normal action ids remain unchanged.

## Out Of Scope

- activation execution
- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- UI widgets
- target picking
- response windows
- effect negation
- passives
- wands
