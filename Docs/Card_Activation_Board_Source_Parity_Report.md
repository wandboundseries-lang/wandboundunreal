# Card Activation Board Source Parity Report

## Purpose

This pass adds fixture-only Board-source activation parity. It lets activation candidates require that the selected source unit's visible board `CardId` matches the activation source card id.

This is not a production card-zone system.

## Board Source Behavior

For `RequiredZone = Board` with `bRequiresFixtureZoneOwnership = true`, `WBCardActivationSourceGate` now validates:

- `SourceUnitId` is present.
- the source unit exists.
- the source unit is not defeated or removed.
- the source unit is on board.
- source unit owner matches the activating player when ownership checks are enabled.
- `SourceCardId` is present.
- source unit `CardId` is present.
- source unit `CardId` equals `SourceCardId`.

Board-source failure reasons are stable and do not include hidden data:

- `board_source_unit_required`
- `board_source_unit_missing`
- `board_source_unit_removed`
- `board_source_unit_not_on_board`
- `board_source_card_id_missing`
- `board_source_unit_card_id_missing`
- `board_source_card_id_mismatch`
- `board_source_owner_mismatch`

## SourceCardId Inheritance

Candidate generation continues to fill missing source-card context from `FWBCardDefinition::CardId`.

Effect-specific source-gate contexts inherit:

1. their own `SourceCardId` when supplied
2. the source-level `SourceCardId` when supplied
3. `Source.CardDefinition.CardId`

Effect-specific overrides can intentionally fail Board parity when they do not match the source unit's visible `CardId`.

## FixtureZoneContext Policy

Board-source parity does not use `FixtureZoneContext`. The visible board unit record is the source of truth for Board card identity.

`FixtureZoneContext` remains the fixture-only ownership source for Hand, Equipped, and Discard. Existing Fixture legacy behavior and Deck unsupported behavior remain unchanged.

## Composition

Board-source parity composes with:

- timing gates
- source unit Stunned/Frozen policy
- detailed external cost gates
- simple external cost flags
- once-per-turn usage gates
- activation candidate generation

Candidate generation remains read-only and does not mutate `FWBGameStateData`.

## Hidden Information

Visible board unit `CardId` is public under the current public board summary policy. Fixture zone metadata remains private test/scaffold data and is ignored for Board parity.

No production `Deck`, `Hand`, or `Discard` arrays are inspected. No hidden fixture zone metadata is copied into traces, public summaries, or `WBActionCodec` ids. Activation candidate ids may include visible source card id as external activation-selection metadata.

## Guardrails

Unchanged:

- Hand, Equipped, Discard, Fixture, and Deck source-zone ownership behavior
- `WBRules::GenerateLegalActions`
- `WBActionCodec`
- `FWBAction`
- activation legal action separation from player-facing action generation

Not added:

- production card zones
- CardDB import
- real card zone mutation
- equip mechanics
- wands
- response windows
- effect negation
- passives
- target-selection UI
- Blueprints, `.uasset`, or `.umap` edits

## Fixtures

Added GodotCanon scenarios for Board card-id match, inherited card id, card-id mismatch, missing/removed/off-board/wrong-owner/Stunned sources, effect-specific context inheritance, no fixture zone context, cost/usage composition, and hidden metadata exclusion.
