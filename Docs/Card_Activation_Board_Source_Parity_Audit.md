# Card Activation Board Source Parity Audit

## Scope

This audit supports fixture-only Board-source activation ownership and card-id parity checks. It does not add production card zones, import Godot CardDB, mutate hand/deck/discard/equipped state, merge activation into `FWBAction`, alter `WBActionCodec`, or add UI/response windows.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/shared/action_schema.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Board Unit Identity

Godot stores board units in `GameState.units`. Each unit dictionary contains `id`, `owner_id`, board coordinates, stats, and `card_id`. `game_state.gd` documents `card_id` as the value the board uses to draw a unit as a card texture. `_spawn_unit` writes the spawned card id to the unit dictionary, and `apply_card_unit_data` replaces or normalizes it from CardDB data.

Godot board unit card identity is therefore public board-unit identity, separate from private hand/deck/discard arrays.

## Godot Activation Sources

`rules.gd:enumerate_player_actions` distinguishes two normal board activation sources:

- board unit activations use the source unit dictionary's `card_id`
- equipped/wand activations iterate the same unit's `equipped_wands` array and use the equipped wand card id

Both use `ActionSchema.make_activate_action(player_id, unit_id, card_id, activation_index, ...)`. Board-source activations pass the source unit's own `card_id`; equipped activations pass the equipped wand id.

Godot's apply path also validates that the activation unit exists and belongs to the acting player before resolving an activate action. Non-wand activations then derive the activation card through `get_unit_activation_card`, which uses the unit's `card_id` unless a requested non-unit source is explicitly supplied.

## Ownership And Card-Id Parity

Godot board-source activations naturally derive ownership from the source unit owner. The enumerated board activation card id is read from the same unit dictionary being activated, so the unit id and source card id are paired by construction during legal action generation.

The audit did not identify a generic production deck/hand/discard activation lane that should be imported for this pass. CardDB remains a reference for future import work only.

## Current Unreal State

Unreal `FWBUnitState` has a public `CardId` field and `FWBGameStateData::GetUnitById`. `FWBUnitState::IsUnitOnBoard` already excludes defeated, removed, and invalid-position units. `WBPublicBoardSummary` includes visible unit `CardId` by current public-summary policy.

Card activation candidate generation already filters through `WBCardActivationSourceGate`, and fixture source-zone ownership currently supports Hand, Equipped, Discard, Deck unsupported, and legacy Fixture behavior.

## Chosen Fixture-Only Board Policy

Board-source parity uses the actual `FWBUnitState::CardId` on the visible board unit as source of truth. It does not require a `FixtureZoneContext` entry, because duplicating public board state in fixture metadata would create two authorities for the same board source.

For Board source gates with `bRequiresFixtureZoneOwnership`:

- `SourceCardId` must be non-empty.
- `SourceUnitId` must be valid.
- the source unit must exist.
- the source unit must be on board and not defeated/removed.
- if ownership checks are enabled, source unit owner must match the context player.
- the source unit `CardId` must be non-empty.
- the source unit `CardId` must equal `SourceCardId`.

Hand, Equipped, Discard, Deck, and Fixture behavior remains unchanged.

## Deferred Work

Production CardDB import is deferred because activation definitions are still fixture-owned C++/JSON data. Real card zones, card draw, discard movement, equip movement, wand attachment, cost payment changes, response windows, effect negation, passives, and UI target selection are outside this pass.

## Hidden-Information Policy

Board unit `CardId` is public under the current public board summary policy. Fixture zone metadata remains private test/scaffold data and is not copied into traces, runtime public summaries, or public board summaries. Activation candidate ids may include source card id as external selection metadata; `FWBAction` ids and `WBActionCodec` remain unchanged.
