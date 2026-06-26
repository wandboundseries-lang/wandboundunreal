# Card Activation Cost Gates Audit

## Godot Files Inspected

- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/effect_runner.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/`

## Findings

Godot represents Resonance Requirement as `rr`, with `rl_cost` accepted as a compatibility fallback in `Rules.card_rr`. Unit RL capacity is stored as `rl_total`, reserved RL is stored as `rl_used`, and `Rules.available_rl` computes effective RL total minus effective `rl_used`, clamped at zero.

Wand equip uses RR as a hard affordability gate. `GameState.equip_wand_to_unit` checks `Rules.available_rl(self, target_unit_id) < rr` before equip unless explicitly ignored, and increments the target unit's `rl_used` after successful equip. Removing an equipped wand subtracts that wand RR from `rl_used`.

Godot effect and hand-action paths also inspect card RR for affordability. `Rules.default_caster_unit_id_for_rr` chooses an active unit with enough available RL, and response/hand effect legality checks `available_rl` before building or applying effect activation payloads. CardDB quick/react/equip data contains many `rr` fields, and CardDB normalization preserves activation/effect data, but Unreal does not import CardDB yet.

Activated unit and wand abilities are represented in Godot as `activations` on card data or copied runtime activation data. Current activation logic gates active unit state, target legality, once-per-turn usage, and response flow. This pass did not find a general production card activation path that should be ported as payment now. Some specific activations have card-specific costs, such as Red Ledger Medic HP payment, which are out of scope for generic RR/RL scaffolding.

Godot opens response windows around pending effect activations and supports negation hooks. Costs and response timing are intertwined with hand/effect activation flows, pending payloads, and UI/runtime orchestration. This Unreal pass intentionally avoids deciding whether costs are paid before declaration, before resolution, or after negation because that belongs with the future production card zone/payment model.

Overflow exists in the Godot runtime. `autoload/Game.gd` checks RL overflow after equip and other RL changes, and presents a UI flow to destroy wands until used RL fits available RL. That behavior is deferred.

## Current Unreal Surface

Unreal already has `FWBUnitState::RLTotal` and `FWBUnitState::RLUsed`, but no production zone model, CardDB importer, payment system, overflow system, equipped wand fallout, or activation `FWBAction` family. Existing activation candidate generation is fixture-owned and routes effects through `WBCardActivationSourceGate`.

## Chosen Unreal Design

This pass adds a fixture-only cost gate to `FWBCardActivationSourceGateDefinition` and an external affordability context to `FWBCardActivationSourceGateContext`.

The gate can require external affordability, carry an expected `RequiredRR`, and store a metadata-only `CostKind`. The context can declare whether affordability was supplied, whether the caller judged the activation affordable, the supplied RR requirement, the supplied available RL, and metadata-only cost kind.

The source gate does not inspect unit RL/RR fields directly for payment, does not mutate `RLUsed`, does not destroy wands, does not produce traces, and does not leak hidden zones. Candidate generation simply filters unaffordable fixture effects because it already asks the source gate for each effect.

The evaluator also performs one deterministic consistency check: if the fixture supplies a positive required RR and supplied available RL is lower than that supplied requirement, the gate fails with `cost_not_affordable` even if the external boolean says affordable. This catches malformed fixture affordability data without implementing payment.

## Deferred

- production zones
- actual RR/RL payment
- `RLUsed` mutation
- overflow and wand destruction
- equipped wand fallout
- CardDB import
- response windows
- negation
- card-specific timing
- UI target selection
- passives and wands
