# Runtime Turn Result Envelope Report

Date: 2026-06-17

## Summary

This pass adds a deterministic runtime result envelope around selected-action execution. The envelope reports the selected action, whether an MP roll was consumed, the consumed roll value, the existing apply result and traces, and a final public turn summary.

## New Public Turn Summary

Added:

```text
FWBPublicPlayerTurnSummary
FWBPublicTurnSummary
WBPublicTurnSummary::Build
```

The summary includes only public turn/resource state:

- current player id
- priority player id
- turn number
- phase
- game-over flag
- player remaining MP
- player last MP roll
- wall counters

It does not include deck, hand, hidden choices, private card identities, or full unit/card hidden state.

## New Runtime Envelope

Added:

```text
FWBRuntimeSelectedActionResult
WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult
```

The envelope includes:

- `ApplyResult`
- `SelectedActionType`
- `SelectedActionId`
- `bConsumedMPRoll`
- `ConsumedMPRoll`
- `FinalPublicTurnSummary`

`ApplyResult` remains the authoritative mutation result and contains the emitted trace events.

## Selected Action ID Reporting

`SelectedActionId` is computed with the existing `WBActionCodec::MakeActionId` helper.

No runtime context, MP roll, or full-transition marker is included in the ID.

Examples:

```text
end_turn:p0
move:p0:u1:4,4->5,4
pass_response:p1
```

## MP Roll Reporting

Move, PassResponse, and basic EndTurn report:

```text
bConsumedMPRoll = false
ConsumedMPRoll = 0
```

Full-transition EndTurn reports the consumed deterministic roll only after the roll source returns a valid roll.

Missing roll source and invalid queued/fixed rolls fail without reporting a consumed roll.

## Trace Behavior

No wrapper trace is added by the envelope.

Trace events remain exactly those emitted by the existing core execution path:

- Move emits `move`
- basic EndTurn emits `end_turn`
- full EndTurn emits the full transition trace sequence
- PassResponse emits `pass_response`
- failures emit no success traces unless the delegated behavior later changes

## Fixture Coverage

Added fixture operation:

```text
operation = apply_runtime_selected_action_with_result
```

Added fixtures:

- `runtime_result_end_turn_full_transition_roll_4.json`
- `runtime_result_end_turn_basic_no_roll.json`
- `runtime_result_move_summary.json`
- `runtime_result_pass_response_summary.json`
- `runtime_result_invalid_roll_no_mutation.json`

This fixture operation is additive and does not alter `apply_action`, `apply_turn_command`, or `apply_runtime_selected_action`.

## Unchanged Boundaries

- Legal action generation is unchanged.
- `WBActionCodec` action IDs are unchanged.
- Replay `apply_action` is unchanged.
- Full transition is not exposed as a legal action.
- Random MP generation is not implemented.
- UI/Blueprint/3D runtime is not wired.

## Future Use

Future runtime/UI code can use the envelope as the public result object after a selected action resolves. It is suitable for presenting turn/resource changes and trace-driven feedback without leaking hidden card data.
