# Runtime Demo Harness Report

## Scope

This pass adds a tiny C++ manual verification harness for the runtime board scaffold.

Added:

- `AWBBoardViewDemoHarnessActor`
- demo harness automation tests

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

## Harness Behavior

`AWBBoardViewDemoHarnessActor::RenderDemoBoard` gets `WBBoardViewDemoData::MakeSmallDemoBoardSummary`, then routes that public summary through `WBBoardSummaryBridge::RenderSummaryToBoardView`.

The harness has:

- `BoardViewActor`
- `bRenderDemoOnBeginPlay`, default false
- `bFindBoardViewActorIfMissing`, default false

If no board view actor is assigned, the harness returns `bRendered=false` safely. If world lookup is enabled, it searches the current world for an existing `AWBBoardViewActor`; it does not spawn actors.

## Public Summary Boundary

The harness uses demo public summary data only. It does not query or store `FWBGameStateData`, decks, hands, discards, hidden choices, or marker identity.

## Manual Editor Verification

1. Open any test level manually.
2. Place `AWBBoardViewActor`.
3. Assign simple existing meshes to `TileMesh`, `UnitMesh`, `WallMesh`, and `TerrainMesh`.
4. Place `AWBBoardViewDemoHarnessActor`.
5. Assign the `BoardViewActor` reference.
6. Either set `bRenderDemoOnBeginPlay = true` and Play, or call `RenderDemoBoard` from future C++/editor utility code.
7. Confirm a 9x9 placeholder board, two demo units, one wall, and two terrain tiles render.

No level, Blueprint, material, mesh, or map asset was created in this pass.

## State Applier Note

The demo harness remains unchanged by the board-view state applier pass. It still renders demo data directly through `WBBoardSummaryBridge`; it does not own or use `UWBBoardViewStateApplierComponent`.

## Future TODO

- real mesh assignment
- camera rig
- tile picking
- selection
- action menu
- hand UI
- response UI
- runtime state owner/controller
