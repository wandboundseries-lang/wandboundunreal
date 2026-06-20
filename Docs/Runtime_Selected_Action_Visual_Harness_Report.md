# Runtime Selected Action Visual Harness Report

## Scope

This pass adds a C++ selected-action-to-visual-refresh harness.

Added:

- `WBSelectedActionVisualHarness`
- `FWBSelectedActionVisualHarnessOptions`
- `FWBSelectedActionVisualHarnessResult`
- selected-action visual harness automation tests

Not added:

- gameplay rules
- legal action generation
- input
- tile picking
- selection
- UI
- camera logic
- animation
- VFX
- sound
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- marker visuals
- hidden marker identity

## Harness Purpose

`WBSelectedActionVisualHarness` is a pure C++ runtime helper for the future runtime flow where an external system has already selected an action.

It accepts:

- mutable external `FWBGameStateData&`
- externally supplied `FWBAction`
- externally supplied `FWBRuntimeTurnResolutionContext`
- optional `UWBRuntimeVisualControllerComponent`
- refresh options

It does not store state, own gameplay truth, or generate legal actions.

## Execution-To-Refresh Path

The harness follows this path:

1. external selected action
2. `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`
3. `FWBRuntimeSelectedActionResult`
4. `UWBRuntimeVisualControllerComponent::ApplyRuntimeSelectedActionResult`
5. `UWBBoardViewStateApplierComponent`
6. `AWBBoardViewActor`

The visual refresh path consumes only `FinalPublicBoardSummary`.

## Refresh Options

`FWBSelectedActionVisualHarnessOptions` controls refresh policy:

- `bRefreshVisualsOnSuccess = true`
- `bRefreshVisualsOnFailure = false`

By default, successful selected actions refresh visuals and failed selected actions do not. Callers may explicitly opt into failure refreshes to display an unchanged public board state.

## Boundaries

The harness does not:

- own or cache `FWBGameStateData`
- inspect deck, hand, discard, hidden marker identity, or pending hidden choices
- call `WBRules::GenerateLegalActions`
- decide action legality
- add traces
- implement input or UI
- add camera, animation, VFX, sound, assets, Blueprints, UMG, `.uasset`, or `.umap` work

## Future Usage

Future runtime controller code can receive a selected legal action from input/UI, call this harness, and allow the harness to execute through the existing core adapter. The resulting public board summary then refreshes the visual controller and board view actor.
