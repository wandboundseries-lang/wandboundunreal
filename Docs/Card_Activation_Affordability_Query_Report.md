# Card Activation Affordability Query Report

## Purpose

This pass adds a production-owned, read-only affordability query interface for future card activation systems. It computes RR/RL affordability context that source gates can consume, without paying costs or changing rules state.

## Interface

`FWBCardActivationAffordabilityRequest` carries:

- `PlayerId`
- `SourceUnitId`
- metadata-only `SourceCardId`
- metadata-only `SourceEffectId`
- `RequiredRR`
- metadata-only `CostKind`

`FWBCardActivationAffordabilityResult` carries:

- `bOk`
- failure `Reason`
- player/source ids
- `RequiredRR`
- `CurrentRL`
- `RLUsed`
- `AvailableRL`
- `bAffordable`
- metadata-only `CostKind`

## QueryFromUnitRL

`WBCardActivationAffordability::QueryFromUnitRL` is read-only. It validates the player, non-negative RR requirement, source unit existence, source unit board state, and source ownership.

Current Unreal unit state has `RLTotal` and `RLUsed` only. Until a base/current RL split exists, the query treats `RLTotal` as current RL and computes:

```text
AvailableRL = max(0, RLTotal - RLUsed)
```

A successful query sets `bOk = true` even when `bAffordable = false`. This keeps "the query succeeded" separate from "the activation can be afforded." Missing, removed, invalid, or owner-mismatched sources return `bOk = false` with stable reason strings.

## Context Preparation

`ApplyResultToSourceGateContext` projects query results into `FWBCardActivationCostGateContext`:

- `bHasExternalAffordability`
- `bExternallyAffordable`
- `SuppliedRequiredRR`
- `SuppliedAvailableRL`
- `CostKind`

When the query succeeds, it also synchronizes the legacy simple flag by setting `bCostsSatisfiedExternally = bAffordable`.

`BuildCandidateSourceWithAffordability` copies a candidate source and prepares effect-specific source-gate contexts for effects whose cost gates require external affordability. It does not mutate the original source or `FWBGameStateData`.

## Effect-Specific Contexts

`FWBCardActivationCandidateSource` now supports `EffectIdToSourceGateContext`. Candidate generation uses an effect-specific context when present, otherwise it falls back to the source-level context. This supports one card source with multiple effects that have different RR costs.

## Fixed Provider

`FWBFixedCardActivationAffordabilityProvider` returns deterministic fixture/test results and fills unset player/source/RR/cost-kind fields from the request. It does not mutate state and is not a payment system.

## Follow-Up - Cost Payment Scaffolding

Affordability query results now feed both source-gate filtering and payment validation scaffolding. The query itself remains read-only: it does not pay RR, mutate `RLUsed`, open response windows, import CardDB, or decide production card playability.

## Fixture Support

Added fixture operation:

```json
"operation": "query_card_activation_affordability"
```

Added optional activation source parser support for:

```json
"effect_source_gate_contexts": {
  "effect_id": {
    "source_zone": "board",
    "cost_context": {}
  }
}
```

## Guardrails

This pass does not:

- directly pay RR/RL costs from the affordability query
- directly mutate `RLUsed` from the affordability query
- mutate rules state
- destroy or overflow wands
- import Godot CardDB
- production-load card JSON
- create production card zones
- merge activation into `FWBAction`
- change `WBActionCodec`
- open response windows
- add UI, Blueprint, `.uasset`, or `.umap` work

Source card/effect ids remain metadata for the query and are not emitted in traces or public summaries.

## Deferred

- production payment ownership
- `RLUsed` mutation
- overflow
- equipped wand fallout
- production CardDB
- real card zones
- response windows
- effect negation
- passives and wands
- card-specific behavior
- UI target selection
