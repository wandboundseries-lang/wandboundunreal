# Runtime Turn Result Envelope Audit

Date: 2026-06-17

## Files Inspected

- `Source/WandboundCore/Public/WBRuntimeTurnResolutionAdapter.h`
- `Source/WandboundCore/Private/WBRuntimeTurnResolutionAdapter.cpp`
- `Source/WandboundCore/Public/WBSelectedActionExecutor.h`
- `Source/WandboundCore/Private/WBSelectedActionExecutor.cpp`
- `Source/WandboundCore/Public/WBMPRollSource.h`
- `Source/WandboundCore/Private/WBMPRollSource.cpp`
- `Source/WandboundCore/Public/WBActionCodec.h`
- `Source/WandboundCore/Private/WBActionCodec.cpp`
- `Source/WandboundCore/Public/WBAction.h`
- `Source/WandboundCore/Public/WBReplayTrace.h`
- `Source/WandboundCore/Private/WBReplayTrace.cpp`
- `Source/WandboundTests/Private/`

## Current Runtime Adapter Behavior

`WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction` is a pure C++ runtime-facing gateway for selected legal actions.

Current behavior:

- non-`EndTurn` actions delegate to `WBSelectedActionExecutor` with default selected-action context
- basic `EndTurn` delegates to `WBSelectedActionExecutor` with full transition disabled
- full-transition `EndTurn` requires an `IWBMPRollSource`
- full-transition `EndTurn` asks the roll source for the next player's MP roll
- after receiving a valid roll, it delegates to `WBSelectedActionExecutor`

The adapter does not generate random rolls, does not mutate state directly, and does not add wrapper trace events.

## Current MP Roll Source Behavior

`IWBMPRollSource` exposes:

```text
TryGetNextMPRoll(PlayerId, OutRoll, OutReason)
```

Current deterministic implementations:

- `FWBFixedMPRollSource`
- `FWBQueuedMPRollSource`

Valid rolls are `1..6`.

Invalid fixed rolls fail with `invalid_mp_roll`.

Queued rolls fail with `mp_roll_queue_empty` when empty. Invalid queued rolls fail with `invalid_mp_roll` and are not popped.

Roll sources do not mutate game state.

## Current Action ID Behavior

`WBActionCodec::MakeActionId` already computes action IDs from `FWBAction`.

Current IDs include:

- `move:pN:uID:x,y->x,y`
- `end_turn:pN`
- `pass:pN`
- `pass_response:pN`

No runtime context, MP roll, command mode, or full-transition marker is included in action IDs.

## Existing Replay Behavior

Replay `apply_action` and `WBReplayVerifier` still use:

```text
WBActionCodec -> WBEffectRunner::ApplyAction
```

That path keeps `EndTurn` as the basic player action and does not use the runtime adapter or selected-action full-transition path.

## Public Turn Summary Scope

Safe public turn summary fields:

- current player id
- priority player id
- turn number
- phase
- game-over flag
- per-player remaining MP
- per-player last MP roll
- per-player wall counters

The summary must not include:

- deck contents
- hand contents
- discard contents if later considered hidden
- private card ids
- hidden pending choices
- hidden marker identities
- full unit/card hidden state

## Chosen Names

Public summary:

```text
FWBPublicPlayerTurnSummary
FWBPublicTurnSummary
WBPublicTurnSummary::Build
```

Runtime result envelope:

```text
FWBRuntimeSelectedActionResult
WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult
```

## Why This Pass Is Reporting-Only

The existing runtime adapter already performs the selected-action execution behavior needed by runtime callers. This pass only packages the result for future runtime/UI consumers:

- selected action type
- selected action id
- apply result and traces
- consumed MP roll reporting
- final public turn summary

Legal actions, action IDs, replay `apply_action`, random roll generation, and gameplay semantics remain unchanged.

## Open TODOs

- Add a future UI/runtime bridge that consumes the envelope.
- Add a random roll source only after deterministic reporting remains stable.
- Decide future network replay semantics separately from `apply_action` replay.
- Extend public summaries only after hidden-information policy is explicit for each field.
