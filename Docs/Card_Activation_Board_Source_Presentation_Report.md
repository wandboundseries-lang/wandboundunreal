# Card Activation Board Source Presentation Report

## Purpose

This pass adds fixture-only Board-source activation legal action presentation snapshot coverage.

It proves future UI can display externally supplied activation legal actions without adding activation to `FWBAction`, `EWBActionType`, `WBActionCodec`, or `WBRules::GenerateLegalActions`.

## Snapshot Fields

`FWBCardActivationLegalActionPresentationEntry` now exposes:

- activation action id
- public label
- player id
- source unit id
- target unit id
- public source-unit found flag
- public target-unit found flag
- source public CardId
- target public CardId

`BuildSnapshot` consumes only `FWBCardActivationLegalActionSet` and `FWBPublicBoardSummary`.

## Public Board Summary Dependency

Source and target public CardIds are copied only from `FWBPublicBoardSummary::Units`.

If the source or target unit is absent from the supplied public summary, the matching `bHasPublic...Unit` flag remains false and the matching public CardId remains empty. The snapshot does not inspect `FWBGameStateData`, hidden zones, fixture zone context, or card-zone state.

## Public Label Policy

Presentation uses the activation action `PublicLabel` when non-empty and falls back to `Activate`.

The activation legal action generator already rejects internal labels containing operation/schema/hook terms such as `damage_effect`, `heal_effect`, `armor_effect`, `status_effect`, `EffectRunner`, `Rules`, `schema`, or `hook`.

## Hidden Metadata Exclusions

Focused tests confirm the public presentation surface excludes:

- `SourceEffectId`
- usage keys
- `DebugActivationId`
- cost metadata
- fixture zone context
- hidden hand/deck/discard scaffolding

Visible board unit `CardId` remains public under the current public board summary policy.

## Lookup Behavior

`TryFindEntryByActivationActionId` returns true only for exactly one matching presentation entry.

It returns false when no entry matches and false when duplicate entries share the same activation action id.

## Separation

Activation presentation remains separate from `WBRuntimeLegalActionPresentation`, which is still only for `FWBAction`.

Confirmed unchanged:

- `WBRules::GenerateLegalActions`
- `WBActionCodec`
- move, attack, end turn, pass, and pass response action ids

## Fixture-Only Scope

Added the test-only fixture operation:

```json
"operation": "build_card_activation_presentation_snapshot"
```

The operation parses externally supplied activation sources, generates candidates, generates activation legal actions, builds the presentation snapshot, validates expected presentation fields, and does not apply activation commands.

Out of scope remains:

- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- response windows
- effect negation
- UI target selection
- passives
- wands
