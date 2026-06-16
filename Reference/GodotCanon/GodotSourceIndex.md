# Godot Source Index

Godot project root: `Reference/GodotProject/godotcanon/`

This index maps the Godot behavior reference to future Unreal C++ ownership. The Godot project is read-only. Use it to port behavior, not architecture.

## Canon Files

| Reference | Path | Use |
| --- | --- | --- |
| Godot project marker | `project.godot` | Confirms project root and autoloads. |
| Rules Bible | `Wandbound_Rules_Bible_v2.txt` | Canonical text rules. If this conflicts with code, ask the user. |
| Glossary | `Wandbound_Canonical_Glossary.txt` | Canonical terms and naming. |
| Game orchestration | `autoload/Game.gd` | High-level flow reference only, not rules truth. |
| Board/state | `scripts/sim/game_state.gd` | Board dimensions, unit state, walls, terrain, statuses, card zones. |
| Rules/legal actions | `scripts/sim/rules.gd` | Legality queries and legal action generation. |
| Effects | `scripts/sim/effect_runner.gd` | Effect resolution and state mutation behavior. |
| Combat | `scripts/sim/combat_runner.gd` | Combat sequencing and combat outcomes. |
| Actions | `scripts/shared/action_schema.gd` | Dictionary action contract for UI, AI, and simulation. |
| Card data | `scripts/data/CardDB/` | Source card database and future importer input. |

## Unreal Mapping

| Area | Godot source | Future Unreal target | Porting note |
| --- | --- | --- | --- |
| Board/state | `scripts/sim/game_state.gd` | `Source/WandboundCore/` | Port as deterministic C++ state structs and board helpers. |
| Rules/legal actions | `scripts/sim/rules.gd` | `WBRules` | Rules validate legality and generate legal actions. Rules do not mutate state. |
| Effects | `scripts/sim/effect_runner.gd` | `WBEffectRunner` | EffectRunner mutates state after legality and target resolution. |
| Combat | `scripts/sim/combat_runner.gd` | `WBCombatRunner` | Keep combat deterministic and testable in headless C++. |
| Actions | `scripts/shared/action_schema.gd` | `WBAction` / `WBActionCodec` | Preserve a stable serializable action contract for tests and replay. |
| Game orchestration | `autoload/Game.gd` | Later runtime/controller layer only | Use as flow reference. Do not make this layer rules truth. |
| Card data | `scripts/data/CardDB/` | Future card data importer | Import data into Unreal-owned formats. Do not runtime-load Godot files. |

## Movement Reference Seeds

Starter movement scenarios are in `GoldenScenarios/`. They are intentionally small so `WandboundCore` tests can assert legality reasons and state transitions before presentation work.
