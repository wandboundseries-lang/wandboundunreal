# Card Activation Board Source Presentation Audit

## Scope

This audit covers fixture-only presentation snapshots for Board-source activation legal actions.

No gameplay, CardDB import, production zones, UI widgets, response windows, passives, wands, Blueprints, `.uasset`, or `.umap` work is part of this pass.

## Current Activation Legal Action Family

`FWBCardActivationLegalAction` is a separate activation action family. It is generated from `FWBCardActivationCandidate` values by `WBCardActivationLegalActionGenerator::GenerateFromCandidates`.

The legal action family preserves deterministic activation action ids, public labels, source unit ids, targets, wrapped activation commands, cost payment commits, and usage commits. It does not add activation to `EWBActionType`, `WBActionCodec`, or `WBRules::GenerateLegalActions`.

## Current Activation Presentation Snapshot

The existing activation presentation snapshot lives beside the activation legal action type in `WBCardActivationLegalAction`. Before this pass, it consumed only the activation legal action set and copied `Candidate.SourceCardId` and `Candidate.SourceEffectId` into presentation entries.

That made the snapshot useful for early fixture inspection, but it did not enforce the future UI boundary. Board-source presentation should only expose card identity that is already public board data.

## Current FWBAction Presentation Snapshot

`WBRuntimeLegalActionPresentation` builds runtime presentation entries for ordinary `FWBAction` values from externally supplied legal actions and `FWBPublicBoardSummary`.

It does not generate legal actions, validate legality, mutate state, own `FWBGameStateData`, or inspect hidden zones. It copies source and target public CardIds only when those units are present in the public board summary.

## Public Board CardId Policy

Under the current Unreal public summary policy, visible board units are public and their unit `CardId` is public. Hidden marker identity and hidden card-zone contents are not modeled as public board data and must not be inferred.

Board-source activation presentation may show source and target CardIds only by looking up source and target unit ids in `FWBPublicBoardSummary::Units`.

## Separation From FWBAction Presentation

Activation legal actions remain separate from `FWBAction` because activation ids, activation commands, costs, usage commits, source gates, and future response-window behavior are not part of the canonical `WBActionCodec` action family yet.

Keeping activation presentation separate lets tests prove future UI display behavior without changing normal move/attack/end-turn/pass action ids, legal generation, replay, or runtime selection semantics.

## Public Label Boundary

Activation presentation labels must be player-facing. They may use `PublicLabel` when it is non-empty and safe, and otherwise fall back to `Activate`.

Labels must not expose raw operation names, schema names, hooks, rules/effect-runner names, player ids, fixture zone details, or hidden metadata such as usage keys and debug activation ids.

## Hidden Information Boundary

Presentation snapshots must not expose:

- fixture zone context
- hidden hand/deck/discard/equipped entries
- usage keys
- `DebugActivationId`
- `SourceEffectId`
- cost metadata
- hidden source/effect metadata

The snapshot may contain activation action ids and clean public labels supplied by the legal action set, plus source/target unit ids and public CardIds found in `FWBPublicBoardSummary`.

## Deferred Work

Deferred explicitly:

- production zones
- CardDB import
- UI widgets
- response windows
- effect negation
- passives
- wands
