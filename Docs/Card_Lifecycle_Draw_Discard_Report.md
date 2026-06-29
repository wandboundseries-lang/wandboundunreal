# Card Lifecycle Draw / Discard Report

Date: 2026-06-28

## Purpose

This pass adds deterministic production card lifecycle movement for Deck, Hand, and Discard.

The implementation is Core-first and state-only except for the narrow production execution handoff integration that moves a successfully used hand-source activation card to Discard before provider refresh.

## DrawOneCard

`WBCardLifecycle::DrawOneCard` moves the top deterministic Deck entry to the end of Hand.

Deck top is the lowest `ZoneIndex` after deterministic sorting. The drawn card keeps its card id and instance id, changes zone to Hand, and gets an appended Hand `ZoneIndex`.

If Deck is empty, the helper returns `deck_empty` without mutation.

## DrawCards

`WBCardLifecycle::DrawCards` draws one card at a time in deterministic order.

If Deck becomes empty mid-request, previous successful draws remain applied and the failing draw returns `deck_empty`.

## Setup Draw Helper

`WBCardLifecycle::ApplySetupDraw` delegates to `DrawCards`. It supports deterministic setup hands, including a 6-card setup draw, without hero selection, marker placement, shuffle, or full setup behavior.

## Turn-Start Draw Helper

`WBCardLifecycle::ApplyTurnStartDraw` is helper-only in this pass.

If `ActivePlayerId == FirstPlayerId` and `TurnNumber == 1`, it returns success/no-op with reason `first_player_first_turn_draw_skipped`.

Otherwise it draws one card for the active player. Existing turn transition code was not modified.

## Hand-To-Discard After Activation

`FWBProductionActivationExecutionHandoff` now handles successful Hand-source activations by moving the used hand source instance to Discard before post-action provider refresh.

The movement happens only after successful execution and only when the provider action is a Hand source with a known source instance id. Failed selection, failed execution, Board-source activation, and already-moved hand cards do not discard.

Future summon/equip execution is expected to use the same lifecycle boundary for Hand-source card movement, but the summon/equip/RL foundation pass only generates read-only options and does not move cards.

The deterministic summon execution pass adds a separate Hand-to-Board transition for Character summon. It removes the source card from Hand, normalizes remaining Hand indexes, creates a board unit from Character stats, and does not move the card to Discard.

## Deterministic Ordering Policy

- Source and destination zones are sorted before movement.
- Source zone relative order is preserved after removal.
- Source zone indexes are normalized to `0..N-1`.
- Destination cards append at `max existing ZoneIndex + 1`.
- No shuffle or RNG is introduced.

## Observation Update Behavior

No observation code changes were required.

After lifecycle movement, `WBCardZoneObservation` reads the updated `FWBCardZoneState` and reflects updated public counts, owner-private Hand identity, and owner-private Discard identity.

## Hidden-Info Policy

Public summaries remain count-only for Deck, Hand, and Discard.

Deck identity, opponent hand identity, opponent deck identity, and hidden marker identity remain excluded. Owner observations may see their own Hand and Discard identities.

## Boundaries

This pass did not add:

- shuffle
- full setup
- mulligans
- equip gameplay
- marker reveal behavior
- NPC phase
- passives or wands
- response windows
- UI, target picking UI, Blueprint, `.uasset`, or `.umap` changes
- activation `FWBAction` creation
- `WBActionCodec` changes
- `WBRules::GenerateLegalActions` changes
- Godot CardDB import
- broad production CardDB loading

## Tests

Added `Wandbound.Core.CardLifecycle.*` coverage for draw order, setup draw, turn-start skip, hand-to-discard, empty deck, duplicate ids, deterministic indexes, observation updates, hidden-info exclusion, and source guards.

Updated `Wandbound.Runtime.ProductionActivationExecutionHandoff.*` coverage so successful Hand-source activation discards, failed Hand-source activation does not discard, Board-source activation does not discard hand cards, refresh happens after discard, and hidden secrets remain excluded.

## Next Planned Pass

Add the next production vertical-slice lifecycle/setup step, likely deterministic summon/equip/RL foundation or an explicit turn-orchestration integration point for turn-start draw once the action timing boundary is confirmed.
