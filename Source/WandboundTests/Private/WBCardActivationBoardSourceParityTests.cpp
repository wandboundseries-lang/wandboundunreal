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

constexpr int32 BoardSourceUnitId = 1;
constexpr int32 BoardTargetUnitId = 2;

FWBPlayerStateData MakeBoardParityPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeBoardParityUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 4;
	Unit.RLUsed = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeBoardParityState(const FString& SourceCardId = TEXT("fixture_board_card"), const int32 SourceOwnerId = 0)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 12;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeBoardParityPlayer(0));
	State.Players.Add(MakeBoardParityPlayer(1));
	State.AddUnitForTest(MakeBoardParityUnit(BoardSourceUnitId, SourceOwnerId, SourceCardId, FWBTile(4, 4)));
	State.AddUnitForTest(MakeBoardParityUnit(BoardTargetUnitId, 1, TEXT("fixture_target"), FWBTile(5, 4)));
	return State;
}

FWBGenericEffectPayload MakeBoardParityHealPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("fixture"));
	return Payload;
}

FWBCardActivationSourceGateDefinition MakeBoardParityGate(
	const bool bBlockedByFrozen = false,
	const bool bRequiresOwnership = true)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.RequiredZone = EWBCardActivationSourceZone::Board;
	Gate.Timing = EWBCardActivationTimingRequirement::Any;
	Gate.bRequiresFixtureZoneOwnership = true;
	Gate.bRequiresSourceUnit = true;
	Gate.bRequiresSourceUnitOwnership = bRequiresOwnership;
	Gate.bBlockedByStunned = true;
	Gate.bBlockedByFrozen = bBlockedByFrozen;
	Gate.bHasExplicitSourceGate = true;
	return Gate;
}

FWBCardEffectDefinition MakeBoardParityEffect(
	const FString& EffectId,
	const FString& PublicLabel,
	const FWBCardActivationSourceGateDefinition& Gate)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = PublicLabel;
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate = Gate;
	Effect.Payloads.Add(MakeBoardParityHealPayload());
	return Effect;
}

FWBCardDefinition MakeBoardParityDefinition(
	const FString& CardId,
	const FWBCardActivationSourceGateDefinition& Gate,
	const FString& EffectId = TEXT("heal_1"),
	const FString& PublicLabel = TEXT("Heal 1 HP"))
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicLabel;
	Definition.Kind = EWBCardDefinitionKind::Fixture;
	Definition.ActivatedEffects.Add(MakeBoardParityEffect(EffectId, PublicLabel, Gate));
	return Definition;
}

FWBEffectTargetRef MakeBoardParityTarget(const int32 UnitId = BoardTargetUnitId)
{
	FWBEffectTargetRef Target;
	Target.TargetUnitId = UnitId;
	return Target;
}

FWBCardActivationSourceGateContext MakeBoardParityContext(
	const FString& SourceCardId,
	const int32 SourceUnitId = BoardSourceUnitId,
	const bool bIncludeSourceCardId = true)
{
	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceUnitId = SourceUnitId;
	Context.SourceZone = EWBCardActivationSourceZone::Board;
	Context.SourceCardId = bIncludeSourceCardId ? SourceCardId : FString();
	Context.bHasExplicitSourceGateContext = true;
	return Context;
}

FWBCardActivationCandidateSource MakeBoardParitySource(
	const FString& CardId,
	const FWBCardActivationSourceGateDefinition& Gate,
	const int32 SourceUnitId = BoardSourceUnitId,
	const bool bIncludeSourceCardId = true)
{
	FWBCardActivationCandidateSource Source;
	Source.PlayerId = 0;
	Source.SourceUnitId = SourceUnitId;
	Source.CardDefinition = MakeBoardParityDefinition(CardId, Gate);
	Source.CandidateTargets.Add(MakeBoardParityTarget());
	Source.SourceGateContext = MakeBoardParityContext(CardId, SourceUnitId, bIncludeSourceCardId);
	return Source;
}

