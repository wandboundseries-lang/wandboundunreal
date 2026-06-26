# Runtime Activation Decision Session Facade Report

## Purpose

This pass adds `UWBRuntimeActivationDecisionSessionFacadeComponent`, a thin C++ runtime session facade for future UI integration.

The facade coordinates existing runtime pieces only. It does not add gameplay behavior, generate rules data, inspect hidden zones, build public summaries from state, or merge activation into normal `FWBAction`.

## External Input

`FWBRuntimeActivationDecisionSessionRefreshInput` contains:

- externally supplied normal `FWBAction` legal actions
- externally supplied `FWBCardActivationLegalActionSet`
- externally supplied `FWBPublicBoardSummary`

The facade accepts this input for both initial session refresh and post-activation refresh.

## Refresh Session Flow

`RefreshSessionFromExternalData` requires a configured `UWBRuntimeDecisionPointOwnerComponent`.

It delegates to:

- `DecisionPointOwner->RefreshDecisionPointFromExternalData`
- `DecisionPointOwner->RefreshActivationPresentationFromExternalData`

The refresh result is successful only when both normal decision-point presentation and activation presentation refresh successfully.

## Resolve Flow

`ResolveActivationActionId` delegates to `DecisionPointOwner->ResolveSelectedActivationActionId`.

It is read-only, does not store local selection state, and does not mutate rules state.

## Handoff Flow

`CreateActivationExecutionHandoff` delegates to `DecisionPointOwner->CreateActivationExecutionHandoff`.

It preserves the existing handoff behavior: successful handoffs report `activation_execution_not_implemented` because the handoff itself is still a payload step, not execution.

## Execute And Post-Refresh Flow

`ExecuteActivationActionIdAndRefreshFromExternalData`:

1. resolves and stores the selected activation action id
2. creates and stores the activation execution handoff
3. delegates execution and post-action refresh to `DecisionPointOwner->ExecuteActivationActionIdAndRefreshFromExternalData`
4. updates facade status from the owner and activation presentation model

The facade may mutate the supplied `FWBGameStateData&` only through this existing owner/sequencer execution path.

## Status Fields

`FWBRuntimeActivationDecisionSessionStatus` reports:

- overall readiness
- normal decision-point readiness
- activation presentation readiness
- normal legal action count
- normal presentation entry count
- activation action count
- activation presentation entry count
- activation target presentation entry count
- last activation selection/execution flags
- last selected activation action id
- last reason

Session readiness requires both normal decision-point refresh and activation presentation refresh.

## Missing Owner And Failure Policy

Missing owner failures use `decision_point_owner_missing`.

Refresh failures use:

- `normal_decision_refresh_failed`
- `activation_presentation_refresh_failed`

Execution failures preserve the existing selection/sequencer reason. Failed activation execution does not refresh post-action presentation by default; refresh-on-failure remains controlled by `FWBRuntimePostActivationRefreshOptions`.

## Stale-State Policy

The facade never computes fresh post-action data.

If the caller supplies stale normal legal actions, stale activation legal actions, or a stale public board summary, the session reflects that supplied data. Tests verify presentation counts come from supplied summaries, not from mutated rules state.

`ClearSession` clears facade-local status/results only. It does not clear the owner, coordinator, or presentation models.

## Hidden-Information Policy

The facade consumes existing public/action data and delegates to existing presentation builders. It does not inspect deck, hand, discard, equipped zones, fixture zone context, hidden marker identity, CardDB-private data, or raw source gates.

Tests verify public presentation, traces, and reason strings exclude source effect ids, usage keys, debug activation ids, and cost metadata.

## No Generation Policy

The facade does not call:

- `WBRules::GenerateLegalActions`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBActionCodec`
- `WBEffectRunner`
- `WBEffectRunner::ApplyCardActivationCommand`
- public board summary builders from `FWBGameStateData`

## FWBAction Boundary

Activation remains separate from `FWBAction`.

`WBActionCodec` is unchanged. `WBRules::GenerateLegalActions` is unchanged. No activation action ids were added to normal action generation.

## Out Of Scope

- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
- overflow destruction
- wand destruction or redirect
- card-specific prevention or replacement
- death triggers
- counters
- random dice
- NPC phase
- visual animation
- VFX
- sound
- Blueprints, `.uasset`, or `.umap` changes
