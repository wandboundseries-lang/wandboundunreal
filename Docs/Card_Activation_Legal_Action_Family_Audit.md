# Card Activation Legal Action Family Audit

## Scope

This audit precedes the Unreal-only card activation legal action family pass.

The target pass converts already-computed `FWBCardActivationCandidate` values into UI/runtime-ready activation entries while keeping them separate from `FWBAction`, `WBRules::GenerateLegalActions`, and `WBActionCodec`.

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/shared/action_schema.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Activation Representation

Godot represents activations as dictionary actions with `kind: "activate"` through `ActionSchema.make_activate_action`.

The activation action shape includes:

- `player_id`
- `unit_id`
- `card_id`
- `activation_index`
- `activation`
- `requires`
- `picks_schema`
- `picks`
- `score`

Godot does not use Unreal-style `WBActionCodec` IDs for activation actions. The action dictionary itself carries source card and activation index metadata.

## Godot Legal Action Generation

`rules.gd` enumerates player actions in this broad order:

- movement
- attacks
- unit activations
- equipped wand activations
- hand/card actions
- wall actions
- end turn/pass style actions in later flow

Unit activations are gated through `can_unit_use_unit_active`. Wand activations are gated through `can_unit_use_wand_active`. Both depend on the physical action gate, and Godot blocks frozen/stunned/negated active sources.

Target availability is checked through helpers such as `_activation_targets_legal`, `_compute_effect_unit_targets`, `query_move_within_range_tiles`, and `query_wall_effect_edges`.

## Godot Activation Resolution Boundary

Godot separates activation declaration from effect resolution. Applying an activation dictionary stages `pending_effect_activation` and opens an `effect_activation_pending` response window. Negation and response hooks can modify or cancel that pending activation before `effect_runner.gd` resolves it.

This Unreal pass intentionally does not model that pending-response boundary. Existing Unreal scaffolding applies the command explicitly through `WBEffectRunner::ApplyCardActivationCommand` only when a test or future runtime owner chooses to do so.

## Existing Unreal Scaffolding

Already present:

- `FWBCardDefinition`
- `FWBCardEffectDefinition`
- `FWBCardActivationCandidate`
- `FWBCardActivationCommand`
- `WBCardActivationExpansion`
- `WBCardActivationCandidateGenerator`
- `WBEffectRunner::ApplyCardActivationCommand`

## Chosen Unreal Shape

This pass adds a separate activation legal action family:

- `FWBCardActivationLegalAction`
- `FWBCardActivationLegalActionSet`
- `FWBCardActivationLegalActionGenerationResult`
- `WBCardActivationLegalActionGenerator`
- activation-only presentation snapshot helpers

The generator takes `FWBCardActivationCandidate` values as input, preserves supplied order, copies the wrapped command, and uses the candidate id as the activation action id.

Malformed candidates fail clearly:

- empty candidate id
- missing command payloads
- unknown command payload operation
- unsafe public label

## ID Policy

Activation action ids use `FWBCardActivationCandidate::ActivationCandidateId` directly. The expected fixture ids begin with `activate:`, but the generator accepts non-`activate:` candidate ids because the id namespace is owned by the candidate source in this pass.

These ids are not `WBActionCodec` ids.

## Label Policy

Activation legal actions use `Candidate.PublicLabel` when present, otherwise `Activate`.

Labels must not expose raw internal names such as `damage_effect`, `heal_effect`, `armor_effect`, `status_effect`, `EffectRunner`, `Rules`, raw schema names, or hook names.

## Why Activation Remains Separate From FWBAction

Godot eventually treats activation as a normal action kind, but Unreal is not ready to merge activation into the canonical `FWBAction` contract yet. Keeping this as a separate family preserves:

- existing move/attack/end-turn/pass action generation
- replay verifier assumptions for `FWBAction`
- `WBActionCodec` action id stability
- runtime selected-action bridge behavior

## Deferred Work

Deferred intentionally:

- Godot CardDB import
- production card JSON loading
- hand/deck/discard ownership and zone legality
- activation costs and RR/RL payments
- once-per-turn gates beyond candidate scaffolding
- UI target selection
- pending response windows
- effect negation
- passives
- wands
- card-specific behavior

## Hidden Information Policy

The activation legal action object may carry fixture-visible `SourceCardId` and `SourceEffectId` metadata for future UI/debug use, but these fields must not be emitted into replay traces, public board summaries, or runtime public summaries.

The only effectful boundary remains explicit application of `FWBCardActivationCommand`; generation itself does not mutate state or emit traces.
