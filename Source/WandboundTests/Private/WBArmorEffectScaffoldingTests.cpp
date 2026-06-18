#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBArmorEffect.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeArmorEffectPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeArmorEffectUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 CurrentArmor,
	const int32 MaxArmor,
	const int32 HP = 5,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 AttacksLeft = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("test_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = 5;
	Unit.ATK = ATK;
	Unit.AR = AR;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = FMath::Max(AttacksLeft, 1);
	Unit.SetArmorForTest(CurrentArmor, MaxArmor);
	return Unit;
}

FWBGameStateData MakeArmorEffectState(const int32 CurrentArmor, const int32 MaxArmor)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 6;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeArmorEffectPlayer(0, 2));
	State.Players.Add(MakeArmorEffectPlayer(1, 0));
	State.AddUnitForTest(MakeArmorEffectUnit(1, 0, FWBTile(4, 4), CurrentArmor, MaxArmor, 5, 2, 1, 2));
	State.AddUnitForTest(MakeArmorEffectUnit(2, 1, FWBTile(5, 4), 0, 0, 5, 1, 1, 1));
	return State;
}

FWBArmorEffectRequest MakeArmorEffectRequest(
	const EWBArmorEffectOp Operation,
	const int32 Amount,
	const int32 TargetUnitId = 1)
{
	FWBArmorEffectRequest Request;
	Request.Operation = Operation;
	Request.TargetUnitId = TargetUnitId;
	Request.Amount = Amount;
	Request.SourceReason = FName(TEXT("test"));
	return Request;
}

FWBAction MakeArmorEffectMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeArmorEffectAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakePlayerOnlyAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString ArmorEffectFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadArmorEffectFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ArmorEffectFixturePath(FileName)))
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

void CompareOptionalIntegerTraceField(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TCHAR* FieldName,
	const int32 ActualValue)
{
	int32 ExpectedValue = 0;
	if (TryReadIntegerField(Expected, FieldName, ExpectedValue))
	{
		Test.TestEqual(*FString::Printf(TEXT("%s %s"), *Label, FieldName), ActualValue, ExpectedValue);
	}
}

bool ExpectTraceFieldsFromFixture(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents)
{
	const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
	const TArray<TSharedPtr<FJsonValue>>* TraceFields = GetArrayField(Expected, TEXT("trace_fields"));
	if (TraceFields == nullptr)
	{
		return true;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s trace field count"), *Label), TraceEvents.Num(), TraceFields->Num());
	const int32 NumToCompare = FMath::Min(TraceEvents.Num(), TraceFields->Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		const TSharedPtr<FJsonObject> ExpectedTrace = (*TraceFields)[Index].IsValid() ? (*TraceFields)[Index]->AsObject() : nullptr;
		if (!ExpectedTrace.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s trace %d malformed expected trace fields"), *Label, Index));
			return false;
		}

		const FWBTraceEvent& ActualTrace = TraceEvents[Index];
		const FString TraceLabel = FString::Printf(TEXT("%s trace %d"), *Label, Index);
		FString ExpectedKind;
		if (ExpectedTrace->TryGetStringField(TEXT("kind"), ExpectedKind))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s kind"), *TraceLabel), ActualTrace.Kind.ToString(), ExpectedKind);
		}

		FString ExpectedOperation;
		if (ExpectedTrace->TryGetStringField(TEXT("armor_effect_operation"), ExpectedOperation))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s operation"), *TraceLabel), ActualTrace.ArmorEffectOperation.ToString(), ExpectedOperation);
		}

		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("target_unit_id"), ActualTrace.TargetUnitId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("player_id"), ActualTrace.PlayerId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_armor"), ActualTrace.PreviousArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_armor"), ActualTrace.NewArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("previous_max_armor"), ActualTrace.PreviousMaxArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("new_max_armor"), ActualTrace.NewMaxArmor);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("armor_effect_amount"), ActualTrace.ArmorEffectAmount);
	}

	return TraceEvents.Num() == TraceFields->Num();
}

