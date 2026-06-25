# Card Activation Target Selection Presentation Audit

## Scope

This audit covers fixture-only activation target-selection presentation scaffolding for externally supplied activation legal actions.

This pass does not add UI widgets, real target picking, target legality, response windows, production zones, CardDB import, activation `FWBAction`, `WBActionCodec` activation ids, Blueprint gameplay, `.uasset`, or `.umap` work.

## Current Activation Legal Action Presentation

`WBCardActivationLegalActionPresentation` builds flat activation presentation entries from `FWBCardActivationLegalActionSet` and `FWBPublicBoardSummary`.

The current snapshot exposes activation action ids, clean public labels, source/target unit ids, and source/target public CardIds copied only from public board unit entries. It does not inspect `FWBGameStateData`, hidden zones, fixture zone context, usage keys, debug activation ids, `SourceEffectId`, or cost metadata.

## Current Target Fields

Activation legal actions carry `FWBEffectTargetRef` through `FWBCardActivationLegalAction::Target`.

The target reference currently contains:

- `TargetUnitId`
- `TargetTile`
- `TargetWallEdge`

There is no production UI target selection model yet. Target requirements are defined on card effect definitions, but presentation snapshots should reflect already-generated activation legal actions rather than re-evaluating definitions or legality.

## Current Public Board Summary Fields

`FWBPublicBoardSummary` exposes public board dimensions, terrain, walls, and visible board units. Public unit summaries include unit id, owner id, visible `CardId`, position, combat fields, RL fields, attacks left, and public statuses.

Under the current Unreal policy, visible board unit `CardId` is public. Hidden marker identity and hidden card-zone contents are not public board data.

## Public CardId Lookup

The existing activation presentation pass populates source and target public CardIds by looking up source and target unit ids in `FWBPublicBoardSummary::Units`.

Target-selection presentation should use the same policy:

- source public CardId may come only from a public source unit summary
- target public CardId may come only from a public target unit summary when the target kind is Unit
- missing public source/target units must not fail snapshot construction
- no hidden-state fallback should be attempted

## Why Consume Activation Legal Actions

Target-selection presentation is a view of activation legal actions that have already been generated externally.

It must not generate candidates, filter candidates, decide source gates, check costs, validate target legality, or call `WBRules`. Future UI can display the safe entries and choose an activation action id, but the presentation layer itself owns no rules truth.

## Why No Legality Or UI Picking

Legality remains in rules/candidate/legal-action generation paths. UI selects only.

This pass only describes target choices already embedded in activation legal actions. It does not create target-picking widgets, mutate selected targets, open response windows, or run effects.

## Hidden Information Boundary

Target-selection presentation must not expose:

- fixture zone context
- hidden hand/deck/discard/equipped entries
- usage keys
- `DebugActivationId`
- `SourceEffectId`
- cost metadata
- hidden source/effect metadata

It may expose activation action ids, clean public activation labels, simple target labels, target refs, source/target unit ids, and source/target CardIds found in public board summary.

## Deferred Work

Deferred explicitly:

- production zones
- CardDB import
- UI widgets
- actual target picking
- response windows
- effect negation
- passives
- wands
