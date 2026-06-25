# Card Activation Board Source Legal Action Audit

## Scope

This audit supports focused Board-source activation legal-action and replay coverage. It does not add production card zones, import Godot CardDB, add activation to `FWBAction`, modify `WBActionCodec`, modify `WBRules::GenerateLegalActions`, or add UI/response windows.

## Files Inspected

- `Source/WandboundCore/Public/WBCardActivationSourceGate.h`
- `Source/WandboundCore/Private/WBCardActivationSourceGate.cpp`
- `Source/WandboundCore/Public/WBCardActivationCandidate.h`
- `Source/WandboundCore/Private/WBCardActivationCandidateGenerator.cpp`
- `Source/WandboundCore/Public/WBCardActivationLegalAction.h`
- `Source/WandboundCore/Private/WBCardActivationLegalActionGenerator.cpp`
- `Source/WandboundCore/Public/WBCardActivationCommand.h`
- `Source/WandboundCore/Private/WBCardActivationExpansion.cpp`
- `Source/WandboundTests/Private/WBCardActivationBoardSourceParityTests.cpp`
- `Source/WandboundTests/Private/WBCardActivationLegalActionGenerationTests.cpp`
- `Source/WandboundTests/Private/WBCardActivationLegalActionReplayVerifierTests.cpp`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`

## Current Board-Source Parity Behavior

Board-source activation gates use `WBCardActivationSourceGate::EvaluateBoardSourceParity` when `RequiredZone = Board` and fixture ownership is enabled. The gate validates source unit presence, board/removal state, optional owner match, non-empty source/unit card ids, and exact source unit `CardId` parity with `SourceCardId`.

Board-source parity uses the visible board unit record as source of truth and does not require `FixtureZoneContext`.

## Current Candidate Generation Behavior

`WBCardActivationCandidateGenerator` builds activation candidates in deterministic source, effect, and target order. It evaluates source gates before expansion. For Board-source definitions, missing `SourceCardId` is inherited from the source-level context or `FWBCardDefinition::CardId`; effect-specific source-gate contexts inherit missing source fields from the base source context.

Board-source candidates carry:

- `ActivationCandidateId`
- `PlayerId`
- `SourceUnitId`
- `SourceCardId`
- `SourceEffectId`
- `PublicLabel`
- `Target`
- wrapped `FWBCardActivationCommand`

## Current Legal Action Family Behavior

`WBCardActivationLegalActionGenerator::GenerateFromCandidates` already handles Board-source candidates without special-case code. It preserves candidate order, uses `Candidate.ActivationCandidateId` as the activation action id, copies public label, target, candidate, and full command, including `CostPaymentCommit` and `UsageCommit`.

The generator is read-only. It does not call `WBRules`, `WBRules::GenerateLegalActions`, `WBEffectRunner`, or `WBActionCodec`.

## Current Replay Verifier Behavior

`FWBCardActivationLegalActionReplayVerifier` is test-only. It resolves a selected activation action id against a supplied `FWBCardActivationLegalActionSet`; missing ids fail with `activation_action_id_not_found`, duplicates fail with `activation_action_id_ambiguous`, and unique selections apply the preserved command through `WBEffectRunner::ApplyCardActivationCommand`.

Fixture operation `apply_card_activation_legal_action_by_id` already generates candidates, converts them to activation legal actions, validates expected ids/labels/payment commit metadata, resolves the selected id, and applies the selected command.

This pass also extends optional fixture validation so Board-source legal-action fixtures can assert generated candidate counts/ids/labels and command usage commit preservation when those expected fields are present.

## Missing Focused Coverage

The generic legal action and replay paths already support Board-source candidates, but focused Board-source coverage was missing for:

- damage/status/armor/heal Board-source legal action conversion
- Board-source legal action ordering
- Board-source cost and usage commit preservation
- selected-id replay for Board-source legal actions
- card mismatch producing no Board-source legal action
- Board-source no-`FixtureZoneContext` legal action generation
- Board-source action id stability and `WBActionCodec` separation
- Board-source hidden metadata exclusion

## Chosen Pass Design

This pass is verification and integration coverage only. It adds Board-source fixtures and tests around the existing generic activation legal action family and replay verifier. No production rules or runtime action systems are changed.

## Separation Policy

Activation legal action ids remain external activation-family ids. They may include fixture/visible source card ids because they are selection metadata for this separate family. They are not `WBActionCodec` ids, do not enter `FWBAction`, do not appear in core legal action generation, and are not emitted in public traces.

## Deferred Work

Still deferred:

- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- response windows
- effect negation
- passives and wands
- UI target selection
- card-specific behavior
