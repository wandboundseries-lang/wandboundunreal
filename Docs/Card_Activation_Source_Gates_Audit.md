# Card Activation Source Gates Audit

## Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Godot Behavior Summary

Godot action enumeration keeps activation legality in rules/orchestration code rather than in UI. `rules.gd` routes normal player action generation through owned units, then checks source usability before adding activation-like actions.

Source ownership is validated when activate actions are applied: the source unit must exist and match the acting player. Wand and unit activations also check whether the source unit can use that active.

Godot zones are real runtime state: hand actions are queried through `query_hand_actions`, unit activations come from board unit cards, and wand activations come from equipped wands. This Unreal pass does not port those production zones. It uses fixture-provided `SourceZone` metadata only.

Timing is split between normal action enumeration and response enumeration. Normal player actions require the active/priority player context. Response windows exist in Godot, but Unreal activation response windows are not integrated yet, so `ResponseWindow` fails explicitly.

Stunned blocks active use through the physical-action gate. Frozen also blocks Godot active use in the current reference path. Unreal keeps legacy default candidate filtering for Frozen, but explicit fixture source gates can choose `bBlockedByFrozen=false` so future policy tests can isolate the gate from old candidate defaults.

Godot cost/RR/RL behavior is card- and zone-driven. Hand actions use cost checks before being exposed, and activation application stages pending effects with RR/RL metadata. This Unreal pass does not pay or mutate costs; it only honors a fixture-provided `bCostsSatisfiedExternally` flag.

Godot tracks once-per-turn usage with helper counts/marks for unit and wand activations. Unreal adds fixture usage keys in `FWBGameStateData` and clears a player's keys during turn-start resource setup, but candidate generation does not mark usage.

## Chosen Unreal Fixture Gate Design

Added `FWBCardActivationSourceGateDefinition` and `FWBCardActivationSourceGateContext`.

The gate checks:

- valid player id
- game-over state
- fixture source zone metadata
- normal-turn priority timing
- unsupported response-window timing
- required source unit existence
- removed/defeated source state
- optional source ownership
- Stunned/Frozen source policy
- fixture-provided cost-satisfied flag
- fixture once-per-turn usage keys

Candidate generation evaluates each activated effect's source gate against its source context. A failed gate skips that effect/source and continues; malformed card/source structure can still fail generation.

## Deferred

- production CardDB import
- real hand/deck/discard/equipped zone legality
- real RL/RR cost payment or mutation
- marking once-per-turn usage during real activation execution
- response windows
- effect negation
- card-specific timing
- UI target selection
- passives and wands
