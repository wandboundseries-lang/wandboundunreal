# Runtime Visual Scaffold Report

## Scope

This pass adds the first Unreal runtime visual scaffold for Wandbound. It is a read-only display layer that consumes public board summary data and renders placeholder board categories through instanced mesh components.

Added:

- `WandboundRuntime` module
- `WBBoardViewTypes` coordinate helper
- `AWBBoardViewActor`
- `WBBoardViewDemoData`
- `WBBoardSummaryBridge`
- `AWBBoardViewDemoHarnessActor`
- `UWBBoardViewStateApplierComponent`
- `UWBRuntimeVisualControllerComponent`
- `UWBRuntimeControllerFacadeComponent`

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
- animations
- VFX
- sound
- camera focus logic
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- CardDB import
- marker visuals
- hidden marker identity

## Public Summary Consumption

`AWBBoardViewActor::RenderPublicBoardSummary` consumes `FWBPublicBoardSummary` only. It does not hold or query `FWBGameStateData`, deck, hand, discard, hidden marker identity, pending choices, or private replay data.

Actors display state only. Rules truth remains in `WandboundCore`.

`WBBoardSummaryBridge` now provides a minimal runtime-safe refresh path:

- build a public summary from rules state through `WBPublicBoardSummary::Build`
- render an existing public summary into `AWBBoardViewActor`
- return public render counts through `FWBBoardViewRefreshResult`

No input, gameplay ownership, hidden-state access, or UI behavior was added.

`AWBBoardViewDemoHarnessActor` now provides an optional C++ manual verification helper. It renders `WBBoardViewDemoData::MakeSmallDemoBoardSummary()` through the bridge into an assigned board view actor. It remains public-summary-only and does not add input, gameplay ownership, UI, camera, animation, VFX, Blueprints, or assets.

`UWBBoardViewStateApplierComponent` now provides a read-only visual state update surface for future runtime/controller code. It caches and applies `FWBPublicBoardSummary` only, with optional const-state conversion through `WBBoardSummaryBridge`.

`UWBRuntimeVisualControllerComponent` now provides the public-summary-only controller shell for future runtime selected-action results. It forwards `FWBPublicBoardSummary` directly, or reads only `FWBRuntimeSelectedActionResult::FinalPublicBoardSummary`, then coordinates refresh calls into `UWBBoardViewStateApplierComponent`.

`WBSelectedActionVisualHarness` now provides a C++-only selected-action-to-visual-refresh seam. It executes an externally supplied selected action through the existing runtime adapter, then optionally forwards the runtime result's public board summary to the visual controller shell.

`UWBRuntimeControllerFacadeComponent` now provides a C++-only runtime controller facade for future input/UI code. It accepts an externally selected action, delegates to the selected-action visual harness, stores the latest runtime and visual refresh results, and still does not own rules state or generate legal actions.

`WBRuntimeActionSelectionBridge` now provides a C++-only action ID resolver for externally supplied legal action lists. It does not implement input/UI and does not generate or validate actions; it resolves a selected ID and delegates through the runtime controller facade.

`WBRuntimeLegalActionPresentation` now provides a C++-only public presentation snapshot for externally supplied legal actions. It consumes `FWBPublicBoardSummary`, emits action IDs and simple public labels, and still does not implement input/UI or generate/validate actions.

`UWBRuntimeTurnInteractionModelComponent` now stores the current externally supplied legal actions and presentation snapshot for future UI consumption. It can execute selected action ids through the action-selection bridge/facade path, but still does not generate actions, own rules state, or implement input/UI.

`WBRuntimeInteractionRefreshAdapter` now provides the C++-only decision-point refresh path. It accepts externally generated legal actions and a public board summary, refreshes the turn interaction model, and can refresh the visual controller from that same public summary. It still does not implement input/UI, generate actions, decide legality, execute actions, own state, inspect hidden information, or add camera/VFX/assets.

`UWBRuntimeDecisionPointCoordinatorComponent` now provides the C++-only read-only decision-point status surface. It wraps the refresh adapter, exposes current status and presentation entries for future UI, and still does not implement input/UI, generate actions, decide legality, execute actions, own state, inspect hidden information, or add camera/VFX/assets.

`UWBRuntimeDecisionPointCoordinatorComponent` now also provides selected-action handoff to the existing turn interaction model execution path. The handoff is still C++-only, still requires externally supplied state/context/facade, and still does not implement input/UI, generate actions, decide legality, call `WBEffectRunner` directly, own state, inspect hidden information, or refresh legal actions automatically.

`FWBDecisionLoopTestHarness` now provides test-only automation coverage for refresh, selected-action handoff, explicit post-action refresh, and two decision points. It lives under `WandboundTests` only. Production runtime code does not include it and still does not generate legal actions.

The visual runtime path remains public-summary-only. No input, UI, camera behavior, animation, VFX, audio, marker visuals, assets, Blueprints, `.uasset`, or `.umap` work was added.

## Placeholder Rendering

The actor owns:

- tile instanced mesh component
- unit instanced mesh component
- wall instanced mesh component
- terrain instanced mesh component

Mesh properties are editable and null by default:

- `TileMesh`
- `UnitMesh`
- `WallMesh`
- `TerrainMesh`

If a mesh is null, the actor skips adding instances for that category and does not crash. Rendered count accessors still report intended public-summary counts for tests and future runtime wiring.

## Coordinate Policy

The board origin is centered at world origin.

- Tile `(4,4)` maps to `(0,0,0)` on a 9x9 board.
- Tile `(0,0)` maps to negative X and negative Y.
- Tile `(8,8)` maps to positive X and positive Y.
- X increases right.
- Y increases forward.
- View/camera orientation is intentionally deferred.

## Manual Visual Test

Manual smoke test steps for a future test level:

1. Add `AWBBoardViewActor` to a level.
2. Assign simple meshes to `TileMesh`, `UnitMesh`, `WallMesh`, and `TerrainMesh` in the Details panel.
3. From future runtime bridge code, call `RenderPublicBoardSummary` with a `FWBPublicBoardSummary`.
4. For a quick C++ harness, call `WBBoardViewDemoData::MakeSmallDemoBoardSummary()` and pass the result to the actor.

No level, Blueprint, material, mesh, or map asset was created in this pass.

## Future TODO

- tile picking
- camera
- real meshes/materials
- unit model avatars
- card art UI
- hand/action UI
- response UI
- animation/VFX
- marker visuals after marker state exists
