# Runtime Legal Action Presentation Snapshot Audit

## Action ID Behavior

Stable action IDs are produced by `WBActionCodec::MakeActionId`.

Current formats:

- Move: `move:p{player}:u{unit}:{from_x},{from_y}->{to_x},{to_y}`
- Attack: `attack:p{player}:u{source}:t{target}`
- EndTurn: `end_turn:p{player}`
- Pass: `pass:p{player}`
- PassResponse: `pass_response:p{player}`

Action IDs do not include runtime context, MP rolls, controller state, visual state, or hidden information.

## Action-Selection Bridge Behavior

`WBRuntimeActionSelectionBridge` consumes an externally supplied legal action list and a selected action ID. It scans only the supplied list, resolves exactly one matching `FWBAction`, and delegates that action to `UWBRuntimeControllerFacadeComponent`.

The bridge does not generate legal actions, decide legality, inspect hidden state, or own `FWBGameStateData`.

## Public Board Summary Fields

`FWBPublicBoardSummary` exposes public board dimensions, default terrain, public units, public walls, and public terrain tiles.

Public unit summaries expose:

- unit id
- owner id
- visible card id
- tile position
- public combat/resource fields
- public statuses

The public board summary does not expose deck, hand, discard, private choices, or hidden marker identity.

## Why The Snapshot Consumes Precomputed Legal Actions

Future UI needs stable, displayable entries for legal actions. The rules/action-generation layer remains responsible for producing legal actions. The snapshot only formats the list it receives so future UI can show public labels and action IDs.

This preserves the rule boundary:

- Rules validate legality.
- EffectRunner mutates state.
- UI selects only.
- Runtime visuals and presentation snapshots display public data only.

## Why The Snapshot Does Not Generate Legal Actions

Generating actions in runtime presentation code would duplicate rules behavior and risk diverging from `WBRules`. The snapshot must not call `WBRules::GenerateLegalActions`, `QueryMove`, `CanMoveUnit`, `CanDeclareAttack`, or any other legality path.

The snapshot also must not filter or sort the supplied list. Ordering remains the order provided by the external rules/runtime owner.

## Why The Snapshot Does Not Decide Legality

The snapshot does not know if an action is legal now, stale, or from a future UI cache. It trusts that another owner supplies the correct list. It only derives presentation fields from `FWBAction` values and the public board summary.

## Public Label Policy

Labels are future UI-facing text and must use simple public terms:

- `Move`
- `Attack`
- `End Turn`
- `Pass`
- `Action` for unknown action types

Labels must not include raw schema fields, player ids, coordinates, internal hook names, `Rules`, `EffectRunner`, or other implementation details. Structured fields may still expose action ids, unit ids, and tiles for future UI/controller use.

## Hidden-Information Boundary

The snapshot receives only precomputed actions and `FWBPublicBoardSummary`. It must not accept or store `FWBGameStateData`, deck, hand, discard, private choices, marker state, or hidden marker identity.

Source and target card ids may be copied only from public unit summaries. If a unit is missing from the public summary, the snapshot must leave the public-card fields empty and the public-unit flags false.

## Intentionally Not Implemented

- input
- UI widgets
- tile selection
- unit selection
- camera behavior
- animation
- VFX
- sound
- marker visuals
- hidden marker identity
- assets
- Blueprints
- UMG
- `.uasset` edits
- `.umap` edits
- gameplay ownership
- legal action generation
- legality decisions
- JSON serialization
