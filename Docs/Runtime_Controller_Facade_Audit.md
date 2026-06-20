# Runtime Controller Facade Audit

## Selected-Action Visual Harness

`WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals` is the current selected-action bridge. It accepts an external `FWBGameStateData&`, an external `FWBAction`, an external `FWBRuntimeTurnResolutionContext`, an optional `UWBRuntimeVisualControllerComponent`, and refresh options.

The harness applies the selected action through `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`, then refreshes the visual controller only when the runtime result matches the configured success/failure refresh policy. It does not store game state, generate legal actions, decide target legality, or inspect hidden deck/hand/discard data.

## Visual Controller

`UWBRuntimeVisualControllerComponent` consumes public board summaries only. It can accept a `FWBPublicBoardSummary` directly, or a `FWBRuntimeSelectedActionResult` only to read `FinalPublicBoardSummary`. It forwards that public summary to `UWBBoardViewStateApplierComponent`, which applies it to `AWBBoardViewActor` and returns public render counts.

This path does not own or mutate rules state. It does not inspect selected-action internals, trace internals, decks, hands, discards, hidden markers, or private choices.

## Runtime Adapter

`WBRuntimeTurnResolutionAdapter` is the current runtime-facing core adapter. It applies an externally selected action and returns `FWBRuntimeSelectedActionResult`, including:

- selected action type
- selected action id
- apply result and trace events
- consumed MP roll metadata for full end-turn transitions
- final public turn summary
- final public board summary

Move and pass-style actions route through the selected-action executor. EndTurn can either preserve basic player action behavior or, when explicitly configured, resolve the deterministic full turn transition with an external MP roll source.

## Why Add A Facade

Future input/UI/controller code needs a stable C++ runtime surface that accepts an already-selected action and delegates to the selected-action visual harness. The facade adds that surface without giving visuals or actors ownership over rules truth.

The facade accepts already-validated selected actions because legality belongs to `WBRules` and action generation, not to runtime visuals. Future UI should choose from a legal action list produced elsewhere, then pass the chosen `FWBAction` to the facade.

## Boundary Decisions

The facade must not generate legal actions because that would blur the canonical boundary:

- Rules validate legality.
- EffectRunner mutates state.
- UI selects only.
- Runtime visuals display public outcomes only.

The facade must not own `FWBGameStateData` because rules state remains external deterministic data. It receives mutable state by reference only long enough to delegate execution.

## Hidden Information Boundary

The retained `FWBRuntimeSelectedActionResult` is acceptable runtime output because it contains public summaries and trace/result metadata, not raw hidden state. The facade must not read or store deck, hand, discard, hidden marker identity, or private choice state. Visual refreshes must continue to use public board summaries only.

## Intentionally Not Implemented

- input
- UI
- tile picking
- unit selection
- camera behavior
- animation
- VFX
- sound
- marker visuals
- hidden marker identity
- asset imports
- Blueprints
- UMG
- `.uasset` edits
- `.umap` edits
- gameplay ownership
- legal action generation
