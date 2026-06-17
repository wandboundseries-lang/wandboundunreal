#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakeGenerationUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 AR = 1,
	const int32 AttacksLeft = 1,
	const int32 MPRemaining = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.AR = AR;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = FMath::Max(AttacksLeft, 1);
	Unit.MPRemaining = MPRemaining;
	return Unit;
}

FWBGameStateData MakeGenerationState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.AddUnitForTest(MakeGenerationUnit(1, 0, FWBTile(4, 4), 2, 1, 2));
	State.AddUnitForTest(MakeGenerationUnit(2, 1, FWBTile(5, 4), 1, 1, 0));
	return State;
}

FWBAction MakeGenerationAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FWBAction MakeGenerationMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeGenerationAttackAction(const int32 PlayerId = 0, const int32 SourceUnitId = 1, const int32 TargetUnitId = 2)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = SourceUnitId;
	Action.TargetUnitId = TargetUnitId;
	return Action;
}

bool ContainsActionId(const TArray<FWBAction>& Actions, const FString& ExpectedActionId)
{
	return WBActionCodec::MakeActionIds(Actions).Contains(ExpectedActionId);
}

int32 FirstIndexOfType(const TArray<FWBAction>& Actions, const EWBActionType Type)
{
	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		if (Actions[Index].Type == Type)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 LastIndexOfType(const TArray<FWBAction>& Actions, const EWBActionType Type)
{
	for (int32 Index = Actions.Num() - 1; Index >= 0; --Index)
	{
		if (Actions[Index].Type == Type)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool ContainsType(const TArray<FWBAction>& Actions, const EWBActionType Type)
{
	return FirstIndexOfType(Actions, Type) != INDEX_NONE;
}

TArray<FString> AttackActionIds(const TArray<FWBAction>& Actions)
{
	TArray<FString> AttackIds;
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == EWBActionType::Attack)
		{
			AttackIds.Add(WBActionCodec::MakeActionId(Action));
		}
	}

	return AttackIds;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

bool LoadLegalActionFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(
			Json,
			*FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

bool ReadExpectedLegalActionIds(const TSharedPtr<FJsonObject>& Fixture, TArray<FString>& OutIds)
{
	OutIds.Reset();
	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		|| ExpectedObject == nullptr
		|| !ExpectedObject->IsValid()
		|| !(*ExpectedObject)->TryGetArrayField(TEXT("legal_action_ids"), Values)
		|| Values == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			return false;
		}
		OutIds.Add(Value->AsString());
	}

	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackActionCodecTest, "Wandbound.Core.AttackLegalActionGeneration.ActionCodec", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackActionCodecTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGenerationState();
	const FWBAction Move = MakeGenerationMoveAction();
	const FWBAction Attack = MakeGenerationAttackAction();
	const FWBAction EndTurn = MakeGenerationAction(EWBActionType::EndTurn, 0);
	const FWBAction Pass = MakeGenerationAction(EWBActionType::Pass, 1);
	const FWBAction PassResponse = MakeGenerationAction(EWBActionType::PassResponse, 1);

	TestEqual(TEXT("Move ID unchanged"), WBActionCodec::MakeActionId(Move), FString(TEXT("move:p0:u1:4,4->5,4")));
	TestEqual(TEXT("EndTurn ID unchanged"), WBActionCodec::MakeActionId(EndTurn), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass ID unchanged"), WBActionCodec::MakeActionId(Pass), FString(TEXT("pass:p1")));
	TestEqual(TEXT("PassResponse ID unchanged"), WBActionCodec::MakeActionId(PassResponse), FString(TEXT("pass_response:p1")));
	TestEqual(TEXT("Attack ID stable"), WBActionCodec::MakeActionId(Attack), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("Attack kind"), WBActionCodec::GetActionKind(EWBActionType::Attack), FString(TEXT("attack")));

	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(WBActionCodec::SerializeAction(Attack), State);
	TestTrue(TEXT("Attack decodes"), DecodeResult.bOk);
	TestEqual(TEXT("Decoded attack type"), static_cast<int32>(DecodeResult.Action.Type), static_cast<int32>(EWBActionType::Attack));
	TestEqual(TEXT("Decoded attack target"), DecodeResult.Action.TargetUnitId, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsIncludeLegalAttacksTest, "Wandbound.Core.AttackLegalActionGeneration.IncludesLegalAttacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsIncludeLegalAttacksTest::RunTest(const FString& Parameters)
{
	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(MakeGenerationState(), 0);
	TestTrue(TEXT("Legal attack generated"), ContainsActionId(Actions, TEXT("attack:p0:u1:t2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsExcludeIllegalAttacksTest, "Wandbound.Core.AttackLegalActionGeneration.ExcludesIllegalAttacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsExcludeIllegalAttacksTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGenerationState();
	State.GetMutableUnitById(1)->AttacksLeft = 0;

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(State, 0);
	TestFalse(TEXT("Illegal attack not generated"), ContainsActionId(Actions, TEXT("attack:p0:u1:t2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsFamilyOrderTest, "Wandbound.Core.AttackLegalActionGeneration.FamilyOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsFamilyOrderTest::RunTest(const FString& Parameters)
{
	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(MakeGenerationState(), 0);

	const int32 LastMoveIndex = LastIndexOfType(Actions, EWBActionType::Move);
	const int32 FirstAttackIndex = FirstIndexOfType(Actions, EWBActionType::Attack);
	const int32 EndTurnIndex = FirstIndexOfType(Actions, EWBActionType::EndTurn);

	TestTrue(TEXT("Move actions exist"), LastMoveIndex != INDEX_NONE);
	TestTrue(TEXT("Attack actions exist"), FirstAttackIndex != INDEX_NONE);
	TestTrue(TEXT("EndTurn exists"), EndTurnIndex != INDEX_NONE);
	TestTrue(TEXT("Moves remain before attacks"), LastMoveIndex < FirstAttackIndex);
	TestTrue(TEXT("Attacks remain before EndTurn"), FirstAttackIndex < EndTurnIndex);
	TestEqual(TEXT("EndTurn remains last"), EndTurnIndex, Actions.Num() - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsOrderingDeterministicTest, "Wandbound.Core.AttackLegalActionGeneration.AttackOrderingDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsOrderingDeterministicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.AddUnitForTest(MakeGenerationUnit(10, 0, FWBTile(4, 4), 3, 1, 1));
	State.AddUnitForTest(MakeGenerationUnit(11, 0, FWBTile(2, 2), 3, 1, 1));
	State.AddUnitForTest(MakeGenerationUnit(20, 1, FWBTile(4, 2), 1, 1, 0));
	State.AddUnitForTest(MakeGenerationUnit(21, 1, FWBTile(2, 0), 1, 1, 0));

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(State, 0);
	const TArray<FWBAction> RepeatedActions = WBRules::GenerateLegalActionsForPlayer(State, 0);
	TestEqual(TEXT("Generation deterministic count"), Actions.Num(), RepeatedActions.Num());
	TestTrue(TEXT("Generation deterministic IDs"), WBActionCodec::MakeActionIds(Actions) == WBActionCodec::MakeActionIds(RepeatedActions));

	const TArray<FString> ExpectedAttackIds = {
		TEXT("attack:p0:u11:t21"),
		TEXT("attack:p0:u11:t20"),
		TEXT("attack:p0:u10:t20")
	};
	TestTrue(TEXT("Attack IDs ordered by attacker then target tile"), AttackActionIds(Actions) == ExpectedAttackIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsNonCurrentPlayerGetsNoAttacksTest, "Wandbound.Core.AttackLegalActionGeneration.NonCurrentPlayerGetsNoAttacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsNonCurrentPlayerGetsNoAttacksTest::RunTest(const FString& Parameters)
{
	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(MakeGenerationState(), 1);
	TestFalse(TEXT("Non-current player gets no attacks"), ContainsType(Actions, EWBActionType::Attack));
	TestEqual(TEXT("Non-current player gets no normal actions"), Actions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsResponsePhaseNoAttacksTest, "Wandbound.Core.AttackLegalActionGeneration.ResponsePhaseNoAttacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsResponsePhaseNoAttacksTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeGenerationState();
	State.Phase = EWBGamePhase::Response;
	State.PriorityPlayer = 1;

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(State, 1);
	TestFalse(TEXT("Response phase generates no attacks"), ContainsType(Actions, EWBActionType::Attack));
	TestEqual(TEXT("Response phase keeps pass response only"), Actions.Num(), 1);
	if (Actions.Num() == 1)
	{
		TestEqual(TEXT("Response action is PassResponse"), static_cast<int32>(Actions[0].Type), static_cast<int32>(EWBActionType::PassResponse));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackLegalActionsFixtureExpectedIdsTest, "Wandbound.Core.AttackLegalActionGeneration.FixtureExpectedIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackLegalActionsFixtureExpectedIdsTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Legal action fixture loads"), LoadLegalActionFixture(TEXT("legal_actions_include_attacks_ordered.json"), Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(TEXT("Legal action fixture builds state"), BuildGameStateFromFixture(Fixture, State, StateReason));
	TestTrue(TEXT("Legal action fixture state reason empty"), StateReason.IsEmpty());

	TArray<FString> ExpectedIds;
	TestTrue(TEXT("Expected legal action IDs read"), ReadExpectedLegalActionIds(Fixture, ExpectedIds));

	const TArray<FString> ActualIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	TestEqual(TEXT("Fixture legal action count"), ActualIds.Num(), ExpectedIds.Num());
	TestTrue(TEXT("Fixture legal action IDs match"), ActualIds == ExpectedIds);
	return true;
}

#endif
