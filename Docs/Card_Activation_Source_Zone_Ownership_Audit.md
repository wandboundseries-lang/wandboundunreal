# Card Activation Source-Zone Ownership Audit

## Scope

This audit supports fixture-only activation source-zone ownership gates for Hand, Equipped, and Discard. It does not create production card zones, import Godot CardDB, inspect real Unreal deck/hand/discard zones, add activation to `FWBAction`, alter `WBActionCodec`, or add UI/response windows.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/shared/action_schema.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Zone Model

Godot stores card zones on `GameState` as player arrays:

- `hands`
- `decks`
- `discards`

Board units are stored separately in `units`. Equipped wands are stored on unit dictionaries through `equipped_wands`, with companion owner/sequence arrays such as `equipped_wand_owners`.

`ActionSchema.make_activate_action` represents activation as a dictionary with `kind = "activate"`, `player_id`, `unit_id`, `card_id`, `activation_index`, `requires`, picks, and activation payload data.

## Godot Activation Sources

`rules.gd:enumerate_player_actions` enumerates normal-turn activations from:

- the active player's board units, using the unit `card_id`
- equipped wands on those units, using the equipped wand card id

The enumeration is gated by active player and priority player checks before unit actions are produced. The equipped-wand path checks the unit's equipped wand array and emits activate actions with the same source unit id.

Hand, deck, and discard are represented and manipulated elsewhere for summons, casts, reactions, card search, draw/discard movement, and card-specific effects. This audit did not find a generic normal-turn deck activation path that should be ported into this scaffold.

## Ownership And Equipped Behavior

Godot's unit activations naturally derive ownership from the source unit owner. Equipped wand activation derives the source unit and card id from the unit's `equipped_wands`. Equipped wand owner metadata exists separately in `equipped_wand_owners`, and removed equipped wands are moved to the recorded owner's discard.

Hand actions in Godot are tied to the current player's hand arrays and timing/cost/target checks. Discard interactions exist but appear card-specific or effect-specific rather than a generic activate-from-discard lane.

## Current Unreal Behavior

Unreal already has fixture-owned activation source gates:

- `FWBCardActivationSourceGateDefinition`
- `FWBCardActivationSourceGateContext`
- `WBCardActivationSourceGate::Evaluate`
- candidate filtering through `WBCardActivationCandidateGenerator`

Before this pass, source gates could check source zone enum, timing, source unit ownership/status, external cost-satisfied flags, detailed cost gates, and once-per-turn usage. They did not have explicit fixture-owned card-zone ownership metadata.

## Chosen Fixture-Only Design

This pass adds explicit fixture metadata:

- `FWBCardActivationFixtureZoneEntry`
- `FWBCardActivationFixtureZoneContext`
- `FWBCardActivationSourceGateDefinition::bRequiresFixtureZoneOwnership`
- `FWBCardActivationSourceGateContext::SourceCardId`
- `FWBCardActivationSourceGateContext::FixtureZoneContext`

The fixture context is only test/scaffold data. It is not a production zone store and is not serialized into public summaries or traces.

The ownership matcher requires exactly one entry matching source card id, player id, and required zone. Equipped entries additionally require `EquippedToUnitId` to match the source unit id; existing source-unit checks still validate that unit against `FWBGameStateData`.

## Zone Decisions

- Hand: fixture entry must match source card, player, and Hand.
- Equipped: fixture entry must match source card, player, Equipped, and source unit id.
- Discard: fixture entry must match source card, player, and Discard.
- Board: supported only as fixture metadata and should normally be paired with source-unit gating or documented card-specific behavior later.
- Deck: explicitly unsupported for activation in this scaffold.
- Fixture: preserves legacy fixture behavior and does not require fixture-zone entries.

## Deferred Work

Production zones are deferred because the project does not yet have a production CardDB importer, production card-zone state, draw/discard/equip movement, or real activation windows. Adding those here would mix fixture legality with production hidden-information ownership.

CardDB import is deferred because current activation definitions are Unreal-owned fixture data and the Godot project is read-only reference material.

UI target selection and response windows are deferred because the rules kernel still treats activation candidates and activation legal action families as separate from player-facing `FWBAction` legal generation.

## Hidden-Information Policy

Fixture zone entries may contain private card ids for tests. Those entries are not copied into `FWBGameStateData`, replay traces, runtime public summaries, or public board summaries. Activation candidate ids remain external selection metadata and may include fixture card ids; they are not `WBActionCodec` ids and are not replay trace/public summary fields.
