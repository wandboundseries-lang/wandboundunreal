#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakeAttackUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 AR = 1,
	const int32 AttacksLeft = 1,
	const int32 HP = 5)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = HP;
	Unit.AR = AR;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = FMath::Max(AttacksLeft, 1);
	return Unit;
}

FWBGameStateData MakeAttackState(
	const FWBTile& AttackerTile = FWBTile(4, 4),
	const FWBTile& DefenderTile = FWBTile(5, 4),
	const int32 AttackerAR = 1,
	const int32 AttacksLeft = 1,
	const int32 DefenderOwnerId = 1)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.RemainingMP = 2;
	State.Players.Add(Player0);
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeAttackUnit(1, 0, AttackerTile, AttackerAR, AttacksLeft));
	State.AddUnitForTest(MakeAttackUnit(2, DefenderOwnerId, DefenderTile, 1, 1));
	return State;
}

FWBAction MakeAttackAction(const int32 PlayerId = 0, const int32 AttackerId = 1, const int32 TargetId = 2)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = AttackerId;
	Action.TargetUnitId = TargetId;
	return Action;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString GoldenScenarioPath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadAttackFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GoldenScenarioPath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		&& Child->IsValid())
	{
		return *Child;
	}

	return nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Values))
	{
		return Values;
	}

	return nullptr;
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
}

bool ReadExpectedOk(const TSharedPtr<FJsonObject>& Fixture, bool& bOutOk)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	return Expected.IsValid() && Expected->TryGetBoolField(TEXT("ok"), bOutOk);
}

FString ReadExpectedReasonContains(const TSharedPtr<FJsonObject>& Fixture)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	FString ReasonContains;
	if (Expected.IsValid())
	{
		Expected->TryGetStringField(TEXT("reason_contains"), ReasonContains);
	}
	return ReasonContains;
}

TSharedPtr<FJsonObject> FindUnitSummaryObject(
	const TArray<TSharedPtr<FJsonValue>>& Units,
	const int32 UnitId)
{
	for (const TSharedPtr<FJsonValue>& UnitValue : Units)
	{
		const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
		int32 CandidateUnitId = -1;
		if (TryReadIntegerField(UnitObject, TEXT("unit_id"), CandidateUnitId) && CandidateUnitId == UnitId)
		{
			return UnitObject;
		}
	}

	return nullptr;
}

bool RuntimeJsonUnitFieldEquals(
	const TSharedPtr<FJsonObject>& Root,
	const int32 UnitId,
	const TCHAR* FieldName,
	const int32 ExpectedValue)
{
	const TSharedPtr<FJsonObject> BoardSummary = GetObjectField(Root, TEXT("final_public_board_summary"));
	const TArray<TSharedPtr<FJsonValue>>* Units = GetArrayField(BoardSummary, TEXT("units"));
	if (Units == nullptr)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Unit = FindUnitSummaryObject(*Units, UnitId);
	int32 ActualValue = -1;
	return TryReadIntegerField(Unit, FieldName, ActualValue) && ActualValue == ExpectedValue;
}

