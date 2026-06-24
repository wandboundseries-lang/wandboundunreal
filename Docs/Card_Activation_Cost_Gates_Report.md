# Card Activation Cost Gates Report

## Purpose

This pass adds fixture-driven card activation cost-gate scaffolding for externally supplied RR/RL affordability. It lets fixture-owned activated effects require affordability information before candidate generation without implementing real payment.

## Design

`FWBCardActivationCostGateDefinition` lives on `FWBCardActivationSourceGateDefinition` and carries:

- `bRequiresExternalAffordability`
- `RequiredRR`
- metadata-only `CostKind`

`FWBCardActivationCostGateContext` lives on `FWBCardActivationSourceGateContext` and carries:

- `bHasExternalAffordability`
- `bExternallyAffordable`
- `SuppliedRequiredRR`
- `SuppliedAvailableRL`
- metadata-only `CostKind`

If a gate does not require external affordability, it passes. If it does, the context must provide affordability, the external result must be affordable, and supplied RR must match declared RR when both are positive.

The evaluator also performs a fixture consistency check: if `SuppliedRequiredRR > 0` and `SuppliedAvailableRL < SuppliedRequiredRR`, the gate fails with `cost_not_affordable` even when the external boolean says affordable.

## Follow-Up - Affordability Query

`WBCardActivationAffordability` can now prepare the external cost context from read-only unit RL fields. Cost gates remain consumers of that prepared data; they still do not compute payment, mutate `RLUsed`, inspect card zones, or own CardDB costs.

## Failure Reasons

- `invalid_cost_requirement`
- `cost_affordability_missing`
- `cost_not_affordable`
- `cost_requirement_mismatch`

## Candidate Filtering

`WBCardActivationCandidateGenerator` was not taught any payment rules. It already calls `WBCardActivationSourceGate::Evaluate` for each activated effect, so unaffordable effects are skipped through the source-gate path.

Candidate order remains source order, effect order, then target order for candidates that survive filtering.

## Simple Cost Flag Interaction

The existing `bRequiresCostsSatisfiedExternally` flag remains intact. If both the simple flag and detailed cost gate are present, both must pass:

- simple flag false fails with `costs_not_satisfied`
- detailed affordability false fails with a detailed cost reason
- both true passes

## No Payment Policy

This pass does not pay RR/RL costs, mutate `RLUsed`, destroy or overflow wands, emit cost-payment traces, inspect real card zones, or decide production card playability. Tests verify candidate generation and explicit activation application leave RL totals and used RL unchanged.

## Hidden Information Policy

Cost gate failure reasons are stable public-safe strings and do not include deck, hand, discard, private card identity, usage keys, or hidden choices. `CostKind` is fixture metadata only.

## Deferred

- real RR/RL payment
- `RLUsed` mutation
- overflow
- equipped wand fallout
- production CardDB import
- real card zones
- response windows
- effect negation
- passives and wands
- card-specific behavior
- UI target selection
