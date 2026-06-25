# Card Activation Source-Zone Ownership Report

## Purpose

This pass adds fixture-only source-zone ownership gates for card activation candidates. It lets tests declare that a fixture card is in Hand, Equipped, Discard, Board, Deck, or Fixture, then filters activation candidates through that metadata.

This is not the production hand/deck/discard/equipped zone system.

## Fixture Types

Added:

- `FWBCardActivationFixtureZoneEntry`
- `FWBCardActivationFixtureZoneContext`
- `FWBCardActivationSourceGateDefinition::bRequiresFixtureZoneOwnership`
- `FWBCardActivationSourceGateContext::SourceCardId`
- `FWBCardActivationSourceGateContext::FixtureZoneContext`

`FWBCardActivationFixtureZoneEntry` contains:

- `CardId`
- `OwnerPlayerId`
- `Zone`
- `EquippedToUnitId` for Equipped sources only

## Supported Zones

- Fixture: legacy mode, still supported without fixture ownership metadata.
- Hand: requires a matching fixture entry for source card, player, and Hand.
- Equipped: requires a matching fixture entry and matching source unit id.
- Discard: requires a matching fixture entry for source card, player, and Discard.
- Board: supported as fixture metadata, but real board/card relationships remain future work.
- Deck: explicitly unsupported for activation in this scaffold and fails with `source_zone_deck_not_supported` when ownership metadata otherwise matches.

## Gate Behavior

`WBCardActivationSourceGate::Evaluate` now composes checks in this order:

1. player and game-over checks
2. source zone enum/mismatch checks
3. fixture zone ownership if requested
4. timing checks
5. source unit ownership/removed/status checks
6. simple external cost flag
7. detailed cost gate
8. once-per-turn usage

Failure reason strings are generic and do not include hidden card names.

## Candidate Generation

`WBCardActivationCandidateGenerator` now fills `SourceCardId` from `FWBCardDefinition::CardId` when fixture context omits it. Effect-specific source-gate contexts inherit player id, source unit id, source card id, source zone, and fixture zone context from the source-level context where missing.

Candidate generation remains read-only and does not mutate real player `Hand`, `Deck`, or `Discard` arrays.

## Composition

Fixture zone ownership composes with:

- normal-turn priority timing
- source unit ownership/removed/stunned/frozen checks
- detailed cost gate affordability
- once-per-turn activation usage
- activation legal action family generation after candidate filtering

## Hidden Information

Fixture zone metadata is not serialized into traces, public turn summaries, public board summaries, or runtime selected-action JSON. Candidate/action ids are external fixture selection metadata and remain outside `FWBAction` and `WBActionCodec`.

## Guardrails

Unchanged:

- `FWBAction`
- `WBRules::GenerateLegalActions`
- `WBActionCodec`
- `WBEffectRunner` activation application semantics

Not added:

- production CardDB import
- production card zones
- real draw/discard/equip movement
- wand attachment behavior
- response windows
- effect negation
- UI target selection
- Blueprints, `.uasset`, or `.umap` edits

## Fixtures

Added GodotCanon scenarios for Hand, Equipped, Discard, Deck unsupported, Fixture legacy mode, cost/usage composition, and hidden metadata exclusion.