bool SerializedTraceHasAttackBudgetFields(const TSharedPtr<FJsonObject>& Root)
{
	const TArray<TSharedPtr<FJsonValue>>* Traces = GetArrayField(Root, TEXT("traces"));
	if (Traces == nullptr || Traces->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Trace = (*Traces)[0].IsValid() ? (*Traces)[0]->AsObject() : nullptr;
	int32 Before = -1;
	int32 After = -1;
	return TryReadIntegerField(Trace, TEXT("attacks_left_before"), Before)
		&& TryReadIntegerField(Trace, TEXT("attacks_left_after"), After)
		&& Before == 2
		&& After == 1;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationAdjacentLegalTest, "Wandbound.Core.AttackDeclaration.AdjacentOrthogonalLegal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationAdjacentLegalTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeAttackState();
	TestTrue(TEXT("Adjacent attack legal"), WBRules::CanDeclareAttack(State, MakeAttackAction()).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationSameRowLegalTest, "Wandbound.Core.AttackDeclaration.SameRowWithinARLegal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationSameRowLegalTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeAttackState(FWBTile(1, 4), FWBTile(4, 4), 3);
	TestTrue(TEXT("Same row within AR legal"), WBRules::CanDeclareAttack(State, MakeAttackAction()).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationSameColumnLegalTest, "Wandbound.Core.AttackDeclaration.SameColumnWithinARLegal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationSameColumnLegalTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeAttackState(FWBTile(4, 1), FWBTile(4, 4), 3);
	TestTrue(TEXT("Same column within AR legal"), WBRules::CanDeclareAttack(State, MakeAttackAction()).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationOutOfRangeFailsTest, "Wandbound.Core.AttackDeclaration.OutOfRangeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationOutOfRangeFailsTest::RunTest(const FString& Parameters)
{
	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(
		MakeAttackState(FWBTile(4, 4), FWBTile(7, 4), 2),
		MakeAttackAction());
	TestFalse(TEXT("Out-of-range attack fails"), Result.bOk);
	TestEqual(TEXT("Out-of-range reason"), Result.Reason, FString(TEXT("out_of_range")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationDiagonalFailsTest, "Wandbound.Core.AttackDeclaration.DiagonalFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationDiagonalFailsTest::RunTest(const FString& Parameters)
{
	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(
		MakeAttackState(FWBTile(4, 4), FWBTile(5, 5), 2),
		MakeAttackAction());
	TestFalse(TEXT("Diagonal attack fails"), Result.bOk);
	TestEqual(TEXT("Diagonal reason"), Result.Reason, FString(TEXT("not_in_line")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationWallBlockedFailsTest, "Wandbound.Core.AttackDeclaration.WallBlockedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationWallBlockedFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState();
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(5, 4)));

	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(State, MakeAttackAction());
	TestFalse(TEXT("Wall-blocked attack fails"), Result.bOk);
	TestEqual(TEXT("Wall-blocked reason"), Result.Reason, FString(TEXT("blocked_by_wall")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationUnitBlockedFailsTest, "Wandbound.Core.AttackDeclaration.UnitBlockedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationUnitBlockedFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState(FWBTile(4, 4), FWBTile(7, 4), 3);
	TestTrue(TEXT("Intervening unit added"), State.AddUnitForTest(MakeAttackUnit(3, 1, FWBTile(5, 4))));

	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(State, MakeAttackAction());
	TestFalse(TEXT("Unit-blocked attack fails"), Result.bOk);
	TestEqual(TEXT("Unit-blocked reason"), Result.Reason, FString(TEXT("blocked_by_unit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationStunnedFailsTest, "Wandbound.Core.AttackDeclaration.StunnedFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationStunnedFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Stunned")), 1);

	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(State, MakeAttackAction());
	TestFalse(TEXT("Stunned attacker fails"), Result.bOk);
	TestEqual(TEXT("Stunned reason"), Result.Reason, FString(TEXT("cannot_attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationCannotAttackFailsTest, "Wandbound.Core.AttackDeclaration.CannotAttackFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationCannotAttackFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("CannotAttack")), 1);

	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(State, MakeAttackAction());
	TestFalse(TEXT("CannotAttack attacker fails"), Result.bOk);
	TestEqual(TEXT("CannotAttack reason"), Result.Reason, FString(TEXT("cannot_attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationNoAttacksLeftFailsTest, "Wandbound.Core.AttackDeclaration.NoAttacksLeftFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationNoAttacksLeftFailsTest::RunTest(const FString& Parameters)
{
	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(
		MakeAttackState(FWBTile(4, 4), FWBTile(5, 4), 1, 0),
		MakeAttackAction());
	TestFalse(TEXT("No attacks left fails"), Result.bOk);
	TestEqual(TEXT("No attacks left reason"), Result.Reason, FString(TEXT("no_attacks_left")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationFriendlyTargetFailsTest, "Wandbound.Core.AttackDeclaration.FriendlyTargetFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationFriendlyTargetFailsTest::RunTest(const FString& Parameters)
{
	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(
		MakeAttackState(FWBTile(4, 4), FWBTile(5, 4), 1, 1, 0),
		MakeAttackAction());
	TestFalse(TEXT("Friendly target fails"), Result.bOk);
	TestEqual(TEXT("Friendly target reason"), Result.Reason, FString(TEXT("friendly_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationMissingAttackerFailsTest, "Wandbound.Core.AttackDeclaration.MissingAttackerFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationMissingAttackerFailsTest::RunTest(const FString& Parameters)
{
	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(MakeAttackState(), MakeAttackAction(0, 99, 2));
	TestFalse(TEXT("Missing attacker fails"), Result.bOk);
	TestEqual(TEXT("Missing attacker reason"), Result.Reason, FString(TEXT("no_attacker")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationMissingDefenderFailsTest, "Wandbound.Core.AttackDeclaration.MissingDefenderFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationMissingDefenderFailsTest::RunTest(const FString& Parameters)
{
	const FWBActionQueryResult Result = WBRules::CanDeclareAttack(MakeAttackState(), MakeAttackAction(0, 1, 99));
	TestFalse(TEXT("Missing defender fails"), Result.bOk);
	TestEqual(TEXT("Missing defender reason"), Result.Reason, FString(TEXT("no_target")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationFailedDoesNotMutateTest, "Wandbound.Core.AttackDeclaration.FailedAttackDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationFailedDoesNotMutateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState(FWBTile(4, 4), FWBTile(7, 4), 2);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAttackDeclare(State, MakeAttackAction());
	TestFalse(TEXT("Illegal attack apply fails"), Result.bOk);
	TestEqual(TEXT("Illegal attack emits no trace"), Result.TraceEvents.Num(), 0);
	TestEqual(TEXT("AttacksLeft unchanged after failure"), State.GetUnitById(1)->AttacksLeft, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationSuccessfulDecrementsTest, "Wandbound.Core.AttackDeclaration.SuccessfulAttackDecrementsAttacksLeft", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationSuccessfulDecrementsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState(FWBTile(4, 4), FWBTile(5, 4), 1, 2);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAttackDeclare(State, MakeAttackAction());
	TestTrue(TEXT("Legal attack applies"), Result.bOk);
	TestEqual(TEXT("AttacksLeft decremented"), State.GetUnitById(1)->AttacksLeft, 1);
	TestEqual(TEXT("Target HP unchanged"), State.GetUnitById(2)->HP, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationTraceTest, "Wandbound.Core.AttackDeclaration.SuccessfulAttackTrace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationTraceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState(FWBTile(4, 4), FWBTile(5, 4), 1, 2);

	const FWBApplyActionResult Result = WBEffectRunner::ApplyAttackDeclare(State, MakeAttackAction());
	TestTrue(TEXT("Legal attack applies"), Result.bOk);
	TestEqual(TEXT("One attack trace emitted"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Trace = Result.TraceEvents[0];
		TestEqual(TEXT("Trace kind"), Trace.Kind, FName(TEXT("attack_declared")));
		TestEqual(TEXT("Trace player"), Trace.PlayerId, 0);
		TestEqual(TEXT("Trace source"), Trace.SourceUnitId, 1);
		TestEqual(TEXT("Trace target"), Trace.TargetUnitId, 2);
		TestTrue(TEXT("Trace from tile"), Trace.FromTile == FWBTile(4, 4));
		TestTrue(TEXT("Trace to tile"), Trace.ToTile == FWBTile(5, 4));
		TestEqual(TEXT("Trace attacks before"), Trace.AttacksLeftBefore, 2);
		TestEqual(TEXT("Trace attacks after"), Trace.AttacksLeftAfter, 1);
		TestTrue(TEXT("Trace ok"), Trace.bOk);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationFixtureScenariosTest, "Wandbound.Core.AttackDeclaration.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("attack_declare_adjacent_legal.json"),
		TEXT("attack_declare_out_of_range_fails.json"),
		TEXT("attack_declare_diagonal_fails.json"),
		TEXT("attack_declare_wall_blocked_fails.json"),
		TEXT("attack_declare_stunned_fails.json"),
		TEXT("attack_declare_no_attacks_left_fails.json"),
		TEXT("attack_declare_friendly_target_fails.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadAttackFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s builds state: %s"), *FixtureName, *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
		if (!StateReason.IsEmpty())
		{
			return false;
		}

		FWBApplyActionResult ApplyResult;
		EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
		FString ApplyReason;
		TestTrue(
			*FString::Printf(TEXT("%s applies fixture operation: %s"), *FixtureName, *ApplyReason),
			ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));

		bool bExpectedOk = false;
		TestTrue(*FString::Printf(TEXT("%s expected ok reads"), *FixtureName), ReadExpectedOk(Fixture, bExpectedOk));
		TestEqual(*FString::Printf(TEXT("%s ok"), *FixtureName), ApplyResult.bOk, bExpectedOk);

		const FString ExpectedReasonContains = ReadExpectedReasonContains(Fixture);
		if (!ExpectedReasonContains.IsEmpty())
		{
			TestTrue(
				*FString::Printf(TEXT("%s reason contains %s"), *FixtureName, *ExpectedReasonContains),
				ApplyResult.Reason.Contains(ExpectedReasonContains));
		}

		FString ExpectReason;
		TestTrue(*FString::Printf(TEXT("%s trace order: %s"), *FixtureName, *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
		TestTrue(*FString::Printf(TEXT("%s final units: %s"), *FixtureName, *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBAttackDeclarationRuntimeSelectedAttackSerializesTest, "Wandbound.Core.AttackDeclaration.RuntimeSelectedAttackSerializes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBAttackDeclarationRuntimeSelectedAttackSerializesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeAttackState(FWBTile(4, 4), FWBTile(5, 4), 1, 2);
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(6);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeAttackAction(),
		Context);

	FString SerializedJson;
	TestTrue(
		TEXT("Runtime attack result serializes"),
		WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
	TestTrue(TEXT("Serialized attack result parses"), Root.IsValid());

	const TSharedPtr<FJsonObject> SelectedAction = GetObjectField(Root, TEXT("selected_action"));
	FString SelectedActionType;
	FString SelectedActionId;
	SelectedAction->TryGetStringField(TEXT("type"), SelectedActionType);
	SelectedAction->TryGetStringField(TEXT("action_id"), SelectedActionId);
	TestEqual(TEXT("Selected attack type"), SelectedActionType, FString(TEXT("attack")));
	TestEqual(TEXT("Selected attack id"), SelectedActionId, FString(TEXT("attack:p0:u1:t2")));

	const TSharedPtr<FJsonObject> MPRoll = GetObjectField(Root, TEXT("mp_roll"));
	bool bConsumed = true;
	MPRoll->TryGetBoolField(TEXT("consumed"), bConsumed);
	TestFalse(TEXT("Attack consumes no runtime MP roll"), bConsumed);
	TestEqual(TEXT("Queued roll remains"), RollSource.NumRemainingRolls(), 1);

	TestTrue(TEXT("Runtime attack trace has attack budget fields"), SerializedTraceHasAttackBudgetFields(Root));
	TestTrue(TEXT("Attacker board summary AttacksLeft decremented"), RuntimeJsonUnitFieldEquals(Root, 1, TEXT("attacks_left"), 1));
	TestTrue(TEXT("Attacker HP unchanged"), RuntimeJsonUnitFieldEquals(Root, 1, TEXT("hp"), 5));
	TestTrue(TEXT("Target HP unchanged"), RuntimeJsonUnitFieldEquals(Root, 2, TEXT("hp"), 5));
	TestFalse(TEXT("No pending attack serialized"), SerializedJson.Contains(TEXT("pending_attack")));
	TestFalse(TEXT("No damage serialized"), SerializedJson.Contains(TEXT("damage")));
	return true;
}

#endif
