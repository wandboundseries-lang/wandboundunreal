# Atomic Hybrid Hero Replacement Audit

## Scope and Sources

Baseline: `f5d487cdc36421ccde84fa146d2ba8e50150e88b` on `main`, equal to `origin/main`.
The audit covered tracked Core, CardDB, Runtime, replay, data, tests, the Rules Bible, canonical glossary, and the read-only Godot reference. No Godot file was modified or loaded at runtime.

## Canonical Answers

| Question | Tracked evidence and result |
|---|---|
| Hybrid identity | Canonical card category `Hybrid`; Unreal previously had no production Hybrid definition kind. |
| Sacrifice count | Exactly one controlled Character. |
| Hero requirement | A Hybrid may sacrifice a controlled Character. This pass implements only the current-Hero branch. |
| Non-Hero sacrifice | Canonically supported, but placement/action support is deliberately deferred. |
| Wand payment | Exactly one Wand card instance, selected deterministically. It is not MP, RR, RL, or currency. |
| Payment zones | Acting player's hand or equipment attached to the sacrificed Character. |
| Payment destination | Paid Wand moves to the acting player's discard through card lifecycle. |
| Other equipment | Remaining equipment on the sacrificed Character moves to discard. |
| Hero destination | The vacated current-Hero tile (`sacrificed_hero_tile`). |
| Non-Hero destination | Adjacent to current Hero; not implemented in this pass. |
| Unit cap | Four including Hero, evaluated after sacrifice and summon. |
| Trigger semantics | Sacrifice is not treated as defeat. General sacrifice triggers are unsupported. |
| Summon/marker integration | Replacement remains a summon; existing marker resolution is called after new Hero installation. No new trigger timing was invented. |
| Production reachability before pass | None. Hybrid was only excluded by Active Format validation. |
| Stable action representation | Existing Summon family can carry the immutable plan; no `WBActionCodec` change is needed. |

## Component Classification

| Path / symbol | Class | Current role | Hybrid / replacement change |
|---|---|---|---|
| `WBCardDefinition.h` / `EWBCardDefinitionKind` | Legality | Core definition schema | Adds Hybrid and explicit summon contract. |
| `WBCardDefinitionRepository::ValidateRepository` | Legality | Definition validation | Fails closed unless the exact supported sacrifice/payment/destination contract is present. |
| `WBProductionCardDatabase` | FixtureOnly | Generic JSON loader | Parses Hybrid and `hybrid_summon`; canonical production definitions remain unchanged. |
| `ProductionCardDB.schema.json` | FixtureOnly | Generic schema | Strict conditional Hybrid payload. |
| `WBHybridSummon::BuildHeroReplacementPlans` | TransactionPlanner | Immutable preflight | Enumerates hand/equipped payments in canonical order, validates complete transaction. |
| `WBHybridSummon::ExecuteHeroReplacement` | MutationHelper | Atomic copy-then-commit | Pays Wand, cleans equipment, removes old Hero, summons replacement, assigns Hero ID, then commits. |
| `WBMatchCoordinator::EnumerateLegalActionsForState` | CoordinatorAuthority | Sole production legal-action owner | Adds stable Summon-family Hybrid actions. |
| `WBMatchCoordinator::SubmitActionId` | CoordinatorAuthority | Sole production action authority | Dispatches validated plan and records one accepted action. |
| `WBCardLifecycle` | PaymentHelper | Zone mutation authority | Existing hand/equipped-to-discard helpers reused unchanged. |
| `WBDeathResolution` | DeathCleanup | Zero-HP and Hero-loss authority | Unchanged; ordinary and replacement-Hero death remain terminal. |
| `WBProductionMatchReplay` | Replay | Accepted-action archive | Existing schema/family/hash chain reused unchanged. |
| `FWBProductionMatchReplayRunner` | Replay | Fresh coordinator verifier | Returns final Hero and public-safe zone counts for smoke verification. |
| `WBPublicBoardSummary` | PublicObservation | Public board DTO | Adds authoritative `bHeroUnit`, derived from player `HeroUnitId`. |
| `WBRuntimeTracePresentationTranslator` | Presentation | Trace-only translation | Maps sacrifice, safe payment, summon, and replacement events. |
| `WBProductionHybridReplacementSmoke` | FixtureOnly | Packaged coordinator smoke | Uses only legal actions, public summaries, recorder, and fresh runner. |
| `Reference/GodotProject/scripts/sim/rules.gd` | Legacy | Read-only behavior reference | Confirms Hero tile, payment zones, discard cleanup, printed stats, and Hero reassignment. |

## Typed Plan

`FWBHybridSummonPlan` contains acting player, Hybrid instance and definition IDs, sacrificed Hero unit ID, typed Wand payment source, payment card instance ID, payment unit ID where applicable, destination tile, replacement-Hero flag, and before generation/revision.

Checks are ownership, hand zone, Hybrid schema, controlled active Character Hero, completed unit cap, in-bounds and occupied-by-sacrifice destination, turn-one summon restriction, eligible Wand ownership/type/zone, and generation/revision freshness. Equivalent payment choices sort by source and stable card instance ID.

Typed failures are `hybrid_definition_invalid`, `hybrid_not_in_hand`, `hybrid_wrong_player`, `hybrid_sacrifice_required`, `hybrid_sacrifice_invalid`, `hybrid_hero_sacrifice_invalid`, `hybrid_wand_payment_required`, `hybrid_wand_payment_invalid`, `hybrid_destination_invalid`, `hybrid_destination_occupied`, `hybrid_unit_cap_exceeded`, `hybrid_replacement_not_supported`, `hybrid_plan_stale`, `hybrid_zone_state_invalid`, and `hybrid_unit_id_allocation_failed`.

## Atomic and Terminal Boundary

Execution mutates a copied state only after the complete plan is regenerated and matched. Mutation order is Wand payment, remaining equipment cleanup, Hybrid hand removal, old Hero sacrifice/removal, replacement creation from printed stats, player `HeroUnitId` update, state commit, traces, then existing marker resolution/coordinator terminal evaluation. No coordinator state, replay record, public summary, or terminal check observes the Hero-less intermediate working copy.

Valid replacement emits no `hero_defeated`, `terminal_state_committed`, or `game_over`. The ordinary zero-HP death path remains unchanged and immediately produces `hero_defeated_without_replacement` for either an original or replacement Hero.

## Unsupported

- Non-Hero Hybrid sacrifice and adjacent placement.
- Sacrifice-trigger and card-specific summon-trigger timing.
- Any HP, status, equipment, RL, attack, activation, temporary-effect, or stable-unit-ID inheritance.
- Simultaneous dual-Hero resolution beyond the existing fail-closed behavior.
- Production Hybrid cards or deck changes.