TSharedPtr<FJsonObject> FindPublicUnitObject(const TSharedPtr<FJsonObject>& Root, const int32 UnitId)
{
	const TSharedPtr<FJsonObject> BoardSummary = GetObjectField(Root, TEXT("final_public_board_summary"));
	const TArray<TSharedPtr<FJsonValue>>* Units = GetArrayField(BoardSummary, TEXT("units"));
	if (Units == nullptr)
	{
		return nullptr;
	}

	for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
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

bool ExpectRuntimeArmorJson(
	FAutomationTestBase& Test,
	const FString& Label,
	const FString& SerializedJson,
	const int32 UnitId,
	const int32 ExpectedCurrentArmor,
	const int32 ExpectedMaxArmor)
{
	const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
	const TSharedPtr<FJsonObject> UnitObject = FindPublicUnitObject(Root, UnitId);
	Test.TestTrue(*FString::Printf(TEXT("%s unit exists"), *Label), UnitObject.IsValid());
	if (!UnitObject.IsValid())
	{
		return false;
	}

	int32 CurrentArmor = -1;
	int32 MaxArmor = -1;
	Test.TestTrue(*FString::Printf(TEXT("%s current armor reads"), *Label), TryReadIntegerField(UnitObject, TEXT("current_armor"), CurrentArmor));
	Test.TestTrue(*FString::Printf(TEXT("%s max armor reads"), *Label), TryReadIntegerField(UnitObject, TEXT("max_armor"), MaxArmor));
	Test.TestEqual(*FString::Printf(TEXT("%s current armor"), *Label), CurrentArmor, ExpectedCurrentArmor);
	Test.TestEqual(*FString::Printf(TEXT("%s max armor"), *Label), MaxArmor, ExpectedMaxArmor);
	return CurrentArmor == ExpectedCurrentArmor && MaxArmor == ExpectedMaxArmor;
}

bool CompareActionIdArrays(
	FAutomationTestBase& Test,
	const FString& Label,
	const TArray<FString>& Actual,
	const TArray<FString>& Expected)
{
	Test.TestEqual(*FString::Printf(TEXT("%s count"), *Label), Actual.Num(), Expected.Num());
	const int32 NumToCompare = FMath::Min(Actual.Num(), Expected.Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s action %d"), *Label, Index), Actual[Index], Expected[Index]);
	}

	return Actual == Expected;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectAddCurrentClampsToMaxTest, "Wandbound.Core.ArmorEffectScaffolding.AddCurrentClampsToMax", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectAddCurrentClampsToMaxTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 5));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor clamps"), State.GetUnitById(1)->GetCurrentArmor(), 3);
	TestEqual(TEXT("Previous current"), Result.PreviousCurrentArmor, 1);
	TestEqual(TEXT("New current"), Result.NewCurrentArmor, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectAddCurrentMaxZeroStaysZeroTest, "Wandbound.Core.ArmorEffectScaffolding.AddCurrentMaxZeroStaysZero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectAddCurrentMaxZeroStaysZeroTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(0, 0);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor remains zero"), State.GetUnitById(1)->GetCurrentArmor(), 0);
	TestEqual(TEXT("Max armor remains zero"), State.GetUnitById(1)->GetMaxArmor(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectReduceCurrentClampsToZeroTest, "Wandbound.Core.ArmorEffectScaffolding.ReduceCurrentClampsToZero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectReduceCurrentClampsToZeroTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(2, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::ReduceCurrentArmor, 5));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor clamps"), State.GetUnitById(1)->GetCurrentArmor(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectSetCurrentClampsToMaxTest, "Wandbound.Core.ArmorEffectScaffolding.SetCurrentClampsToMax", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectSetCurrentClampsToMaxTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::SetCurrentArmor, 9));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor clamps"), State.GetUnitById(1)->GetCurrentArmor(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectAddMaxDoesNotAutoFillCurrentTest, "Wandbound.Core.ArmorEffectScaffolding.AddMaxDoesNotAutoFillCurrent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectAddMaxDoesNotAutoFillCurrentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddMaxArmor, 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor unchanged"), State.GetUnitById(1)->GetCurrentArmor(), 1);
	TestEqual(TEXT("Max armor increased"), State.GetUnitById(1)->GetMaxArmor(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectReduceMaxClampsCurrentTest, "Wandbound.Core.ArmorEffectScaffolding.ReduceMaxClampsCurrent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectReduceMaxClampsCurrentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(5, 5);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::ReduceMaxArmor, 3));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor clamps to new max"), State.GetUnitById(1)->GetCurrentArmor(), 2);
	TestEqual(TEXT("Max armor reduced"), State.GetUnitById(1)->GetMaxArmor(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectSetMaxClampsCurrentTest, "Wandbound.Core.ArmorEffectScaffolding.SetMaxClampsCurrent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectSetMaxClampsCurrentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(4, 5);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::SetMaxArmor, 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor clamps"), State.GetUnitById(1)->GetCurrentArmor(), 2);
	TestEqual(TEXT("Max armor sets"), State.GetUnitById(1)->GetMaxArmor(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectRestoreToMaxTest, "Wandbound.Core.ArmorEffectScaffolding.RestoreArmorToMax", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectRestoreToMaxTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 4);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::RestoreArmorToMax, 0));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current restored to max"), State.GetUnitById(1)->GetCurrentArmor(), 4);
	TestEqual(TEXT("Max unchanged"), State.GetUnitById(1)->GetMaxArmor(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectMissingTargetFailsTest, "Wandbound.Core.ArmorEffectScaffolding.MissingTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectMissingTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2, 99));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_target_unit")));
	TestEqual(TEXT("Existing armor unchanged"), State.GetUnitById(1)->GetCurrentArmor(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectRemovedTargetFailsTest, "Wandbound.Core.ArmorEffectScaffolding.RemovedTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectRemovedTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	State.GetMutableUnitById(1)->MarkUnitDefeated();
	State.GetMutableUnitById(1)->RemoveUnitFromBoard();

	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("target_unit_removed")));
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(1)->GetCurrentArmor(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectNegativeAmountFailsTest, "Wandbound.Core.ArmorEffectScaffolding.NegativeAmountFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectNegativeAmountFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(2, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::ReduceCurrentArmor, -1));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("negative_armor_effect_amount")));
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(1)->GetCurrentArmor(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectUnknownOperationFailsTest, "Wandbound.Core.ArmorEffectScaffolding.UnknownOperationFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectUnknownOperationFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(2, 3);
	const FWBArmorEffectResult Result = WBArmorEffect::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::Unknown, 1));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("unknown_armor_effect_operation")));
	TestEqual(TEXT("Armor unchanged"), State.GetUnitById(1)->GetCurrentArmor(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectRunnerTraceTest, "Wandbound.Core.ArmorEffectScaffolding.EffectRunnerEmitsArmorModifiedTrace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectRunnerTraceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	const FWBApplyActionResult Result = WBEffectRunner::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestTrue(TEXT("Apply succeeds"), Result.bOk);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		const FWBTraceEvent& Trace = Result.TraceEvents[0];
		TestEqual(TEXT("Trace kind"), Trace.Kind, FName(TEXT("armor_modified")));
		TestEqual(TEXT("Trace target"), Trace.TargetUnitId, 1);
		TestEqual(TEXT("Trace player"), Trace.PlayerId, 0);
		TestEqual(TEXT("Trace previous armor"), Trace.PreviousArmor, 1);
		TestEqual(TEXT("Trace new armor"), Trace.NewArmor, 3);
		TestEqual(TEXT("Trace previous max"), Trace.PreviousMaxArmor, 3);
		TestEqual(TEXT("Trace new max"), Trace.NewMaxArmor, 3);
		TestEqual(TEXT("Trace op"), Trace.ArmorEffectOperation, FName(TEXT("add_current_armor")));
		TestEqual(TEXT("Trace amount"), Trace.ArmorEffectAmount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectPublicSummaryReflectsChangesTest, "Wandbound.Core.ArmorEffectScaffolding.PublicSummaryReflectsArmorChanges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectPublicSummaryReflectsChangesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeArmorEffectState(1, 3);
	const FWBApplyActionResult ApplyResult = WBEffectRunner::ApplyArmorEffect(State, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestTrue(TEXT("Apply succeeds"), ApplyResult.bOk);

	const FWBPublicBoardSummary BoardSummary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Public unit count"), BoardSummary.Units.Num(), 2);
	if (BoardSummary.Units.Num() > 0)
	{
		TestEqual(TEXT("Current armor updated"), BoardSummary.Units[0].CurrentArmor, 3);
		TestEqual(TEXT("Max armor unchanged"), BoardSummary.Units[0].MaxArmor, 3);
	}

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("armor_effect"));
	Envelope.SelectedActionId = TEXT("armor_effect:add_current_armor:u1");
	Envelope.ApplyResult = ApplyResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = BoardSummary;

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestTrue(TEXT("Runtime armor json"), ExpectRuntimeArmorJson(*this, TEXT("runtime armor"), SerializedJson, 1, 3, 3));
	TestFalse(TEXT("Hidden pending attack excluded"), SerializedJson.Contains(TEXT("pending_attack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectLegalGenerationUnchangedTest, "Wandbound.Core.ArmorEffectScaffolding.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeArmorEffectState(1, 3);
	FWBGameStateData MutatedState = MakeArmorEffectState(1, 3);
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));
	WBEffectRunner::ApplyArmorEffect(MutatedState, MakeArmorEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	const TArray<FString> MutatedIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MutatedState, 0));
	CompareActionIdArrays(*this, TEXT("legal action ids"), MutatedIds, BaselineIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectActionCodecUnchangedTest, "Wandbound.Core.ArmorEffectScaffolding.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeArmorEffectMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeArmorEffectAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectFixtureScenariosTest, "Wandbound.Core.ArmorEffectScaffolding.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("armor_effect_add_current_clamps_to_max.json"),
		TEXT("armor_effect_reduce_current_clamps_to_zero.json"),
		TEXT("armor_effect_set_current_clamps_to_max.json"),
		TEXT("armor_effect_add_max_does_not_auto_fill_current.json"),
		TEXT("armor_effect_reduce_max_clamps_current.json"),
		TEXT("armor_effect_set_max_clamps_current.json"),
		TEXT("armor_effect_restore_to_max.json"),
		TEXT("armor_effect_missing_target_fails_no_mutation.json"),
		TEXT("armor_effect_removed_target_fails_no_mutation.json"),
		TEXT("runtime_result_serialization_after_armor_effect.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadArmorEffectFixture(FixtureName, Fixture));
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
		TestTrue(*FString::Printf(TEXT("%s trace fields"), *FixtureName), ExpectTraceFieldsFromFixture(*this, FixtureName, Fixture, ApplyResult.TraceEvents));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBArmorEffectRuntimeSerializationFixtureTest, "Wandbound.Core.ArmorEffectScaffolding.RuntimeSerializationFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBArmorEffectRuntimeSerializationFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("runtime_result_serialization_after_armor_effect.json");
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Runtime armor effect fixture loads"), LoadArmorEffectFixture(FixtureName, Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(*FString::Printf(TEXT("State builds: %s"), *StateReason), BuildGameStateFromFixture(Fixture, State, StateReason));
	if (!StateReason.IsEmpty())
	{
		return false;
	}

	FWBApplyActionResult ApplyResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString ApplyReason;
	TestTrue(TEXT("Armor effect applies"), ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
	TestTrue(TEXT("Armor effect succeeds"), ApplyResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("armor_effect"));
	Envelope.SelectedActionId = TEXT("armor_effect:add_current_armor:u1");
	Envelope.ApplyResult = ApplyResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestTrue(TEXT("Runtime armor JSON matches"), ExpectRuntimeArmorJson(*this, FixtureName, SerializedJson, 1, 3, 3));

	const TSharedPtr<FJsonObject> ExpectedJson = GetObjectField(Fixture, TEXT("expected_json"));
	const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = GetArrayField(ExpectedJson, TEXT("forbidden_substrings"));
	if (ForbiddenSubstrings != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ForbiddenSubstrings)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				continue;
			}

			const FString Forbidden = Value->AsString();
			TestFalse(*FString::Printf(TEXT("Forbidden substring excluded: %s"), *Forbidden), SerializedJson.Contains(Forbidden));
		}
	}

	FString ExpectReason;
	TestTrue(*FString::Printf(TEXT("Trace order: %s"), *ExpectReason), ExpectTraceOrder(Fixture, ApplyResult.TraceEvents, ExpectReason));
	TestTrue(*FString::Printf(TEXT("Final units: %s"), *ExpectReason), ExpectUnitStatusSummary(Fixture, State, ExpectReason));
	return true;
}

#endif
