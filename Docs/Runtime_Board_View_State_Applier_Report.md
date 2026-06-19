# Runtime Board View State Applier Report

## Scope

This pass adds a read-only visual state applier component for runtime board view updates.

Added:

- `UWBBoardViewStateApplierComponent`
- state applier automation tests

Not implemented:

- gameplay rules
- legal action generation
- player input
- tile picking
- unit selection
- action menus
- hand UI
- response UI
- card UI
- camera logic
- animations
- VFX
- sound
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- CardDB import
- marker visuals
- hidden marker identity

## Component Purpose

`UWBBoardViewStateApplierComponent` stores the latest public board summary and applies it to an assigned `AWBBoardViewActor`.

The component stores:

- `FWBPublicBoardSummary`
- `bHasLatestPublicBoardSummary`
- `FWBBoardViewRefreshResult`

It does not store `FWBGameStateData`.

## Update Flow

Runtime/controller code can call:

- `SetLatestPublicBoardSummary`
- `SetLatestPublicBoardSummaryFromState`
- `ApplyLatestPublicBoardSummary`

`SetLatestPublicBoardSummaryFromState` accepts a const state reference only to delegate through `WBBoardSummaryBridge::BuildPublicSummaryFromState`.

`ApplyLatestPublicBoardSummary` calls `WBBoardSummaryBridge::RenderSummaryToBoardView` and stores the last refresh result.

## Public Summary Boundary

The component caches public summary data only. Deck, hand, discard, pending hidden choices, and marker identity remain outside the component.

## No Gameplay Mutation

The component does not mutate game state, generate legal actions, own gameplay truth, implement input, or drive selection. It is a visual state cache and applier only.

## Future Usage

After a rules action resolves, future runtime/controller code can build or receive a `FWBPublicBoardSummary` and set it on this component. The component can then refresh the board actor immediately or wait for an explicit apply call.
