# Card Effect Request Scaffolding Audit

## Scope

This audit inspected the Godot reference effect-resolution shape before adding Unreal card-effect request scaffolding. The Unreal pass is limited to deterministic request transport for generic operations already implemented in C++. It does not import Godot CardDB, add card activation timing, create target-selection UI, open response windows, run passives, or implement card-specific effects.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/handlers/modify_stat_handler.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effects/effect_registry.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/CardDB.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

Search terms included `EffectRunner`, `resolve_card_effects`, `apply_effect`, `effect type`, `effects`, `activations`, `target_unit_id`, `chosen_unit_id`, `chosen_tile`, `modify_stat`, `restore_armor`, `armor`, `status`, `heal`, `damage`, `wall_remove`, `wall_place`, `terrain`, and `CardDB`.

## Godot Effect Shape

Godot cards carry effect dictionaries under root `effects` and activation `activations[].effects`. `CardDB.gd` normalizes these arrays as card metadata, but the runtime effect resolution remains dictionary-based.

`EffectRunner.resolve_card_effects(gs, card, ctx, picks)` is the central execution path. It copies context per effect, annotates `_effect_path`, normalizes tags, reads `type`, rejects invalid or unsupported effects, then dispatches either through `EffectRegistry` or through legacy `match` handlers in `effect_runner.gd`.

Registry-backed effects validate first, then produce events or directives through `EffectRegistry.apply`. Legacy effects directly call helper handlers. This confirms that Unreal should keep effect mutation inside `EffectRunner`, not inside UI or action generation.

## Source Representation

Godot source information is gathered from the effect context:

- `caster_player` / `active_player`
- `caster_unit_id`
- `card_id`

`_effect_source_from_ctx` returns source fields such as `source_player_id`, `source_unit_id`, and `source_card_id`, with effect-specific extras. In Unreal, `SourceCardId` and `SourceEffectId` are fixture/debug metadata only for this pass. They are not used to load card data and are not emitted in replay traces or public summaries.

## Target Representation

Godot target selection is split between effect dictionaries and `picks`:

- unit picks commonly use `chosen_unit_id` and `chosen_unit_id_2`
- tile picks use selectors such as `chosen_tile`, `from_tile`, and `to_tile`
- wall and edge effects use dedicated wall/edge helpers
- event reactions can resolve `event_target_unit`, `event_attacker`, `event_defender`, and similar canonical event payload fields

Unreal request scaffolding mirrors this as explicit target refs only:

- target unit id
- target tile
- target wall edge

No UI target selection is implemented in this pass.

## Dispatch Findings

Godot supports a broad vocabulary including damage, healing, statuses, stat modification, walls, terrain, movement, negation, prevention, and several card-specific handlers. This pass does not port that vocabulary directly.

Already-existing Unreal generic operations include:

- armor modification through `WBArmorEffect` and `WBEffectRunner::ApplyArmorEffect`
- damage resolution through `WBDamageResolution`
- death cleanup through `WBDeathResolution` and zero-HP cleanup
- start/end status tick helpers
- attack declaration and attack damage paths

The only supported generic effect request payload in this pass is:

- `armor_effect`

## Armor Reference

Godot's `modify_stat_handler.gd` maps `restore_armor` to `current_armor` and maps the `armor` stat alias to `current_armor`. It emits a `modify_stat` directive with source player/unit/card metadata and an optional `restore_current_by_delta` flag. Unreal already has generic armor operations from the prior pass, so this pass routes `armor_effect` payloads to that existing implementation instead of importing CardDB entries.

## Deferred Work

Godot CardDB import is deferred because card ownership, activation timing, hidden zones, card-specific validation, and data normalization need a separate audit and importer design.

UI activation windows are deferred because effect requests are not player-selectable legal actions in this pass. UI will eventually select targets and submit explicit runtime commands, but the rules kernel needs deterministic request transport first.

Response windows, negation, passives, wands, death triggers, discard movement, and card-specific replacement/prevention are deferred because they change timing and priority semantics beyond generic request dispatch.

## Hidden Information Policy

Effect request traces and runtime/public summaries must not leak deck, hand, discard, private choices, hand indices, raw CardDB payloads, source card ids, or source effect ids. Public summaries may continue to expose visible board unit card ids, matching the existing public board summary policy.

This pass uses `SourceCardId` and `SourceEffectId` only as fixture/debug metadata stored on the request object. They are not used for lookup and are not serialized into machine traces.