TArray<FString> BoardParityCandidateIds(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Ids.Add(Candidate.ActivationCandidateId);
	}
	return Ids;
}

TArray<FString> BoardParityPublicLabels(const FWBCardActivationCandidateGenerationResult& Result)
{
	TArray<FString> Labels;
	for (const FWBCardActivationCandidate& Candidate : Result.Candidates)
	{
		Labels.Add(Candidate.PublicLabel);
	}
	return Labels;
}

FString BoardParityFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadBoardParityFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *BoardParityFixturePath(FileName)))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutFixture) && OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetBoardParityExpectedObject(const TSharedPtr<FJsonObject>& Fixture)
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

bool ReadBoardParityStringArray(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FString>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			return false;
		}
		OutValues.Add(Value->AsString());
	}

	return true;
}

FWBAction MakeBoardParityAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = BoardSourceUnitId;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	Action.TargetUnitId = BoardTargetUnitId;
	return Action;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityGateTest, "Wandbound.Core.CardActivationBoardSourceParity.GateReasons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityGateTest::RunTest(const FString& Parameters)
{
	const FWBCardActivationSourceGateDefinition Gate = MakeBoardParityGate();
	FWBGameStateData State = MakeBoardParityState(TEXT("fixture_board_card"));
	FWBCardActivationSourceGateResult Result =
		WBCardActivationSourceGate::EvaluateBoardSourceParity(State, Gate, MakeBoardParityContext(TEXT("fixture_board_card")));
	TestTrue(TEXT("Owned board source card match succeeds"), Result.bOk);

	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(State, Gate, MakeBoardParityContext(TEXT("fixture_board_card"), -1));
	TestFalse(TEXT("Missing source unit id fails"), Result.bOk);
	TestEqual(TEXT("Missing source unit id reason"), Result.Reason, FString(TEXT("board_source_unit_required")));

	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(State, Gate, MakeBoardParityContext(TEXT("fixture_board_card"), 404));
	TestFalse(TEXT("Missing source unit fails"), Result.bOk);
	TestEqual(TEXT("Missing source unit reason"), Result.Reason, FString(TEXT("board_source_unit_missing")));

	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(State, Gate, MakeBoardParityContext(TEXT(""), BoardSourceUnitId));
	TestFalse(TEXT("Missing source card id fails"), Result.bOk);
	TestEqual(TEXT("Missing source card id reason"), Result.Reason, FString(TEXT("board_source_card_id_missing")));

	FWBGameStateData RemovedState = MakeBoardParityState(TEXT("fixture_board_card"));
	RemovedState.GetMutableUnitById(BoardSourceUnitId)->MarkUnitDefeated();
	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(RemovedState, Gate, MakeBoardParityContext(TEXT("fixture_board_card")));
	TestFalse(TEXT("Defeated source unit fails"), Result.bOk);
	TestEqual(TEXT("Defeated source reason"), Result.Reason, FString(TEXT("board_source_unit_removed")));

	FWBGameStateData OffBoardState = MakeBoardParityState(TEXT("fixture_board_card"));
	FWBUnitState* OffBoardUnit = OffBoardState.GetMutableUnitById(BoardSourceUnitId);
	OffBoardUnit->X = -1;
	OffBoardUnit->Y = 4;
	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(OffBoardState, Gate, MakeBoardParityContext(TEXT("fixture_board_card")));
	TestFalse(TEXT("Invalid-position source unit fails"), Result.bOk);
	TestEqual(TEXT("Invalid-position source reason"), Result.Reason, FString(TEXT("board_source_unit_not_on_board")));

	FWBGameStateData WrongOwnerState = MakeBoardParityState(TEXT("fixture_board_card"), 1);
	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(WrongOwnerState, Gate, MakeBoardParityContext(TEXT("fixture_board_card")));
	TestFalse(TEXT("Wrong owner fails"), Result.bOk);
	TestEqual(TEXT("Wrong owner reason"), Result.Reason, FString(TEXT("board_source_owner_mismatch")));

	FWBGameStateData MissingUnitCardState = MakeBoardParityState(TEXT(""));
	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(MissingUnitCardState, Gate, MakeBoardParityContext(TEXT("fixture_board_card")));
	TestFalse(TEXT("Missing unit card id fails"), Result.bOk);
	TestEqual(TEXT("Missing unit card id reason"), Result.Reason, FString(TEXT("board_source_unit_card_id_missing")));

	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(State, Gate, MakeBoardParityContext(TEXT("wrong_fixture_card")));
	TestFalse(TEXT("Card id mismatch fails"), Result.bOk);
	TestEqual(TEXT("Card id mismatch reason"), Result.Reason, FString(TEXT("board_source_card_id_mismatch")));

	FWBCardActivationSourceGateDefinition NoOwnershipGate = MakeBoardParityGate(false, false);
	Result = WBCardActivationSourceGate::EvaluateBoardSourceParity(WrongOwnerState, NoOwnershipGate, MakeBoardParityContext(TEXT("fixture_board_card")));
	TestTrue(TEXT("Ownership disabled allows wrong owner"), Result.bOk);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityCandidateTest, "Wandbound.Core.CardActivationBoardSourceParity.CandidateGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityCandidateTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_card");
	const FWBCardActivationSourceGateDefinition Gate = MakeBoardParityGate();

	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { MakeBoardParitySource(CardId, Gate) });
	TestTrue(TEXT("Board source owned match generation ok"), Result.bOk);
	TestEqual(TEXT("Board source owned match candidate generated"), Result.Candidates.Num(), 1);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { MakeBoardParitySource(CardId, Gate, BoardSourceUnitId, false) });
	TestTrue(TEXT("Card id inherited generation ok"), Result.bOk);
	TestEqual(TEXT("Card id inherited candidate generated"), Result.Candidates.Num(), 1);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { MakeBoardParitySource(TEXT("wrong_fixture_card"), Gate) });
	TestTrue(TEXT("Card id mismatch generation ok"), Result.bOk);
	TestEqual(TEXT("Card id mismatch excluded"), Result.Candidates.Num(), 0);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { MakeBoardParitySource(CardId, Gate, 404) });
	TestTrue(TEXT("Missing source unit generation ok"), Result.bOk);
	TestEqual(TEXT("Missing source unit excluded"), Result.Candidates.Num(), 0);

	FWBGameStateData RemovedState = MakeBoardParityState(CardId);
	RemovedState.GetMutableUnitById(BoardSourceUnitId)->MarkUnitDefeated();
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(RemovedState, { MakeBoardParitySource(CardId, Gate) });
	TestTrue(TEXT("Removed source generation ok"), Result.bOk);
	TestEqual(TEXT("Removed source excluded"), Result.Candidates.Num(), 0);

	FWBGameStateData OffBoardState = MakeBoardParityState(CardId);
	OffBoardState.GetMutableUnitById(BoardSourceUnitId)->X = -1;
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(OffBoardState, { MakeBoardParitySource(CardId, Gate) });
	TestTrue(TEXT("Off-board source generation ok"), Result.bOk);
	TestEqual(TEXT("Off-board source excluded"), Result.Candidates.Num(), 0);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId, 1), { MakeBoardParitySource(CardId, Gate) });
	TestTrue(TEXT("Wrong owner generation ok"), Result.bOk);
	TestEqual(TEXT("Wrong owner excluded"), Result.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityStatusPolicyTest, "Wandbound.Core.CardActivationBoardSourceParity.StatusPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityStatusPolicyTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_card");
	FWBGameStateData State = MakeBoardParityState(CardId);
	State.GetMutableUnitById(BoardSourceUnitId)->AddStatus(FName(TEXT("Stunned")), 1);
	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { MakeBoardParitySource(CardId, MakeBoardParityGate()) });
	TestTrue(TEXT("Stunned source generation ok"), Result.bOk);
	TestEqual(TEXT("Stunned source excluded"), Result.Candidates.Num(), 0);

	FWBGameStateData FrozenState = MakeBoardParityState(CardId);
	FrozenState.GetMutableUnitById(BoardSourceUnitId)->AddStatus(FName(TEXT("Frozen")), 1);
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(FrozenState, { MakeBoardParitySource(CardId, MakeBoardParityGate(false)) });
	TestTrue(TEXT("Frozen allowed generation ok"), Result.bOk);
	TestEqual(TEXT("Frozen allowed by policy"), Result.Candidates.Num(), 1);

	Result = WBCardActivationCandidateGenerator::GenerateCandidates(FrozenState, { MakeBoardParitySource(CardId, MakeBoardParityGate(true)) });
	TestTrue(TEXT("Frozen blocked generation ok"), Result.bOk);
	TestEqual(TEXT("Frozen blocked by policy"), Result.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityEffectContextTest, "Wandbound.Core.CardActivationBoardSourceParity.EffectContextInheritance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityEffectContextTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_card");
	FWBCardActivationSourceGateDefinition Gate = MakeBoardParityGate();
	Gate.CostGate.bRequiresExternalAffordability = true;
	Gate.CostGate.RequiredRR = 1;
	Gate.CostGate.CostKind = FName(TEXT("RR"));

	FWBCardActivationCandidateSource Source = MakeBoardParitySource(CardId, Gate, BoardSourceUnitId, false);
	FWBCardActivationSourceGateContext EffectContext;
	EffectContext.CostContext.bHasExternalAffordability = true;
	EffectContext.CostContext.bExternallyAffordable = true;
	EffectContext.CostContext.SuppliedRequiredRR = 1;
	EffectContext.CostContext.SuppliedAvailableRL = 3;
	EffectContext.CostContext.CostKind = FName(TEXT("RR"));
	EffectContext.bHasExplicitSourceGateContext = true;
	Source.EffectIdToSourceGateContext.Add(TEXT("heal_1"), EffectContext);

	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { Source });
	TestTrue(TEXT("Effect context inherits SourceCardId generation ok"), Result.bOk);
	TestEqual(TEXT("Effect context inherits SourceCardId candidate"), Result.Candidates.Num(), 1);

	Source.EffectIdToSourceGateContext[TEXT("heal_1")].SourceCardId = TEXT("wrong_fixture_card");
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { Source });
	TestTrue(TEXT("Effect context override mismatch generation ok"), Result.bOk);
	TestEqual(TEXT("Effect context override mismatch excluded"), Result.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityNoFixtureContextTest, "Wandbound.Core.CardActivationBoardSourceParity.NoFixtureZoneContextRequired", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityNoFixtureContextTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_card");
	FWBCardActivationCandidateSource Source = MakeBoardParitySource(CardId, MakeBoardParityGate());
	Source.SourceGateContext.FixtureZoneContext.Entries.Reset();

	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { Source });
	TestTrue(TEXT("Board source without fixture zone context generation ok"), Result.bOk);
	TestEqual(TEXT("Board source without fixture zone context candidate generated"), Result.Candidates.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityExistingZonesTest, "Wandbound.Core.CardActivationBoardSourceParity.ExistingZonesUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityExistingZonesTest::RunTest(const FString& Parameters)
{
	FWBCardActivationSourceGateDefinition Gate;
	Gate.bRequiresFixtureZoneOwnership = true;

	FWBCardActivationSourceGateContext Context;
	Context.PlayerId = 0;
	Context.SourceCardId = TEXT("fixture_zone_card");

	Gate.RequiredZone = EWBCardActivationSourceZone::Hand;
	Context.SourceZone = EWBCardActivationSourceZone::Hand;
	Context.FixtureZoneContext.Entries = { { TEXT("fixture_zone_card"), 0, EWBCardActivationSourceZone::Hand, -1 } };
	TestTrue(TEXT("Hand fixture ownership still succeeds"), WBCardActivationSourceGate::EvaluateFixtureZoneOwnership(Gate, Context).bOk);

	Gate.RequiredZone = EWBCardActivationSourceZone::Discard;
	Context.SourceZone = EWBCardActivationSourceZone::Discard;
	Context.FixtureZoneContext.Entries = { { TEXT("fixture_zone_card"), 0, EWBCardActivationSourceZone::Discard, -1 } };
	TestTrue(TEXT("Discard fixture ownership still succeeds"), WBCardActivationSourceGate::EvaluateFixtureZoneOwnership(Gate, Context).bOk);

	Gate.RequiredZone = EWBCardActivationSourceZone::Equipped;
	Context.SourceZone = EWBCardActivationSourceZone::Equipped;
	Context.SourceUnitId = BoardSourceUnitId;
	Context.FixtureZoneContext.Entries = { { TEXT("fixture_zone_card"), 0, EWBCardActivationSourceZone::Equipped, BoardSourceUnitId } };
	TestTrue(TEXT("Equipped fixture ownership still succeeds"), WBCardActivationSourceGate::EvaluateFixtureZoneOwnership(Gate, Context).bOk);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityCompositionTest, "Wandbound.Core.CardActivationBoardSourceParity.CostAndUsageComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityCompositionTest::RunTest(const FString& Parameters)
{
	const FString CardId = TEXT("fixture_board_card");
	FWBCardActivationSourceGateDefinition CostGate = MakeBoardParityGate();
	CostGate.CostGate.bRequiresExternalAffordability = true;
	CostGate.CostGate.RequiredRR = 2;
	CostGate.CostGate.CostKind = FName(TEXT("RR"));

	FWBCardActivationCandidateSource Source = MakeBoardParitySource(CardId, CostGate);
	Source.SourceGateContext.CostContext.bHasExternalAffordability = true;
	Source.SourceGateContext.CostContext.bExternallyAffordable = true;
	Source.SourceGateContext.CostContext.SuppliedRequiredRR = 2;
	Source.SourceGateContext.CostContext.SuppliedAvailableRL = 3;
	Source.SourceGateContext.CostContext.CostKind = FName(TEXT("RR"));
	FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { Source });
	TestTrue(TEXT("Board parity affordable generation ok"), Result.bOk);
	TestEqual(TEXT("Board parity affordable candidate generated"), Result.Candidates.Num(), 1);

	Source.SourceGateContext.CostContext.bExternallyAffordable = false;
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(MakeBoardParityState(CardId), { Source });
	TestTrue(TEXT("Board parity unaffordable generation ok"), Result.bOk);
	TestEqual(TEXT("Board parity unaffordable excluded"), Result.Candidates.Num(), 0);

	FWBCardActivationSourceGateDefinition OnceGate = MakeBoardParityGate();
	OnceGate.bOncePerTurn = true;
	FWBCardActivationCandidateSource OnceSource = MakeBoardParitySource(CardId, OnceGate);
	FWBGameStateData State = MakeBoardParityState(CardId);
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { OnceSource });
	TestTrue(TEXT("Board parity unused once generation ok"), Result.bOk);
	TestEqual(TEXT("Board parity unused once candidate generated"), Result.Candidates.Num(), 1);

	State.MarkActivationUsageKeyForTest(0, WBCardActivationSourceGate::BuildDefaultUsageKey(0, BoardSourceUnitId, CardId, TEXT("heal_1")));
	Result = WBCardActivationCandidateGenerator::GenerateCandidates(State, { OnceSource });
	TestTrue(TEXT("Board parity used once generation ok"), Result.bOk);
	TestEqual(TEXT("Board parity used once excluded"), Result.Candidates.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityNoMutationHiddenAndSeparationTest, "Wandbound.Core.CardActivationBoardSourceParity.NoMutationHiddenAndSeparation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityNoMutationHiddenAndSeparationTest::RunTest(const FString& Parameters)
{
	const FString VisibleCardId = TEXT("visible_board_card_identity");
	const FString HiddenFixtureToken = TEXT("hidden_fixture_zone_token");
	FWBGameStateData State = MakeBoardParityState(VisibleCardId);
	State.Players[0].Hand.Add(TEXT("hidden_real_hand_token"));
	State.Players[0].Deck.Add(TEXT("hidden_real_deck_token"));
	State.Players[0].Discard.Add(TEXT("hidden_real_discard_token"));

	const int32 SourceHPBefore = State.GetUnitById(BoardSourceUnitId)->HP;
	const int32 TargetHPBefore = State.GetUnitById(BoardTargetUnitId)->HP;
	const TArray<FString> HandBefore = State.Players[0].Hand;
	const TArray<FString> DeckBefore = State.Players[0].Deck;
	const TArray<FString> DiscardBefore = State.Players[0].Discard;

	FWBCardActivationCandidateSource Source = MakeBoardParitySource(VisibleCardId, MakeBoardParityGate());
	Source.SourceGateContext.FixtureZoneContext.Entries.Add({ HiddenFixtureToken, 0, EWBCardActivationSourceZone::Hand, -1 });
	const FWBCardActivationCandidateGenerationResult Result =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, { Source });
	TestTrue(TEXT("Hidden metadata generation ok"), Result.bOk);
	TestEqual(TEXT("Hidden metadata candidate generated"), Result.Candidates.Num(), 1);
	TestEqual(TEXT("Source HP not mutated by generation"), State.GetUnitById(BoardSourceUnitId)->HP, SourceHPBefore);
	TestEqual(TEXT("Target HP not mutated by generation"), State.GetUnitById(BoardTargetUnitId)->HP, TargetHPBefore);
	TestTrue(TEXT("Hand not mutated by generation"), State.Players[0].Hand == HandBefore);
	TestTrue(TEXT("Deck not mutated by generation"), State.Players[0].Deck == DeckBefore);
	TestTrue(TEXT("Discard not mutated by generation"), State.Players[0].Discard == DiscardBefore);

	const FWBCardActivationCommandResult ApplyResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Result.Candidates[0].Command);
	TestTrue(TEXT("Apply succeeds for trace/public checks"), ApplyResult.bOk);
	const FString TraceJson = WBReplayTrace::SerializeEvents(ApplyResult.TraceEvents);
	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestFalse(TEXT("Trace excludes ignored fixture zone token"), TraceJson.Contains(HiddenFixtureToken));
	TestFalse(TEXT("Trace excludes hidden hand"), TraceJson.Contains(TEXT("hidden_real_hand_token")));
	TestFalse(TEXT("Trace excludes hidden deck"), TraceJson.Contains(TEXT("hidden_real_deck_token")));
	TestFalse(TEXT("Trace excludes hidden discard"), TraceJson.Contains(TEXT("hidden_real_discard_token")));
	TestEqual(TEXT("Visible source unit card id remains public"), Summary.Units[0].CardId, VisibleCardId);
	TestFalse(TEXT("Public board excludes ignored fixture zone token"), Summary.Units[0].CardId.Contains(HiddenFixtureToken));

	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MakeBoardParityState(VisibleCardId), 0));
	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MakeBoardParityState(VisibleCardId), 0));
	TestEqual(TEXT("FWBAction legal generation unchanged count"), AfterIds.Num(), BaselineIds.Num());
	for (int32 Index = 0; Index < FMath::Min(AfterIds.Num(), BaselineIds.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("FWBAction legal id %d unchanged"), Index), AfterIds[Index], BaselineIds[Index]);
		TestFalse(*FString::Printf(TEXT("FWBAction legal id is not activation %s"), *AfterIds[Index]), AfterIds[Index].StartsWith(TEXT("activate:")));
	}

	TestEqual(TEXT("Move action id unchanged"), WBActionCodec::MakeActionId(MakeBoardParityAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id unchanged"), WBActionCodec::MakeActionId(MakeBoardParityAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id unchanged"), WBActionCodec::MakeActionId(MakeBoardParityAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id unchanged"), WBActionCodec::MakeActionId(MakeBoardParityAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id unchanged"), WBActionCodec::MakeActionId(MakeBoardParityAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardActivationBoardSourceParityFixtureScenariosTest, "Wandbound.Core.CardActivationBoardSourceParity.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardActivationBoardSourceParityFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("card_activation_board_source_owned_card_match_success.json"),
		TEXT("card_activation_board_source_card_id_inherited_success.json"),
		TEXT("card_activation_board_source_card_id_mismatch_excluded.json"),
		TEXT("card_activation_board_source_missing_unit_excluded.json"),
		TEXT("card_activation_board_source_removed_unit_excluded.json"),
		TEXT("card_activation_board_source_not_on_board_excluded.json"),
		TEXT("card_activation_board_source_wrong_owner_excluded.json"),
		TEXT("card_activation_board_source_stunned_excluded.json"),
		TEXT("card_activation_board_source_effect_specific_context_inherits_card_id.json"),
		TEXT("card_activation_board_source_no_fixture_zone_context_required.json"),
		TEXT("card_activation_board_source_composes_with_cost_usage.json"),
		TEXT("card_activation_board_source_hidden_metadata_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadBoardParityFixture(FixtureName, Fixture));
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

		const TSharedPtr<FJsonObject> ExpectedObject = GetBoardParityExpectedObject(Fixture);
		TestTrue(*FString::Printf(TEXT("%s has expected"), *FixtureName), ExpectedObject.IsValid());
		if (!ExpectedObject.IsValid())
		{
			return false;
		}

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ExpectedObject->TryGetBoolField(TEXT("ok"), bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), Result.bOk, bExpectedOk);

		double ExpectedCandidateCount = -1.0;
		TestTrue(*FString::Printf(TEXT("%s expected candidate_count reads"), *FixtureName), ExpectedObject->TryGetNumberField(TEXT("candidate_count"), ExpectedCandidateCount));
		TestEqual(*FString::Printf(TEXT("%s candidate count"), *FixtureName), Result.Candidates.Num(), static_cast<int32>(ExpectedCandidateCount));

		TArray<FString> ExpectedCandidateIds;
		TestTrue(*FString::Printf(TEXT("%s expected candidate_ids reads"), *FixtureName), ReadBoardParityStringArray(ExpectedObject, TEXT("candidate_ids"), ExpectedCandidateIds));
		TestTrue(*FString::Printf(TEXT("%s candidate ids"), *FixtureName), BoardParityCandidateIds(Result) == ExpectedCandidateIds);

		TArray<FString> ExpectedPublicLabels;
		TestTrue(*FString::Printf(TEXT("%s expected public_labels reads"), *FixtureName), ReadBoardParityStringArray(ExpectedObject, TEXT("public_labels"), ExpectedPublicLabels));
		TestTrue(*FString::Printf(TEXT("%s public labels"), *FixtureName), BoardParityPublicLabels(Result) == ExpectedPublicLabels);

		if (ExpectedObject->HasField(TEXT("final_units")))
		{
			FString FinalUnitsReason;
			TestTrue(*FString::Printf(TEXT("%s final units unchanged: %s"), *FixtureName, *FinalUnitsReason), ExpectUnitStatusSummary(Fixture, State, FinalUnitsReason));
		}
	}

	return true;
}

#endif
