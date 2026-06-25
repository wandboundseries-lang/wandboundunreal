#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeZonePlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeZoneUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("fixture_zone_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 3;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeZoneState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 11;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeZonePlayer(0));
	State.Players.Add(MakeZonePlayer(1));
	State.AddUnitForTest(MakeZoneUnit(1, 0, FWBTile(4, 4)));
	State.AddUnitForTest(MakeZoneUnit(2, 1, FWBTile(5, 4)));
	return State;
}

FWBGenericEffectPayload MakeZoneHealPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardActivationSourceGateDefinition MakeZoneGate(
	const EWBCardActivationSourceZone Zone,
	const bool bRequiresSourceUnit = false)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = Zone;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresFixtureZoneOwnership = true;
	Gate.bRequiresSourceUnit = bRequiresSourceUnit;
	Gate.bRequiresSourceUnitOwnership = true;
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardEffectDefinition MakeZoneEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBCardActivationSourceGateDefinition& Gate)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = Gate;
	Effect.Payloads.Add(MakeZoneHealPayload());
	return Effect;
}

FWBCardDefinition MakeZoneDefinition(
	const FString& CardId,
	const FWBCardActivationSourceGateDefinition& Gate,
	const FString& EffectId = TEXT("heal_1"),
	const FString& PublicLabel = TEXT("Heal 1 HP"))
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicLabel;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeZoneEffect(EffectId, PublicLabel, Gate));
	return Definition;
}

FWBEffectTargetRef MakeZoneUnitTarget(const int32 UnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardActivationFixtureZoneEntry MakeZoneEntry(
	const FString& CardId,
	const int32 OwnerPlayerId,
	const EWBCardActivationSourceZone Zone,
	const int32 EquippedToUnitId = -1)
{
	FWBCardActivationFixtureZoneEntry Entry;
	Entry.CardId = CardId;
	Entry.OwnerPlayerId = OwnerPlayerId;
	Entry.Zone = Zone;
	Entry.EquippedToUnitId = EquippedToUnitId;
	return Entry;
}

FWBCardActivationSourceGateContext MakeZoneContext(
	const FString& CardId,
	const EWBCardActivationSourceZone Zone,
	const TArray<FWBCardActivationFixtureZoneEntry>& Entries,
	const int32 SourceUnitId,
	const bool bIncludeSourceCardId = true)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = Zone;
	Context.SourceCardId = bIncludeSourceCardId ? CardId : FString();
	Context.FixtureZoneContext.Entries = Entries;
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationCandidateSource MakeZoneSource(
	const FString& CardId,
	const EWBCardActivationSourceZone Zone,
	const TArray<FWBCardActivationFixtureZoneEntry>& Entries,
	const FWBCardActivationSourceGateDefinition& Gate,
	const int32 SourceUnitId = -1,
	const bool bIncludeSourceCardId = true)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = MakeZoneDefinition(CardId, Gate);
	Source.CandidateTargets.Add(MakeZoneUnitTarget(2));
	Source.SourceGateContext = MakeZoneContext(CardId, Zone, Entries, SourceUnitId, bIncludeSourceCardId);
	return Source;
}

TArray<FString> CandidateIds(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Ids.Add(Candidate.ActivationCandidateId);
	}
	return Ids;
}

FString ZoneFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadZoneFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ZoneFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutFixture) && OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetZoneExpectedObject(const TSharedPtr<FJsonObject>& Fixture)
{
	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		&& ExpectedObject != nullptr
		&& ExpectedObject->IsValid())
	{
		return *ExpectedObject;
	}

	return nullptr;
}

