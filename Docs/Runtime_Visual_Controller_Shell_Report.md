# Runtime Visual Controller Shell Report

## Scope

This pass adds a read-only runtime visual controller shell for public board refresh coordination.

Added:

- `UWBRuntimeVisualControllerComponent`
- visual controller shell automation tests

Not added:

- optional actor shell
- gameplay rules
- legal action generation
- player input
- tile picking
- unit selection
- UI
- camera logic
- animations
- VFX
- sound
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- marker visuals
- hidden marker identity

## Component Purpose

`UWBRuntimeVisualControllerComponent` is a C++-only visual coordination component. It owns no rules truth and stores only:

- a `UWBBoardViewStateApplierComponent` reference
- `bAutoApplyPublicBoardSummaries`
- the last `FWBBoardViewRefreshResult`
- whether a public board summary has been received

The component does not store, mutate, or expose `FWBGameStateData`.

## Public Summary Input

`ApplyPublicBoardSummary` accepts `const FWBPublicBoardSummary&`, records receipt, forwards the summary to the assigned state applier, and stores the latest refresh result.

If no state applier is assigned, the component returns a default `FWBBoardViewRefreshResult` with `bRendered=false` and does not crash.

If the state applier is set to manual apply and `bAutoApplyPublicBoardSummaries` is true, the controller explicitly calls `ApplyLatestPublicBoardSummary`.

## Runtime Selected-Action Result Input

`ApplyRuntimeSelectedActionResult` reads only `FWBRuntimeSelectedActionResult::FinalPublicBoardSummary` and then delegates to `ApplyPublicBoardSummary`.

It does not inspect hidden state, trace details, selected action data, legal actions, decks, hands, discards, marker identity, or rules state.

## Hidden-Information Boundary

The component consumes public board summaries only. Hidden deck, hand, discard, marker identity, pending choices, and private replay details stay outside runtime rendering.

## Future Usage

After selected-action resolution produces a `FWBRuntimeSelectedActionResult`, future runtime controller code can pass that result to the visual controller shell. The shell forwards the final public board summary to the state applier, and the state applier refreshes the board view actor's placeholder tile, unit, wall, and terrain instances.

`WBSelectedActionVisualHarness` can now feed runtime selected-action results into this visual controller shell. The harness still executes only externally supplied selected actions through the existing runtime adapter and refreshes this component from the result's public board summary.

`UWBRuntimeControllerFacadeComponent` now coordinates externally selected action execution into this visual refresh path. The facade delegates to `WBSelectedActionVisualHarness`, then exposes the retained runtime selected-action result and board refresh result.
