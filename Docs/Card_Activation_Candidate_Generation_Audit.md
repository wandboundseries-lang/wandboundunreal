# Card Activation Candidate Generation Audit

## Scope

This audit precedes fixture-driven Unreal card activation candidate generation.

The target pass adds deterministic Unreal-owned candidate generation from `FWBCardDefinition` data and supplied targets. It does not import Godot CardDB, production-load card JSON, add player-facing activation legal actions, open UI target selection, or implement response/negation behavior.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/shared/action_schema.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/CardDB.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Activation Discovery

Godot stores card data in CardDB dictionaries keyed by card id. CardDB normalizes root `effects`, activation `effects`, passive payloads, effect types, targets, status ids, terrain ids, stat ids, and rule ids.

Godot action enumeration in `rules.gd` includes unit and equipped-wand activations as regular action dictionaries with `kind: "activate"`. For active-player enumeration:

- it skips response windows and stale active/priority players
- it iterates units owned by the player
- it enumerates move and attack actions first
- it then enumerates unit activations from the unit card's `activations` array
- it then enumerates equipped wand activations from each equipped wand card's `activations` array
- it uses `ActionSchema.make_activate_action`

`ActionSchema.make_activate_action` includes:

- `kind`
- `player_id`
- `unit_id`
- `card_id`
- `activation_index`
- `activation`
- `requires`
- `picks_schema`
- `picks`
- `score`

## Godot Source Unit/Card Legality

Godot uses `can_unit_use_unit_active` for unit activations and `can_unit_use_wand_active` for wand activations.

These gates require the unit to be able to take physical actions. In the inspected code, `unit_can_take_physical_actions` blocks frozen/stunned/negated active and passive sources. Godot comments also state that frozen/stunned/negated aura sources provide no passive effects.

Godot checks once-per-turn and once-per-turn-per-copy counters for relevant unit/wand activations.

Godot activation application also revalidates:

- activating unit exists
- activating unit belongs to the player
- source card/payload exists
- unit/wand active gate
- required picks are present
- picked unit targets are legal
- activation use counters

## Godot Target Handling

Godot derives required picks from effect definitions. Unit target requirements include selectors such as:

- `chosen_friendly_unit`
- `chosen_enemy_unit`
- `chosen_any_unit`
- `chosen_unit`
- `chosen_target_unit`
- `target_unit`
- secondary variants ending in `_2`

Tile and wall requirements are inferred from effect types such as move/teleport/terrain/wall effects.

For candidate availability, Godot checks target families through helpers such as:

- `_effect_required_picks`
- `_activation_targets_legal`
- `_compute_effect_unit_targets`
- `query_effect_unit_targets`
- `query_effect_tile_targets`
- `query_wall_effect_edges`

## Stun And Frozen

Godot blocks active use through its physical-action gate. Stunned and Frozen both prevent unit/wand active activations in the inspected code.

For this Unreal pass, Stunned and Frozen sources are excluded from generated activation candidates. This is stricter than the earlier attack-damage Frozen-break path, but matches Godot active-use gating.

## Response And Negation

Godot activation application does not immediately resolve effects. It writes `pending_effect_activation` and opens an `effect_activation_pending` response window. Response actions can then negate or otherwise affect the pending activation. EffectRunner later resolves pending card effects if the activation is not negated.

This Unreal pass does not implement response windows, negation, or pending activation state. It only generates explicit candidates whose commands can be applied by tests through `WBEffectRunner::ApplyCardActivationCommand`.

## Godot Activation Action IDs

Godot represents activations as dictionary actions, not as Unreal-style `WBActionCodec` ids. The activation dictionary contains `kind: "activate"`, the source card id, activation index, requires, picks, and activation payload.

This pass intentionally does not add activation ids to `WBActionCodec`.

## Existing Unreal Scaffolding

Already present:

- `FWBCardDefinition`
- `FWBCardEffectDefinition`
- `FWBCardActivationCommand`
- `WBCardActivationExpansion`
- `WBEffectRunner::ApplyCardActivationCommand`
- generic `FWBEffectRequest` payload dispatch for armor/status/damage/heal

## Chosen Unreal Candidate Shape

This pass will add `FWBCardActivationCandidate` and `WBCardActivationCandidateGenerator`.

Inputs are explicit Unreal-owned fixture definitions:

- player id
- optional source unit id
- `FWBCardDefinition`
- candidate targets

Outputs are candidates containing:

- deterministic internal candidate id
- player/source metadata
- public label
- target
- `FWBCardActivationCommand`

Generation order is deterministic:

1. supplied source order
2. `ActivatedEffects` order
3. supplied target order

Dynamic invalid entries are excluded rather than making the whole result fail:

- removed/defeated source units
- Stunned/Frozen source units
- missing/removed/defeated unit targets
- target mismatch build failures

Malformed input fails the whole result:

- invalid player id
- missing required source unit
- missing card id
- malformed/ambiguous effect id
- empty payloads
- unknown payload operations

## Why Candidates Stay Separate From FWBAction

Godot eventually treats activations as legal player actions, but Unreal is not ready for that boundary yet. This pass creates future-safe candidates without changing Move/Attack/EndTurn/PassResponse generation, replay action ids, or `WBActionCodec`.

## Deferred Work

CardDB import is deferred because current Unreal definitions are fixture-owned and intentionally small.

UI target selection is deferred because candidate targets are supplied externally in this pass.

Response windows and negation are deferred because current Unreal activation commands resolve explicitly through the existing effect runner path and no pending activation state exists yet.

Hand/deck/discard zones, costs, RR/RL payments, once-per-turn gates, passives, wands, counters, draw, death triggers, and card-specific replacement/prevention behavior remain out of scope.

## Hidden Information Policy

Candidates may carry source card/effect ids as internal fixture/debug metadata. They must not serialize into public traces, public board summaries, or runtime public summaries. Candidate ids are internal selection metadata and must not be used as public trace ids.
