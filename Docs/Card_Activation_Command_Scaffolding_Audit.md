# Card Activation Command Scaffolding Audit

## Scope

This pass audits Godot card/effect activation shape before adding Unreal scaffolding. The Unreal implementation should consume externally supplied effect-request data and delegate deterministic mutation to `WBEffectRunner::ApplyEffectRequest`.

Out of scope: CardDB import, card-specific behavior, activation legal action generation, UI target selection, response windows, negation, deck/hand ownership checks, costs, Blueprints, assets, and presentation.

## Godot Reference Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/shared/action_schema.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Activation Shape

Godot routes card/effect activation through `Game.gd` orchestration and resolves effect behavior through `EffectRunner.resolve_card_effects`.

Observed source metadata:

- `caster_player`
- `caster_unit_id`
- `card_id`
- card dictionary data
- effect tags/action identifiers in activation payloads

Observed target and pick metadata:

- `chosen_unit_id`
- `chosen_unit_id_2`
- `chosen_tile`
- `edge`
- `from_edge`
- `to_edge`

Godot builds an effect context with `caster_player`, `caster_unit_id`, and `card_id`, then passes a `picks` dictionary into `EffectRunner.resolve_card_effects`. `effect_runner.gd` derives source references from that context through helpers such as `_caster_player_id`, `_caster_unit_id`, and `_effect_source_from_ctx`.

## Activation And Response Boundary

Godot has flows that store `pending_effect_activation`, emit `effect_activation_pending`, and open response windows before eventual resolution. The relevant resolution function still ends by constructing context/picks and calling `EffectRunner.resolve_card_effects`.

For this Unreal pass, that pending/response layer is intentionally deferred. Unreal will add only a deterministic command wrapper around an already selected, externally supplied `FWBEffectRequest`.

## Existing Unreal Effect Request Shape

Unreal already has deterministic effect request support for:

- armor
- status
- damage
- heal

`WBRules::CanApplyEffectRequest` validates the request and payloads. `WBEffectRunner::ApplyEffectRequest` applies the request atomically through a working-state copy and emits effect-request traces.

## Chosen Unreal Scaffolding Shape

Unreal card activation scaffolding will add:

- `FWBCardActivationSource`
- `FWBCardActivationCommand`
- `FWBCardActivationCommandResult`

The command source records external fixture/debug metadata, while the effect request remains the only behavior payload. Missing effect-request source fields may be filled from the command source before dispatch.

Validation remains deliberately narrow:

- game is not over
- player exists
- optional source unit exists, is active, and belongs to the player
- source-less/global fixture effects are allowed when `SourceUnitId == -1` and player is valid
- request source player/unit metadata matches the command source or is fillable
- request payloads are non-empty and valid through `CanApplyEffectRequest`

Validation does not prove card ownership, resource costs, timing windows, CardDB definitions, UI target selection, or response availability.

## Trace And Hidden Information Policy

The parent trace kind for successful card activation should be `card_activation_resolved`, followed by child effect-request traces.

Trace-safe fields:

- `PlayerId`
- `SourceUnitId`
- `TargetUnitId`
- `bOk`

The trace must not serialize:

- source card id
- source effect id
- debug activation id
- hand/deck/discard identity
- hidden marker identity

This mirrors the existing Unreal hidden-info boundary: public summaries and replay traces should expose deterministic public state, not private card-zone metadata.

## Deferred Work

Future passes should add activation legal action generation, CardDB-backed effect expansion, costs, response windows, negation, and UI target selection as separate tested layers. This pass only proves the deterministic command-to-effect-request bridge.
