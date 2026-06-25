# Runtime Activation Selection Resolver Audit

## Scope

This audit covers a runtime activation selection resolver that resolves a selected activation action id from already-stored runtime activation presentation state.

This pass must remain read-only. It must not execute activations, generate candidates, generate activation legal actions, inspect `FWBGameStateData`, merge activation into `FWBAction`, modify `WBActionCodec`, modify `WBRules::GenerateLegalActions`, add UI widgets, or implement target picking.

## Current Activation Presentation Model

`UWBRuntimeActivationPresentationModelComponent` stores an externally supplied `FWBCardActivationLegalActionSet`.

On refresh, it builds:

- `FWBCardActivationLegalActionPresentationSnapshot`
- `FWBCardActivationTargetPresentationSnapshot`
- `FWBRuntimeActivationPresentationStatus`

It exposes read-only getters and presentation lookup helpers. It has no activation execution method and does not inspect game state.

## Current Activation Legal Action Presentation Snapshot

`WBCardActivationLegalActionPresentation::BuildSnapshot` consumes `FWBCardActivationLegalActionSet` and `FWBPublicBoardSummary`.

It exposes activation action id, public label, source/target unit ids, and public source/target CardIds copied only from public board summary unit entries.

Its lookup helper fails on missing or duplicate activation action ids.

## Current Activation Target Presentation Snapshot

`WBCardActivationTargetPresentation::BuildSnapshot` consumes the same activation action set and public board summary.

It classifies stored target refs as None, Unit, Tile, WallEdge, or Unknown. It exposes public activation labels, public target labels, target refs, source/target unit ids, and public CardIds copied only from the public summary. Unit-target public CardIds are looked up only for unit targets.

Its lookup helper also fails on missing or duplicate activation action ids.

## Current Coordinator And Owner Refresh Behavior

`UWBRuntimeDecisionPointCoordinatorComponent` can refresh normal `FWBAction` decision points and can separately refresh activation presentation state through an optional `UWBRuntimeActivationPresentationModelComponent`.

`UWBRuntimeDecisionPointOwnerComponent` delegates activation presentation refresh and explicit activation presentation clear to the coordinator.

Neither component executes activation actions or merges activation entries into normal action presentation.

## Why This Pass Adds Selection Resolution Only

Future UI/runtime needs a safe way to resolve a selected activation action id into the stored activation legal action and its public presentation entries.

Resolution can be read-only because the activation legal action set already exists in the runtime activation presentation model.

## Why Resolution Must Not Execute Activations

Activation execution mutates rules state and belongs behind explicit effect-runner paths. The resolver returns the matching stored activation legal action and command for later use, but it must not apply the command.

## Why The Resolver Must Not Generate Candidates Or Legal Actions

Candidate and legal-action generation depend on deterministic rules inputs such as definitions, source gates, ownership, costs, and targets. Runtime presentation consumes externally supplied action families and should not own rules truth.

## Why Activation Stays Separate From FWBAction

Activation legal actions are not `FWBAction` values and are not encoded by `WBActionCodec`.

The resolver operates on `FWBCardActivationLegalActionSet` only. Normal movement, attack, end-turn, pass, and pass-response action presentation remains unchanged.

## Hidden Information Boundary

The returned `FWBCardActivationLegalAction` is internal runtime selection data and may retain command metadata for later execution handoff.

Public presentation entries must not expose:

- source effect ids
- usage keys
- debug activation ids
- cost metadata
- fixture zone entries
- hidden hand/deck/discard/equipped contents
- raw operation/schema/hook labels

Failure reasons must be stable reason codes only and must not include hidden metadata.

## Deferred Work

Deferred explicitly:

- activation execution handoff
- production zones
- CardDB import
- UI widgets
- target picking
- response windows
- negation
- passives
- wands