FWBAction MakeZoneAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	Action.TargetUnitId = 2;
	return Action;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneHandTest, "Wandbound.Core.CardActivationSourceZoneOwnership.Hand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneHandTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_hand_card");
	FWBGameStateData State = MakeZoneState();
	const FWBCardActivationSourceGateDefinition HandGate = MakeZoneGate(EWBCardActivationSourceZone::Hand);

	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, HandGate)
		});
	TestTrue(TEXT("Hand owned generation ok"), Result.bOk);
	TestEqual(TEXT("Hand owned candidate generated"), Result.Candidates.Num(), 1);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, {
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 1, EWBCardActivationSourceZone::Hand) }, HandGate)
	});
	TestTrue(TEXT("Hand wrong owner generation ok"), Result.bOk);
	TestEqual(TEXT("Hand wrong owner excluded"), Result.Candidates.Num(), 0);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, {
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(TEXT("other_card"), 0, EWBCardActivationSourceZone::Hand) }, HandGate)
	});
	TestTrue(TEXT("Hand missing card generation ok"), Result.bOk);
	TestEqual(TEXT("Hand missing card excluded"), Result.Candidates.Num(), 0);

	FWBGameStateData WrongPriorityState = MakeZoneState();
	WrongPriorityState.PriorityPlayer = 1;
	FWBCardActivationSourceGateDefinition PriorityGate = HandGate;
	PriorityGate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(WrongPriorityState, {
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, PriorityGate)
	});
	TestTrue(TEXT("Hand wrong priority generation ok"), Result.bOk);
	TestEqual(TEXT("Hand wrong priority excluded"), Result.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneEquippedTest, "Wandbound.Core.CardActivationSourceZoneOwnership.Equipped", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneEquippedTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_equipped_card");
	FWBGameStateData State = MakeZoneState();
	const FWBCardActivationSourceGateDefinition EquippedGate = MakeZoneGate(EWBCardActivationSourceZone::Equipped, true);

	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeZoneSource(CardId, EWBCardActivationSourceZone::Equipped, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Equipped, 1) }, EquippedGate, 1)
		});
	TestTrue(TEXT("Equipped owned generation ok"), Result.bOk);
	TestEqual(TEXT("Equipped owned candidate generated"), Result.Candidates.Num(), 1);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, {
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Equipped, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Equipped, 4) }, EquippedGate, 1)
	});
	TestTrue(TEXT("Equipped wrong unit generation ok"), Result.bOk);
	TestEqual(TEXT("Equipped wrong unit excluded"), Result.Candidates.Num(), 0);

	FWBGameStateData RemovedState = MakeZoneState();
	RemovedState.GetMutableUnitById(1)->MarkUnitDefeated();
	RemovedState.GetMutableUnitById(1)->RemoveUnitFromBoard();
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(RemovedState, {
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Equipped, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Equipped, 1) }, EquippedGate, 1)
	});
	TestTrue(TEXT("Equipped removed source generation ok"), Result.bOk);
	TestEqual(TEXT("Equipped removed source excluded"), Result.Candidates.Num(), 0);

	FWBCardActivationSourceGateContext MissingUnitContext =
		MakeZoneContext(CardId, EWBCardActivationSourceZone::Equipped, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Equipped, 1) }, -1);
	const FWBCardActivationSourceGateResult MissingUnitResult =
		WBCardActivationSourceGate::EvaluateFixtureZoneOwnership(EquippedGate, MissingUnitContext);
	TestFalse(TEXT("Equipped missing source unit fails"), MissingUnitResult.bOk);
	TestEqual(TEXT("Equipped missing source unit reason"), MissingUnitResult.Reason, FString(TEXT("equipped_source_unit_required")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneDiscardDeckFixtureTest, "Wandbound.Core.CardActivationSourceZoneOwnership.DiscardDeckFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneDiscardDeckFixtureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZoneState();

	const FString DiscardCardId = TEXT("fixture_discard_card");
	const FWBCardActivationSourceGateDefinition DiscardGate = MakeZoneGate(EWBCardActivationSourceZone::Discard);
	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeZoneSource(DiscardCardId, EWBCardActivationSourceZone::Discard, { MakeZoneEntry(DiscardCardId, 0, EWBCardActivationSourceZone::Discard) }, DiscardGate)
		});
	TestTrue(TEXT("Discard owned generation ok"), Result.bOk);
	TestEqual(TEXT("Discard owned candidate generated"), Result.Candidates.Num(), 1);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, {
		MakeZoneSource(DiscardCardId, EWBCardActivationSourceZone::Discard, { MakeZoneEntry(DiscardCardId, 1, EWBCardActivationSourceZone::Discard) }, DiscardGate)
	});
	TestTrue(TEXT("Discard wrong owner generation ok"), Result.bOk);
	TestEqual(TEXT("Discard wrong owner excluded"), Result.Candidates.Num(), 0);

	const FString DeckCardId = TEXT("fixture_deck_card");
	const FWBCardActivationSourceGateDefinition DeckGate = MakeZoneGate(EWBCardActivationSourceZone::Deck);
	FWBCardActivationSourceGateContext DeckContext =
		MakeZoneContext(DeckCardId, EWBCardActivationSourceZone::Deck, { MakeZoneEntry(DeckCardId, 0, EWBCardActivationSourceZone::Deck) }, -1);
	const FWBCardActivationSourceGateResult DeckGateResult =
		WBCardActivationSourceGate::Evaluate(State, DeckGate, DeckContext);
	TestFalse(TEXT("Deck unsupported fails"), DeckGateResult.bOk);
	TestEqual(TEXT("Deck unsupported reason"), DeckGateResult.Reason, FString(TEXT("source_zone_deck_not_supported")));

	FWBCardActivationSourceGateDefinition LegacyGate;
	LegacyGate.RequiredZone = EWBCardActivationSourceZone::Fixture;
	LegacyGate.bHasExplicitSourceGate = true;
	FWBCardActivationCandidateSource LegacySource = MakeZoneSource(
		TEXT("fixture_legacy_card"),
		EWBCardActivationSourceZone::Fixture,
		{},
		LegacyGate);
	LegacySource.SourceGateContext.FixtureZoneContext.Entries.Reset();
	LegacySource.SourceGateContext.SourceCardId.Reset();
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { LegacySource });
	TestTrue(TEXT("Fixture legacy generation ok"), Result.bOk);
	TestEqual(TEXT("Fixture legacy candidate generated"), Result.Candidates.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneCompositionTest, "Wandbound.Core.CardActivationSourceZoneOwnership.Composition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneCompositionTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_composed_card");
	FWBGameStateData State = MakeZoneState();
	FWBCardActivationSourceGateDefinition CostGate = MakeZoneGate(EWBCardActivationSourceZone::Hand);
	CostGate.CostGate.bRequiresExternalAffordability = true;
	CostGate.CostGate.RequiredRR = 2;
	CostGate.CostGate.CostKind = FName(TEXT("RR"));

	FWBCardActivationCandidateSource AffordableSource =
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, CostGate);
	AffordableSource.SourceGateContext.CostContext.bHasExternalAffordability = true;
	AffordableSource.SourceGateContext.CostContext.bExternallyAffordable = true;
	AffordableSource.SourceGateContext.CostContext.SuppliedRequiredRR = 2;
	AffordableSource.SourceGateContext.CostContext.SuppliedAvailableRL = 3;
	AffordableSource.SourceGateContext.CostContext.CostKind = FName(TEXT("RR"));
	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { AffordableSource });
	TestTrue(TEXT("Owned affordable generation ok"), Result.bOk);
	TestEqual(TEXT("Owned affordable candidate generated"), Result.Candidates.Num(), 1);

	FWBCardActivationCandidateSource UnaffordableSource = AffordableSource;
	UnaffordableSource.SourceGateContext.CostContext.bExternallyAffordable = false;
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { UnaffordableSource });
	TestTrue(TEXT("Owned unaffordable generation ok"), Result.bOk);
	TestEqual(TEXT("Owned unaffordable candidate excluded"), Result.Candidates.Num(), 0);

	FWBCardActivationSourceGateDefinition OnceGate = MakeZoneGate(EWBCardActivationSourceZone::Hand);
	OnceGate.bOncePerTurn = true;
	FWBCardActivationCandidateSource OnceSource =
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, OnceGate);
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { OnceSource });
	TestTrue(TEXT("Owned unused once generation ok"), Result.bOk);
	TestEqual(TEXT("Owned unused once candidate generated"), Result.Candidates.Num(), 1);

	State.MarkActivationUsageKeyForTest(0, WBCardActivationSourceGate::BuildDefaultUsageKey(0, -1, CardId, TEXT("heal_1")));
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { OnceSource });
	TestTrue(TEXT("Owned used once generation ok"), Result.bOk);
	TestEqual(TEXT("Owned used once candidate excluded"), Result.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneInheritanceAndAmbiguityTest, "Wandbound.Core.CardActivationSourceZoneOwnership.InheritanceAndAmbiguity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneInheritanceAndAmbiguityTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_inherited_card");
	FWBGameStateData State = MakeZoneState();
	const FWBCardActivationSourceGateDefinition HandGate = MakeZoneGate(EWBCardActivationSourceZone::Hand);

	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, HandGate, -1, false)
		});
	TestTrue(TEXT("SourceCardId inherited generation ok"), Result.bOk);
	TestEqual(TEXT("SourceCardId inherited candidate generated"), Result.Candidates.Num(), 1);

	FWBCardActivationCandidateSource EffectContextSource =
		MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, HandGate);
	FWBCardActivationSourceGateContext EffectContext;
	EffectContext.PlayerId = 0;
	EffectContext.bHasExplicitSourceGateContext = true;
	EffectContextSource.EffectIdToSourceGateContext.Add(TEXT("heal_1"), EffectContext);
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { EffectContextSource });
	TestTrue(TEXT("Effect-specific context inheritance generation ok"), Result.bOk);
	TestEqual(TEXT("Effect-specific context inherits fixture zone"), Result.Candidates.Num(), 1);

	FWBCardActivationSourceGateContext AmbiguousContext =
		MakeZoneContext(CardId, EWBCardActivationSourceZone::Hand, {
			MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand),
			MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand)
		}, -1);
	const FWBCardActivationSourceGateResult AmbiguousResult =
		WBCardActivationSourceGate::Evaluate(State, HandGate, AmbiguousContext);
	TestFalse(TEXT("Duplicate entries fail"), AmbiguousResult.bOk);
	TestEqual(TEXT("Duplicate entries reason"), AmbiguousResult.Reason, FString(TEXT("source_zone_card_ambiguous")));

	FWBCardActivationSourceGateContext MissingCardIdContext = AmbiguousContext;
	MissingCardIdContext.SourceCardId.Reset();
	const FWBCardActivationSourceGateResult MissingCardIdResult =
		WBCardActivationSourceGate::Evaluate(State, HandGate, MissingCardIdContext);
	TestFalse(TEXT("Missing SourceCardId fails"), MissingCardIdResult.bOk);
	TestEqual(TEXT("Missing SourceCardId reason"), MissingCardIdResult.Reason, FString(TEXT("source_card_id_missing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneNoMutationAndHiddenTest, "Wandbound.Core.CardActivationSourceZoneOwnership.NoMutationAndHiddenMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneNoMutationAndHiddenTest::RunTest(const FString& Parameters)
{
	const FString SecretCardId = TEXT("secret_zone_card_token");
	FWBGameStateData State = MakeZoneState();
	State.Players[0].Hand.Add(TEXT("secret_real_hand_token"));
	State.Players[0].Discard.Add(TEXT("secret_real_discard_token"));
	State.Players[0].Deck.Add(TEXT("secret_real_deck_token"));

	const TArray<FString> InitialHand = State.Players[0].Hand;
	const TArray<FString> InitialDiscard = State.Players[0].Discard;
	const TArray<FString> InitialDeck = State.Players[0].Deck;

	const FWBCardActivationSourceGateDefinition HandGate = MakeZoneGate(EWBCardActivationSourceZone::Hand);
	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeZoneSource(SecretCardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(SecretCardId, 0, EWBCardActivationSourceZone::Hand) }, HandGate)
		});
	TestTrue(TEXT("Hidden fixture metadata generation ok"), Result.bOk);
	TestEqual(TEXT("Hidden fixture metadata candidate generated"), Result.Candidates.Num(), 1);
	TestTrue(TEXT("Real hand unchanged"), State.Players[0].Hand == InitialHand);
	TestTrue(TEXT("Real discard unchanged"), State.Players[0].Discard == InitialDiscard);
	TestTrue(TEXT("Real deck unchanged"), State.Players[0].Deck == InitialDeck);

	const FWBCardActivationCommandResult ApplyResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Result.Candidates[0].Command);
	TestTrue(TEXT("Hidden fixture command applies"), ApplyResult.bOk);

	const FString TraceJson = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestFalse(TEXT("Trace excludes fixture zone card id"), TraceJson.Contains(SecretCardId));
	TestFalse(TEXT("Trace excludes real hand"), TraceJson.Contains(TEXT("secret_real_hand_token")));
	TestFalse(TEXT("Trace excludes real discard"), TraceJson.Contains(TEXT("secret_real_discard_token")));
	TestFalse(TEXT("Trace excludes real deck"), TraceJson.Contains(TEXT("secret_real_deck_token")));
	for (const FWBPublicUnitBoardSummary& Unit : Summary.Units)
	{
		TestFalse(TEXT("Public board excludes fixture zone card id"), Unit.CardId.Contains(SecretCardId));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneSeparationTest, "Wandbound.Core.CardActivationSourceZoneOwnership.LegalGenerationAndCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneSeparationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZoneState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));

	const FString CardId = TEXT("fixture_zone_separation_card");
	const FWBCardActivationSourceGateDefinition HandGate = MakeZoneGate(EWBCardActivationSourceZone::Hand);
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, {
			MakeZoneSource(CardId, EWBCardActivationSourceZone::Hand, { MakeZoneEntry(CardId, 0, EWBCardActivationSourceZone::Hand) }, HandGate)
		});
	TestTrue(TEXT("Candidate generation ok"), CandidateResult.bOk);
	TestEqual(TEXT("Candidate generated"), CandidateResult.Candidates.Num(), 1);

	const FWBCardActivationLegalActionGenerationResult ActivationLegalResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	TestTrue(TEXT("Activation legal family ok"), ActivationLegalResult.bOk);
	TestEqual(TEXT("Activation legal family count"), ActivationLegalResult.ActionSet.Actions.Num(), 1);

	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	TestEqual(TEXT("FWBAction legal generation unchanged count"), AfterIds.Num(), BaselineIds.Num());
	for (int32 Index = 0; Index < FMath::Min(AfterIds.Num(), BaselineIds.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("FWBAction legal id %d unchanged"), Index), AfterIds[Index], BaselineIds[Index]);
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *AfterIds[Index]), AfterIds[Index].StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeZoneAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeZoneAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeZoneAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeZoneAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeZoneAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no activation IDs"), ActionCodecSource.Contains(TEXT("activate:")));

	const FString RulesPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBRules.cpp"));
	FString RulesSource;
	TestTrue(TEXT("Rules source loads"), FFileHelper::LoadFileToString(RulesSource, *RulesPath));
	TestFalse(TEXT("Rules legal generation has no activation family"), RulesSource.Contains(TEXT("WBCardActivationLegalAction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationSourceZoneFixtureScenariosTest, "Wandbound.Core.CardActivationSourceZoneOwnership.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationSourceZoneFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_zone_hand_owned_success.json"),
		TEXT("card_activation_zone_hand_wrong_owner_excluded.json"),
		TEXT("card_activation_zone_hand_missing_card_excluded.json"),
		TEXT("card_activation_zone_equipped_owned_to_source_success.json"),
		TEXT("card_activation_zone_equipped_wrong_unit_excluded.json"),
		TEXT("card_activation_zone_equipped_removed_source_excluded.json"),
		TEXT("card_activation_zone_discard_owned_success.json"),
		TEXT("card_activation_zone_discard_wrong_owner_excluded.json"),
		TEXT("card_activation_zone_deck_not_supported_excluded.json"),
		TEXT("card_activation_zone_fixture_default_legacy_success.json"),
		TEXT("card_activation_zone_combines_with_cost_usage.json"),
		TEXT("card_activation_zone_hidden_metadata_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadZoneFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));

		TArray<FWBCardActivationCandidateSource> Sources;
		FString ParseReason;
		TestTrue(
			*FString::Printf(TEXT("%s parses activation sources: %s"), *FixtureName, *ParseReason),
			ParseCardActivationCandidateSourcesFromFixture(Fixture, Sources, ParseReason));

		const FWBCardActivationCandidateGenerationResult Result =
			WBCardActivationCandidateGenerator::GenerateCandidates(State, Sources);

		FWBApplyActionResult OperationResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString OperationReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *OperationReason),
			ApplyFixtureOperation(Fixture, State, OperationResult, OperationKind, OperationReason));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::GenerateCardActivationCandidates);
		TestEqual(*FString::Printf(TEXT("%s operation ok"), *FixtureName), OperationResult.bOk, Result.bOk);

		const TSharedPtr<FJsonObject> ExpectedObject = GetZoneExpectedObject(Fixture);
		TestTrue(*FString::Printf(TEXT("%s has expected"), *FixtureName), ExpectedObject.IsValid());
		if (ExpectedObject.IsValid())
		{
			bool bExpectedOk = false;
			TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ExpectedObject->TryGetBoolField(TEXT("ok"), bExpectedOk));
			TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), Result.bOk, bExpectedOk);

			double ExpectedCandidateCount = -1.0;
			if (ExpectedObject->TryGetNumberField(TEXT("candidate_count"), ExpectedCandidateCount))
			{
				TestEqual(*FString::Printf(TEXT("%s candidate count"), *FixtureName), Result.Candidates.Num(), static_cast<int32>(ExpectedCandidateCount));
			}
		}
	}

	return true;
}

#endif
