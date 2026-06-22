# Runtime Legal Action Presentation Snapshot Report

## Scope

This pass adds a pure C++ presentation snapshot for precomputed legal actions.

Added:

- `EWBRuntimeActionPresentationType`
- `FWBRuntimeActionPresentationEntry`
- `FWBRuntimeLegalActionPresentationSnapshot`
- `WBRuntimeLegalActionPresentation`
- legal-action presentation automation tests
- legal-action presentation audit

Not added:

- gameplay rules
- legal action generation
- legality decisions
- state ownership
- player input
- tile picking
- unit selection
- UI widgets
- camera logic
- animations
- VFX
- sound
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- marker visuals
- hidden marker identity
- JSON serialization

## Snapshot Purpose

`WBRuntimeLegalActionPresentation` builds public, UI-ready entries from an externally supplied legal action list and a public board summary.

It does not know how legal actions were produced. It does not call `WBRules`, generate actions, validate legality, mutate state, or own `FWBGameStateData`.

## Snapshot Fields

Each entry includes:

- stable `ActionId`
- presentation `Type`
- public label
- player id
- source and target unit ids
- from and to tiles
- source and target public card ids when found in the public board summary
- flags for whether public source/target units were found

Structured ids and tiles are kept out of the public label.

## Public Label Policy

Labels are simple future UI-facing text:

- `Move`
- `Attack`
- `End Turn`
- `Pass`
- `Action` for unknown action types

`PassResponse` currently presents as `Pass` for player-facing clarity.

Labels do not include raw schema names, player ids, coordinates, internal hook names, `Rules`, or `EffectRunner`.

## Public Unit Lookup

Source and target public card ids are copied only from `FWBPublicBoardSummary::Units`.

If a source or target unit is absent from the public summary:

- the matching `bHas...PublicUnit` flag remains false
- the matching public card id remains empty
- no hidden state lookup is attempted

Removed units and hidden marker identities are not inferred.

## Lookup Behavior

`TryFindEntryByActionId` mirrors action-selection ambiguity behavior:

- returns true for exactly one matching entry
- returns false for no match
- returns false for duplicate matching entries

The snapshot builder itself preserves input order and includes duplicate IDs if the supplied action list contains them.

## Future Usage

Future rules/runtime code can generate legal actions, build a public board summary, then build this presentation snapshot. Future UI can display the snapshot entries and pass the chosen action id to `WBRuntimeActionSelectionBridge` for execution through the runtime controller facade.

`UWBRuntimeTurnInteractionModelComponent` now stores and exposes these snapshots for future UI consumption. The model owns no rules truth; it only caches externally supplied legal actions and public presentation data until a future owner refreshes them.

`WBRuntimeInteractionRefreshAdapter` can now rebuild these snapshots at runtime decision points by feeding externally supplied legal actions and the current public board summary into the turn interaction model.

This remains C++-only and does not add input, UI widgets, camera behavior, animation, VFX, assets, Blueprints, UMG, `.uasset`, or `.umap` work.
