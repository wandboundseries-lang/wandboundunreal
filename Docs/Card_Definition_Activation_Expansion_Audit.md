# Card Definition Activation Expansion Audit

## Scope

This audit reviews the Godot card/effect data shape before adding Unreal-owned fixture card definitions that expand into `FWBCardActivationCommand` candidates.

The implementation target is intentionally narrow:

- fixture-authored Unreal card definitions only
- deterministic expansion into the existing generic effect request command shape
- no Godot CardDB import
- no production JSON card loading
- no legal activation action generation
- no UI, response window, negation, passive, wand, or card-specific behavior

## Godot Reference Shape

The Godot reference CardDB lives under `Reference/GodotProject/godotcanon/scripts/data/CardDB/`.

Observed card data patterns:

- Cards use `id`, `name`, and `category` as public identity fields.
- Character cards define `stats`, optional `activations`, and optional `passives`.
- Action/effect cards define timing-style metadata such as `timing`, `rr`, `subclass`, `restrictions`, and root `effects`.
- Wand equip cards are card definitions with timing/restriction/effect metadata, not standalone rules modules.
- `activations` entries can contain a local `name`, rule flags such as `once_per_turn`, requirements such as `edge`, and an `effects` array.
- Effects are normalized through `CardDB.gd` before runtime use.

Observed effect data patterns:

- Effect dictionaries use a `type` plus optional target or context fields.
- Common Godot effect types include `damage`, `heal`, `apply_status`, `modify_stat`, `cleanse_all`, `set_terrain`, wall operations, draw/prevent/negate/redirect directives, and nested event/passive apply arrays.
- Target strings are semantic selectors such as `target_unit`, `chosen_any_unit`, `chosen_friendly_unit`, `chosen_tile`, `caster_unit`, `caster_player`, `opponent_hero`, or `board`.
- Runtime resolution in Godot combines card metadata, context, and user picks before dispatching effects.

## Unreal Boundary

Current Unreal already has generic effect payloads for:

- armor
- status
- damage
- heal

Current Unreal also has `FWBCardActivationCommand`, which carries:

- source player/unit/card/effect metadata
- one `FWBEffectRequest`
- fixture/debug activation id

This pass should add only a small Unreal-owned definition layer that maps fixture effect definitions into that existing command shape. It should not import Godot definitions or copy the Godot architecture.

## Chosen Mapping

`FWBCardDefinition` will represent a fixture card with public id/name/kind and ordered activated effects.

Each `FWBCardEffectDefinition` will carry:

- effect id
- public label
- target requirement
- ordered generic effect payloads

`WBCardActivationExpansion` will:

- find exactly one effect by requested effect id
- validate simple fixture-level target requirements
- validate that payload operations are among current Unreal generic operations
- build an `FWBCardActivationCommand`
- leave payload target/source ids unchanged when they are authored as `-1`

Leaving payload-level target/source ids unchanged preserves the existing `WBEffectRunner::ApplyCardActivationCommand` behavior, which fills source metadata and lets `ApplyEffectRequest` fill target ids from the request target when appropriate.

## Deferred Behavior

The following Godot concepts remain out of scope:

- CardDB import or production card JSON loading
- card ownership, hand/deck/discard checks, costs, timing, and once-per-turn gates
- legal activation action generation
- player target selection UI
- response windows, negation, prevention, redirect, passives, and wand-specific behavior
- terrain, wall, draw, cleanse, stat modify, event hooks, or card-specific effects beyond current generic fixture payloads

If future Godot CardDB behavior conflicts with the Rules Bible, stop and ask the user before porting.
