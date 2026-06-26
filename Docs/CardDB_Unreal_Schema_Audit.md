# CardDB Unreal Schema Audit

## Scope

This is a documentation-only audit for a future Unreal-owned CardDB schema and activation import mapping. No importer, JSON loader, production CardDB, production zones, source code, tests, fixtures, UI, response windows, Blueprints, `.uasset`, or `.umap` changes were added.

## Files Inspected

Unreal core headers:

- `Source/WandboundCore/Public/WBCardDefinition.h`
- `Source/WandboundCore/Public/WBCardActivationCommand.h`
- `Source/WandboundCore/Public/WBCardActivationExpansion.h`
- `Source/WandboundCore/Public/WBCardActivationCandidate.h`
- `Source/WandboundCore/Public/WBCardActivationCandidateGenerator.h`
- `Source/WandboundCore/Public/WBCardActivationLegalAction.h`
- `Source/WandboundCore/Public/WBCardActivationLegalActionGenerator.h`
- `Source/WandboundCore/Public/WBCardActivationSourceGate.h`
- `Source/WandboundCore/Public/WBCardActivationAffordability.h`
- `Source/WandboundCore/Public/WBCardActivationCostPayment.h`
- `Source/WandboundCore/Public/WBEffectRequest.h`
- `Source/WandboundCore/Public/WBArmorEffect.h`
- `Source/WandboundCore/Public/WBStatusEffect.h`
- `Source/WandboundCore/Public/WBDamageEffect.h`
- `Source/WandboundCore/Public/WBHealEffect.h`
- `Source/WandboundCore/Public/WBTypes.h`

Unreal reports:

- `Docs/Card_Effect_Request_Scaffolding_Report.md`
- `Docs/Card_Activation_Command_Scaffolding_Report.md`
- `Docs/Card_Definition_Activation_Expansion_Report.md`
- `Docs/Card_Activation_Candidate_Generation_Report.md`
- `Docs/Card_Activation_Legal_Action_Family_Report.md`
- `Docs/Card_Activation_Source_Gates_Report.md`
- `Docs/Card_Activation_Source_Zone_Ownership_Report.md`
- `Docs/Card_Activation_Board_Source_Parity_Report.md`
- `Docs/Card_Activation_Affordability_Query_Report.md`
- `Docs/Card_Activation_Cost_Gates_Report.md`
- `Docs/Card_Activation_Cost_Payment_Scaffolding_Report.md`
- `Docs/Runtime_Activation_CardDB_Zone_Provider_Plan.md`
- `Docs/Runtime_Activation_Data_Provider_Interface_Report.md`
- `Docs/Runtime_Activation_Data_Provider_Contract_Report.md`
- `Docs/Wandbound_Unreal_Migration_Plan.md`

Godot reference, read-only:

