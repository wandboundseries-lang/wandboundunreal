# Runtime Activation Presentation Handoff Audit

## Scope

This audit covers adding runtime handoff/storage for externally supplied activation legal actions and their presentation snapshots.

This pass must not add production zones, CardDB import, activation execution, activation `FWBAction`, activation codec ids, UI widgets, target picking, response windows, effect negation, passives, wands, Blueprints, `.uasset`, or `.umap` work.

## Current Activation Legal Action Presentation

`FWBCardActivationLegalActionSet` is a separate activation action family. It stores externally generated activation legal actions, action ids, public labels, source ids, target refs, candidate metadata, and the wrapped activation command.

`WBCardActivationLegalActionPresentation::BuildSnapshot` consumes an activation legal action set plus `FWBPublicBoardSummary`. It builds flat public entries with activation action ids, labels, source/target unit ids, and public source/target CardIds copied only from public board summary units.

This presentation path remains separate from normal `FWBAction` presentation and does not use `WBActionCodec`.

## Current Activation Target Presentation

`WBCardActivationTargetPresentation::BuildSnapshot` consumes the same activation legal action set plus `FWBPublicBoardSummary`.

It classifies already-generated activation target refs as:

- None
- Unit
- Tile
- WallEdge
- Unknown

It exposes simple public target labels and public source/target CardIds from the public summary. Unit-target public CardId lookup happens only when the target is a unit. Missing public units leave CardIds empty and do not fail snapshot construction.

The target presentation builder does not inspect `FWBGameStateData`, fixture zone context, usage keys, debug activation ids, source effect ids, cost metadata, `WBRules`, or `WBEffectRunner`.

## Current Runtime Turn Interaction Model

`UWBRuntimeTurnInteractionModelComponent` stores externally supplied normal `FWBAction` legal actions and builds `FWBRuntimeLegalActionPresentationSnapshot`.

It executes selected normal action ids through `WBRuntimeActionSelectionBridge`, using external mutable state and runtime context supplied by the caller. It does not generate legal actions, own game state, or inspect hidden zones.

This model is for normal `FWBAction` only and should not be widened to activation legal actions in this pass.

## Current Runtime Refresh Adapter

`WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint` refreshes the normal turn interaction model and optional visual controller from externally supplied `FWBAction` legal actions and `FWBPublicBoardSummary`.

It validates requested refresh targets first, then forwards public data. It does not generate legal actions or execute selected actions.

## Current Decision-Point Coordinator

`UWBRuntimeDecisionPointCoordinatorComponent` coordinates normal decision-point refresh and selected-action handoff.

It stores a normal decision-point status, exposes the normal presentation snapshot, and delegates selected action execution through `UWBRuntimeTurnInteractionModelComponent`. It does not call `WBRules` or `WBEffectRunner` directly and does not own `FWBGameStateData`.

Activation presentation should be optional and separate so missing activation UI/storage cannot break normal decision-point refresh.

## Current Decision-Point Owner

`UWBRuntimeDecisionPointOwnerComponent` accepts externally supplied normal legal actions and public board summaries, delegates refresh to the coordinator, delegates selected action ids through the coordinator, and can explicitly refresh from caller-supplied post-action normal data after successful execution.

It does not generate legal actions, decide legality, call `WBRules`, call `WBEffectRunner`, or own rules state.

## Why Activation Presentation Remains Separate

Activation legal actions are not `FWBAction` values and are not encoded by `WBActionCodec`.

Keeping activation presentation in a sibling runtime model avoids mixing future activation UI/storage into the normal movement/attack/end-turn/pass action list. It also preserves the current replay/action-codec boundary while card zones, costs, response windows, and real activation execution remain deferred.

## Why The Runtime Layer Consumes External Activation Legal Actions

Runtime presentation is a display/handoff layer. The caller owns rules truth and supplies activation legal actions plus public summary data.

This pass should only copy those externally supplied activation actions, build public snapshots, and expose read-only lookup helpers for future UI.

## Why This Pass Does Not Generate Activation Candidates

Candidate generation needs card definitions, source gates, source ownership, cost context, and target refs. Those belong to deterministic rules/test scaffolding and future production rules owners, not a runtime presentation model.

The runtime activation presentation model receives the already-generated activation legal action set and does not call `WBCardActivationCandidateGenerator` or `WBCardActivationLegalActionGenerator`.

## Why This Pass Does Not Execute Activations

Activation execution mutates rules state through `WBEffectRunner::ApplyCardActivationCommand`. This pass only stores and exposes presentation snapshots.

No runtime activation execution API is added here.

## Why This Pass Does Not Implement UI Target Picking

The target presentation snapshot displays target choices that are already embedded in externally supplied activation legal actions. It does not discover valid targets or mutate chosen targets.

Future UI can render these read-only entries and choose an activation action id, but real target picking remains deferred.

## Hidden Information Boundary

Runtime activation presentation may expose:

- activation action ids
- clean public activation labels
- target kind
- clean public target labels
- target refs already present on legal actions
- source/target unit ids
- source and unit-target public CardIds from `FWBPublicBoardSummary`

Runtime activation presentation must not expose:

- hidden hand/deck/discard/equipped contents
- fixture zone context
- usage keys
- debug activation ids
- source effect ids
- cost metadata
- raw operation/schema/hook labels
- hidden marker identity

Public board unit `CardId` remains public under the current public board summary policy.

## Deferred Work

Deferred explicitly:

- production zones
- CardDB import
- UI widgets
- actual target picking
- response windows
- effect negation
- passives
- wands
