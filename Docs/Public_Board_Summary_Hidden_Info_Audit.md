# Public Board Summary Hidden Info Audit

Date: 2026-06-17

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/ai/ai_env.gd`
- `Reference/GodotProject/godotcanon/scripts/ai/data/decision_snapshot_builder.gd`
- `Reference/GodotProject/godotcanon/scripts/ai/replay_logger.gd`
- `Reference/GodotProject/godotcanon/scripts/game/game_match_log_controller.gd`

## Godot Public Unit Identity

Godot AI observation builds public unit summaries in `scripts/ai/ai_env.gd`.

`_public_unit_summary` includes:

- unit id
- owner id
- `card_id`
- tile position
- HP/max HP
- ATK/AR
- attacks left
- RL fields
- status ids

Godot match-state snapshots also include board unit `card_id`.

Conclusion: visible board unit identity is public. Unreal public board summaries may include visible unit `CardId`.

## Hidden Marker Identity

Godot `GameState` stores hidden tile markers with:

- kind
- owner
- active/spent
- revealed flags
- `card_id`
- `revealed_card_id`

Godot public marker summaries include marker identity only when the marker belongs to the viewing player or has been revealed to that player. Otherwise the marker kind is reported as `hidden`, and card identity is omitted.

Conclusion: hidden marker card identity is not public and must not appear in public board summaries.

## Deck, Hand, And Discard

Deck and hand contents are private.

Godot player observations expose deck and hand counts broadly and hand details only to the owning player. Discard contents appear in Godot observations, but this Unreal pass is a board summary, not a full public game observation.

Conclusion: deck and hand contents are excluded. Discard contents remain out of this board summary pass unless explicitly requested later.

## Current Unreal State Support

Current Unreal state has:

- `FWBUnitState::CardId`
- public board unit combat fields
- public board unit statuses
- `FWBPlayerStateData::Deck`
- `FWBPlayerStateData::Hand`
- `FWBPlayerStateData::Discard`

Current Unreal state does not yet have:

- marker structs
- hidden marker identity
- revealed marker identity
- pending choices in core state

## Chosen Public Board Summary Policy

Include visible board units only.

Include visible unit `CardId`, because Godot treats visible board unit card identity as public.

Exclude hidden marker identity entirely. Since Unreal has no marker state yet and this summary is not player-perspective scoped, no marker fields are serialized in this pass.

## Fields Included

Board:

- board width
- board height

Units:

- unit id
- owner id
- visible unit `CardId`
- x/y tile position
- HP/max HP
- ATK/AR
- RL total/used
- attacks left
- public status ids and turns remaining

## Fields Excluded

Excluded:

- deck contents
- hand contents
- discard contents
- hidden marker identity
- hidden marker card IDs
- pending choices
- private response options
- passives
- equipped cards/wands
- raw internal object pointers/resources
- file paths
- terrain and wall summaries

## TODOs

- Add marker summaries only after core marker state exists.
- Add player-perspective revealed marker summaries separately from global board summaries.
- Add NPC/trap marker tests once Unreal owns marker state.
- Add public discard summaries only after a separate discard visibility audit.
- Add public wall and terrain summaries in their own scoped passes.