- `Reference/GodotCanon/README.md`
- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/CardDB.gd`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/effects_quick.json`
- `Reference/GodotProject/godotcanon/scripts/data/CardDB/wands_equip.json`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/data/assets/asset_manifest.schema.json`

## Current Unreal-Owned Card And Effect Scaffolding

`FWBCardDefinition` is the current Unreal-owned card definition shape. It contains:

- `CardId`
- `PublicName`
- `Kind`
- `ActivatedEffects`

`FWBCardEffectDefinition` contains:

- `EffectId`
- `PublicLabel`
- `TargetRequirement`
- `Payloads`
- `SourceGate`

`WBCardActivationExpansion` maps one card effect and target into `FWBCardActivationCommand`. That command carries source metadata, an `FWBEffectRequest`, optional cost payment commit, optional usage commit, and debug activation id.

`WBCardActivationCandidateGenerator` converts supplied card sources and candidate targets into deterministic candidates. `WBCardActivationLegalActionGenerator` converts candidates into the separate activation legal action family. Activation remains outside `FWBAction` and `WBActionCodec`.

## Current Supported Generic Effect Payloads

`FWBGenericEffectPayload` supports four operations:

- `ArmorEffect`
- `StatusEffect`
- `DamageEffect`
- `HealEffect`

The current generic payload structs are:

- `FWBArmorEffectRequest`
- `FWBStatusEffectRequest`
- `FWBDamageEffectRequest`
- `FWBHealEffectRequest`

Current armor operations:

- `AddCurrentArmor`
- `ReduceCurrentArmor`
- `SetCurrentArmor`
- `AddMaxArmor`
- `ReduceMaxArmor`
- `SetMaxArmor`
- `RestoreArmorToMax`

Current status operations:

- `ApplyStatus`
- `RemoveStatus`
- `SetStatusDuration`
- `AddStatusDuration`
- `ReduceStatusDuration`
- `CleanseStatus`
- `CleanseAllStatuses`

Damage and heal payloads are amount-based and target unit based. Damage supports armor bypass. Healing clamps to max HP through the current heal effect path.

## Current Activation Source Gate Fields

`FWBCardActivationSourceGateDefinition` currently includes:

- required source zone
- timing requirement
- fixture zone ownership requirement
- source unit requirement
- source unit ownership requirement
- Stunned block flag
- Frozen block flag
- external costs-satisfied flag
- detailed cost gate
- once-per-turn flag
- once-per-turn key
- explicit source gate marker

Allowed source-zone enum values are:

- `Fixture`
- `Hand`
- `Board`
- `Equipped`
- `Discard`
- `Deck`

Current timing enum values are:

- `Any`
- `NormalTurnPriority`
- `ResponseWindow`

Response window timing exists as an enum value, but response-window activation is not implemented and currently remains unsupported in the activation provider plan.

## Current Cost And Usage Scaffolding

`FWBCardActivationCostGateDefinition` contains:

- external affordability requirement
- required RR
- cost kind metadata

`WBCardActivationAffordability` can query available RL without mutating state. Current RL policy treats `RLTotal` as current RL until a base/current RL distinction exists. Available RL is computed from `RLTotal - RLUsed`.

`FWBCardActivationCostPaymentCommit` lives on `FWBCardActivationCommand`. Payment happens only during successful activation execution, not during candidate generation or provider output generation.

`FWBCardActivationUsageCommit` marks usage only after successful activation. Failed activation does not mark usage.

## Current Zone-Source Scaffolding

Unreal has fixture-only zone ownership metadata:

- `FWBCardActivationFixtureZoneEntry`
- `FWBCardActivationFixtureZoneContext`
- `EquippedToUnitId` for fixture Equipped sources

Board-source parity uses visible board unit `CardId`. Hand, Equipped, and Discard zone checks are fixture scaffold only. Deck activation is explicitly unsupported in this scaffold.

There is no production hand/deck/discard/equipped zone model yet.

## Current Runtime Provider Contract Expectations

The runtime activation provider contract expects successful provider results to include:

- accepted request kind
- valid player id
- non-empty decision point id
- selected activation action id for post-activation results
- normal legal actions when required
- activation legal actions when required
- public board summary units when required
- non-empty activation action ids
- deterministic output
- no forbidden hidden/internal substrings in provider-facing debug output

The runtime provider shell remains an external-data consumer. Runtime does not generate normal legal actions, activation candidates, activation legal actions, or public summaries through the provider path.

## Godot CardDB And Effect Shape Observations

The read-only Godot `CardDB.gd` loads card arrays from characters, hybrids, equip wands, react effects, quick effects, traps, and NPCs. It normalizes ids, categories, card names, effect metadata, passives, status ids, tags, and category filters. It returns deep copies for card queries.

Sample Godot card data uses fields such as:

- `id`
- `category`
- `name`
- `timing`
- `effects`
- `rr`
- `subclass`
- `restrictions`
- `notes`

Observed Godot effect strings include supported-like concepts such as heal and status application, plus unsupported current Unreal concepts such as draw cards, move within range, swap units, wall placement, wall movement, wall removal, stat modification, rule flags, on-event directives, response triggers, prevention, negation, summons, deck search, and wand behaviors.

Godot state includes hands, decks, discards, equipped wand arrays on units, hidden tile markers, board unit card ids, and reveal flags. Godot asset documentation says hidden hand and deck use card backs, hidden markers use a generic marker, and discard is public with top card face allowed.

## Gaps Between Godot Data And Current Unreal Payloads

Current Unreal activation import can only safely target:

- armor effects
- status effects
- damage effects
- heal effects

Current gaps include:

- draw/search/deck manipulation
- hand/deck/discard/equipped production zones
- response windows
- effect negation
- passives
- wands
- replacement/prevention effects
- death triggers
- marker manipulation
- terrain mutation effects
- wall placement/removal/movement effects
- summon effects
- random effects
- multi-choice/modal effects
- conditional branches
- complex until-end-of-turn modifiers
- card-specific exceptions

Those effects must fail closed until explicit Unreal systems exist.

## Why Documentation-Only

The schema is the contract target for a future importer. Implementing an importer before the schema, diagnostics, visibility policy, and fail-closed boundaries are documented would risk silent behavior gaps or hidden-information leaks.

## Why Importer Implementation Is Deferred

The importer should wait until:

- schema validation examples are stable
- fail-closed diagnostics are named
- production zone visibility is explicit
- provider contract extensions are planned
- unsupported effect policy is agreed
- CardDB import tests can prove no runtime, UI, or hidden-information boundary is crossed
