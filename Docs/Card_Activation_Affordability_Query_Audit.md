# Card Activation Affordability Query Audit

## Files Inspected

- `Source/WandboundCore/Public/WBGameStateData.h`
- `Source/WandboundCore/Public/WBPublicBoardSummary.h`
- `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`
- `Source/WandboundCore/Public/WBCardActivationSourceGate.h`
- `Source/WandboundCore/Private/WBCardActivationSourceGate.cpp`
- `Source/WandboundCore/Public/WBCardDefinition.h`
- `Source/WandboundCore/Public/WBCardActivationCandidate.h`
- `Source/WandboundCore/Private/WBCardActivationCandidateGenerator.cpp`
- `Docs/Card_Activation_Cost_Gates_Audit.md`
- `Docs/Card_Activation_Cost_Gates_Report.md`

## Current Unreal RL/RR Surface

`FWBUnitState` currently stores `RLTotal` and `RLUsed`. There is no separate base/current RL split yet, so this pass treats `RLTotal` as current RL for read-only affordability queries. Available RL is computed as `max(0, RLTotal - RLUsed)`.

`FWBPublicUnitBoardSummary` exposes `RLTotal` and `RLUsed` for visible public units, and runtime result JSON serializes those same public values. Public summaries do not currently expose a precomputed available RL field.

Card RR exists only in fixture-owned cost gate data today. `FWBCardActivationCostGateDefinition::RequiredRR` carries a required RR for a fixture effect, and `FWBCardActivationCostGateContext` carries externally supplied affordability data. There is no production CardDB import, real card zone model, or production card cost owner yet.

## Current Cost-Gate Behavior

`WBCardActivationSourceGate::Evaluate` is read-only and consumes external cost context. If a cost gate requires affordability, the context must say affordability exists and must say the activation is affordable. It rejects negative cost values, mismatched declared/supplied RR when both are positive, and supplied available RL lower than supplied required RR.

Candidate generation already calls the source gate for each effect and skips failed effects. It does not pay costs, mark usage, call `WBEffectRunner`, or generate `FWBAction` activation entries.

## Why Add an Affordability Query Interface

The cost gate deliberately does not inspect rules state for payment or CardDB data. A separate read-only affordability query interface gives future card/zone/cost systems a deterministic way to compute the external context that source gates already consume.

This pass adds the query interface now so fixtures and future production callers can prepare affordability context without merging activation into player legal actions, importing CardDB, or deciding payment semantics prematurely.

## Hidden-Information Boundary

The query request can carry source card/effect ids as metadata, but this pass does not serialize them into public summaries or traces. The query reads only the supplied source unit's public rules-state RL fields. It does not inspect deck, hand, discard, hidden choices, or private card zones.

## Deferred

- CardDB/imported RR values
- real card zones
- equipped wands and wand fallout
- actual payment mutation
- `RLUsed` mutation
- overflow and wand destruction
- response windows
- effect negation
- passives/wands
- UI target selection
