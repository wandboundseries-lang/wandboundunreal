# Card Activation Legal Action Replay Verifier Audit

## Scope

This audit was completed before adding the selected activation action ID replay verifier. It inspected the existing activation candidate, activation legal action, cost payment, replay trace, and fixture utility paths only. Godot reference files remain read-only and were not modified, imported, compiled, or runtime-loaded.

## Current Activation Candidate Generation

`WBCardActivationCandidateGenerator::GenerateCandidates` builds deterministic fixture-owned candidates from externally supplied `FWBCardActivationCandidateSource` values. It validates player/source basics, filters through source gates, skips dynamic invalid targets, expands matching card effects into `FWBCardActivationCommand`, and preserves deterministic order by source, effect, and candidate target.

Each candidate stores:

- activation candidate id.
- player id.
- source unit id.
- source card/effect ids.
- public label.
- target reference.
- wrapped activation command.

The candidate id currently uses a deterministic `activate:p...` format, but this id is not a `WBActionCodec` id.

## Current Activation Legal Action Family

`WBCardActivationLegalActionGenerator::GenerateFromCandidates` converts precomputed candidates into a separate activation legal action family. It preserves supplied order, uses the candidate id as the activation action id, copies the candidate/target/command, validates basic command payload shape, and rejects unsafe public labels.

`FWBCardActivationLegalAction` is intentionally separate from `FWBAction`. It can carry rich activation command metadata without expanding the core action enum or replay action codec.

## Current Activation Action ID Policy

Activation legal action ids are external decision ids owned by the activation legal action family. The current fixture ids start with `activate:`, but the core `FWBAction` legal action path does not generate them and `WBActionCodec` does not encode or decode them.

This separation lets tests select fixture-owned activation commands without changing player action semantics.

## Current Cost-Payment Behavior

`FWBCardActivationCommand` carries optional `FWBCardActivationCostPaymentCommit` metadata. `WBRules::CanApplyCardActivationCommand` validates payment affordability, and `WBEffectRunner::ApplyCardActivationCommand` pays on a working copy before effect resolution.

The original state is assigned only after the full activation succeeds. Failed payment, failed effect validation, and failed effect resolution leave original `RLUsed` and usage state unchanged.

Payment mutates only source unit `RLUsed`. Overflow, wand destruction, equipped-wand fallout, production zones, CardDB import, response windows, negation, passives, and card-specific behavior remain deferred.

## Current Replay Fixture Support

`WBReplayFixtureTestUtils` already supports:

- `generate_card_activation_candidates`
- `generate_card_activation_legal_actions`
- `apply_card_activation_command`
- `build_card_activation_command`
- `build_and_apply_card_activation_command`

The shared fixture checks already support optional expectations for activation action ids, public labels, payment commit metadata, final RL state, cost-paid trace fields, forbidden trace substrings, trace order, and final unit state.

Before this pass there is no fixture operation that selects an activation action id from a generated activation legal action family and applies the selected command.

## Current Cost-Payment Verifier Coverage

The test-only `FWBCardActivationCostPaymentVerifier` can verify:

- source unit `RLTotal`, `RLUsed`, and derived available RL.
- `card_activation_cost_paid` trace fields.
- presence of a trace kind.
- serialized trace JSON exclusion of forbidden substrings.

These helpers live under `WandboundTests` only.

## Current Trace Fields

Successful card activation command application can emit:

- `card_activation_resolved`
- `card_activation_cost_paid`
- `effect_request_resolved`
- child payload traces such as `damage_effect_resolved`, `heal_effect_resolved`, `armor_modified`, and `status_modified`
- `card_activation_usage_marked`

`card_activation_cost_paid` carries public-safe payment fields:

- player id.
- source unit id.
- cost amount.
- previous and new `RLUsed`.
- available RL before and after payment.
- cost kind.

Trace serialization does not include source card id, source effect id, debug activation id, deck, hand, discard, private choices, or usage keys.

## Missing Replay Coverage

Before this pass, coverage proves payment and activation legal action generation separately, but not the selected-ID replay path. Missing coverage includes:

- resolving a selected activation action id from a generated activation legal action set.
- applying the selected activation command through the existing EffectRunner path.
- missing selected id failure without mutation.
- ambiguous selected id failure without mutation.
- selected-id cost payment trace parity.
- selected-id final `RLUsed` and derived available RL parity.
- selected-id payment plus usage trace ordering.
- selected-id hidden metadata exclusion.
- confirmation that selected activation ids remain external and are not `FWBAction` / `WBActionCodec` ids.

## Why This Pass Is Replay/Verifier-Only

The gameplay behavior already exists at the activation command application layer. This pass should only add deterministic test helpers, fixture operation support, golden scenarios, and automation coverage for selecting and applying activation legal action ids.

## Why Activation Remains Separate From FWBAction

Activation legal actions currently carry command metadata, cost payment commits, usage commits, source/effect ids, target refs, and fixture card definitions. Pulling that into `FWBAction` would change the canonical player action surface and replay action codec. This pass only verifies the external activation decision path.

## Why WBActionCodec Remains Unchanged

`WBActionCodec` is the canonical codec for existing core `FWBAction` ids such as movement, attack, end turn, pass, and pass response. Activation ids are fixture-owned external decision inputs for this pass, so the codec must not encode `activate:` ids yet.

## Deferred Work

- overflow and wand fallout
- production CardDB import
- real hand/deck/discard/equipped zones
- response windows
- effect negation
- passives and wands
- card-specific costs and replacement/prevention effects
- UI/runtime presentation
