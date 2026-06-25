# Card Activation Board Source Legal Action Report

## Purpose

This pass adds focused fixture-only coverage for Board-source activation candidates converting into the separate activation legal action family.

No production card zones, CardDB import, activation `FWBAction`, `WBActionCodec` activation ids, response windows, UI, Blueprints, `.uasset`, or `.umap` work was added.

## Flow

Board-source activation definitions still enter through externally supplied `FWBCardActivationCandidateSource` fixture data. Candidate generation validates Board-source parity against the visible board unit `CardId`, then `WBCardActivationLegalActionGenerator::GenerateFromCandidates` converts the resulting candidates into `FWBCardActivationLegalAction` values.

The conversion path preserves:

- `ActivationCandidateId`
- `PublicLabel`
- `SourceUnitId`
- `SourceCardId`
- `SourceEffectId`
- target reference
- wrapped `FWBCardActivationCommand`
- `CostPaymentCommit`
- `UsageCommit`

## ActivationActionId Policy

Board-source activation legal action ids come directly from `Candidate.ActivationCandidateId`. They are deterministic activation-family ids and are not `WBActionCodec` ids.

Focused coverage verifies stable ids for repeated generation and distinct ids when source, effect, or target differs.

## PublicLabel Policy

Legal action labels use `Candidate.PublicLabel`, falling back to `Activate` only when empty. Board-source tests verify player-facing labels such as `Deal 2 damage`, `Heal 2 HP`, `Apply Burn`, and `Restore Armor`, and reject internal operation/schema/hook labels.

## Cost And Usage

Board-source candidate generation can produce cost and usage commits through existing source-gate scaffolding. Legal action generation preserves both commits without paying costs, marking usage, or mutating state.

Selected-id replay coverage verifies explicit application through the test-only replay verifier and existing `WBEffectRunner::ApplyCardActivationCommand` path.

## Separation

Activation legal actions remain separate from `FWBAction`.

Confirmed unchanged:

- `WBRules::GenerateLegalActions`
- `WBActionCodec`
- existing Move, Attack, EndTurn, Pass, and PassResponse action ids

## Hidden Information

Visible board unit `CardId` remains public under current public board summary policy. Fixture zone metadata, usage keys, hand/deck/discard scaffolding, and debug activation metadata are excluded from trace serialization and public board assertions.

## Fixture-Only Scope

This pass verifies the Board-source path using focused GodotCanon fixtures. It does not add production zone ownership, CardDB loading, card movement, equip/wand behavior, activation target UI, response windows, negation, passives, wands, card-specific prevention/replacement, counters, or random dice.

## Golden Scenarios

Added or updated focused Board-source activation legal action fixtures for:

- damage, status, armor, and heal legal action conversion
- deterministic ordering
- cost payment commit preservation
- usage commit preservation
- selected-id replay success
- card mismatch selected-id failure
- separation from `FWBAction`
- activation action id stability
- hidden metadata exclusion
