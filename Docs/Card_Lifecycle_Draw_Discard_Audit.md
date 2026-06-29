# Card Lifecycle Draw / Discard Audit

## Current Scope

This audit covers deterministic Deck, Hand, and Discard movement for production card lifecycle scaffolding. It intentionally keeps shuffle, mulligan, summon, equip, response windows, UI selection, activation `FWBAction` integration, `WBActionCodec`, and `WBRules::GenerateLegalActions` out of scope.

## Current CardZoneState Behavior

`FWBCardZoneState` stores per-player ordered Deck, Hand, and Discard arrays as `FWBZoneCardEntry` values. Each entry carries a public `CardId`, private/stable `InstanceId`, owner, zone, and `ZoneIndex`.

`WBCardZoneState::SortOrderedZonesDeterministically` sorts player zones by `PlayerId`, and sorts Deck, Hand, and Discard by `ZoneIndex`, then `InstanceId`, then `CardId`. Validation checks invalid player ids, duplicate player zones, duplicate card instance ids across zones/equipped cards, card ownership mismatches, zone mismatches, and invalid marker placeholders. It does not currently normalize zone indexes after removal.

## Current Deck / Hand / Discard Ordering Policy

The existing zone sort policy gives this pass a stable deterministic order. For lifecycle moves, this pass will treat the top of Deck as the lowest `ZoneIndex` after deterministic sorting.

Chosen movement policy:

- Sort the source and destination ordered zones deterministically before movement.
- Preserve relative order in the source zone after removal.
- Normalize the source zone's `ZoneIndex` values to `0..N-1` after removal.
- Append the moved card to the destination zone with `ZoneIndex = max existing destination ZoneIndex + 1`.
- Keep destination relative order stable.

This means draw order is deterministic, repeated draws are deterministic, and gaps in a source zone caused by movement are removed without reshuffling destination history.

## Current Turn-Start Behavior

`FWBGameStateData::AdvanceTurnBasic` advances player and turn counters, while `ApplyTurnStartResourceSetupForPlayer` handles resource setup such as MP rolls. Card draw is currently not part of the deterministic turn transition. The existing comments already keep card draw and start triggers outside the baseline transition.

This pass will expose `WBCardLifecycle::ApplyTurnStartDraw` as a narrow helper. It will not integrate draw into existing turn transition code because that would broaden turn orchestration and risks touching rule flow beyond the lifecycle boundary.

First-player first-turn policy:

- If `ActivePlayerId == FirstPlayerId` and `TurnNumber == 1`, return success/no-op with stable reason `first_player_first_turn_draw_skipped`.
- Otherwise draw one card for the active player.
- Empty deck returns `deck_empty` and does not implement lose-on-empty-deck.

## Current Activation Execution Handoff Behavior

`FWBProductionActivationExecutionHandoff` currently:

- Requests provider-generated activation entries.
- Resolves target selection through the production target-selection bridge.
- Rebuilds an internal activation command from repository/provider data.
- Executes through the existing runtime activation execution bridge.
- Refreshes provider data after execution.

The handoff does not own legality generation, does not create `FWBAction`, does not call `WBRules::GenerateLegalActions`, and does not call `WBEffectRunner` directly.

## Current Hand-Source Activation Behavior

Hand-source activation entries encode source zone and source instance in provider action ids, for example with `:zhand:` and `:i<instance>:` segments. Successful hand-source execution currently applies the effect path but leaves the used card in Hand, so the follow-up provider refresh can still see the same hand-source candidate.

This pass will move the used hand card to Discard only after successful hand-source execution and before provider refresh.

## Chosen Lifecycle Helper / API

This pass will add `WBCardLifecycle` in `WandboundCore` with deterministic helpers:

- `DrawOneCard`
- `DrawCards`
- `MoveHandCardToDiscard`
- `ApplySetupDraw`
- `ApplyTurnStartDraw`
- `ResultCodeToString`

The helper operates on `FWBGameStateData` / `FWBCardZoneState` only. It does not mutate repository definitions, does not generate legal actions, and does not create runtime/UI state.

## Post-Resolution Hand-Source Movement Policy

After successful existing execution:

- If the selected provider action is a Hand-source activation, extract the source instance id from the action id.
- Move that exact card from the viewer/active player's Hand to Discard with `WBCardLifecycle::MoveHandCardToDiscard`.
- Refresh provider data after the movement.

The movement will not happen on selection failure, execution failure, board-source activations, unknown source instance ids, or if the card has already left Hand. A missing/relocated card fails safely with lifecycle diagnostics instead of guessing replacement/exile behavior.

## Hidden-Info Policy

Lifecycle results may carry the moved card id and instance id for internal diagnostics and tests. Public observations must continue to expose counts only for Deck, Hand, and Discard and must not reveal deck identities, opponent hand identities, or hidden marker identities.

Owner observations may show the owner's Hand and Discard identities after movement. Opponent observations remain count-only for hidden zones.

Handoff refresh data and diagnostics must not expose opponent Hand, Deck, or marker secret identities.

## Exclusions

No shuffle is added because shuffle requires a canon RNG policy and deck construction boundary that are not part of this deterministic movement pass.

No UI is added because UI selects only and this pass is rules/state scaffolding.

No response windows are added because post-resolution hand movement is being modeled only for immediate successful activation execution; response/counter/replacement timing remains future work.

`WBActionCodec` is not modified because lifecycle movement is not yet an action id surface and activation actions are not being added to `FWBAction`.

`WBRules::GenerateLegalActions` is not modified because lifecycle actions are not part of normal move/attack/end-turn/pass legal action generation in this pass.

## Risks / Open Questions

- Future card effects may replace the destination for used cards, such as exile/banish/equip/transform semantics.
- Empty-deck consequences are not modeled yet.
- Full setup, mulligan, hero selection, and marker placement remain separate lifecycle/setup passes.
- Turn-start draw is helper-only until turn orchestration can adopt it without broad rules-flow changes.
- Public Discard is count-only in current observation policy; if canon makes discard identities public later, observation policy will need a deliberate update.
