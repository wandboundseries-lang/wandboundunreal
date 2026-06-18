#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBEffectRequest.h"
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

FWBPlayerStateData MakeEffectRequestPlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeEffectRequestUnit(
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
	Unit.CardId = FString::Printf(TEXT("fixture_unit_%d"), UnitId);
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

FWBGameStateData MakeEffectRequestState(const int32 TargetCurrentArmor, const int32 TargetMaxArmor)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 7;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeEffectRequestPlayer(0, 2));
	State.Players.Add(MakeEffectRequestPlayer(1, 0));
	State.AddUnitForTest(MakeEffectRequestUnit(1, 0, FWBTile(4, 4), 0, 0, 5, 2, 1, 2));
	State.AddUnitForTest(MakeEffectRequestUnit(2, 1, FWBTile(5, 4), TargetCurrentArmor, TargetMaxArmor, 5, 1, 1, 1));
	return State;
}

FWBGenericEffectPayload MakeArmorPayload(
	const EWBArmorEffectOp Operation,
	const int32 Amount,
	const int32 TargetUnitId = -1)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::ArmorEffect;
	Payload.ArmorEffect.Operation = Operation;
	Payload.ArmorEffect.TargetUnitId = TargetUnitId;
	Payload.ArmorEffect.Amount = Amount;
	Payload.ArmorEffect.SourceReason = FName(TEXT("test"));
	return Payload;
}

FWBEffectRequest MakeEffectRequest(
	const EWBArmorEffectOp Operation,
	const int32 Amount,
	const int32 RequestTargetUnitId = 2,
	const int32 PayloadTargetUnitId = -1,
	const int32 SourceUnitId = 1)
{
	FWBEffectRequest Request;
	Request.Source.PlayerId = 0;
	Request.Source.SourceUnitId = SourceUnitId;
	Request.Source.SourceCardId = TEXT("fixture_card_armor");
	Request.Source.SourceEffectId = TEXT("fixture_effect");
	Request.Target.TargetUnitId = RequestTargetUnitId;
	Request.Payloads.Add(MakeArmorPayload(Operation, Amount, PayloadTargetUnitId));
	return Request;
}

FWBAction MakeEffectRequestMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}

FWBAction MakeEffectRequestAttackAction()
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

FString EffectRequestFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadEffectRequestFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *EffectRequestFixturePath(FileName)))
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

		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("player_id"), ActualTrace.PlayerId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("source_unit_id"), ActualTrace.SourceUnitId);
		CompareOptionalIntegerTraceField(Test, TraceLabel, ExpectedTrace, TEXT("target_unit_id"), ActualTrace.TargetUnitId);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestArmorAddCurrentTest, "Wandbound.Core.EffectRequestScaffolding.ArmorAddCurrentSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestArmorAddCurrentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor"), State.GetUnitById(2)->GetCurrentArmor(), 3);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Parent trace"), Result.TraceEvents[0].Kind, FName(TEXT("effect_request_resolved")));
		TestEqual(TEXT("Child trace"), Result.TraceEvents[1].Kind, FName(TEXT("armor_modified")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestArmorReduceCurrentTest, "Wandbound.Core.EffectRequestScaffolding.ArmorReduceCurrentSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestArmorReduceCurrentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(3, 3);
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::ReduceCurrentArmor, 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestArmorAddMaxThenRestoreTest, "Wandbound.Core.EffectRequestScaffolding.ArmorAddMaxThenRestoreSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestArmorAddMaxThenRestoreTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	FWBEffectRequest Request = MakeEffectRequest(EWBArmorEffectOp::AddMaxArmor, 2, 2, 2);
	Request.Payloads.Add(MakeArmorPayload(EWBArmorEffectOp::RestoreArmorToMax, 0, 2));

	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Max armor"), State.GetUnitById(2)->GetMaxArmor(), 5);
	TestEqual(TEXT("Current armor"), State.GetUnitById(2)->GetCurrentArmor(), 5);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestMultiplePayloadsAtomicOnFailureTest, "Wandbound.Core.EffectRequestScaffolding.MultiplePayloadsAtomicOnFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestMultiplePayloadsAtomicOnFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	FWBEffectRequest Request = MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2, 2, 2);
	FWBGenericEffectPayload UnknownPayload;
	UnknownPayload.Operation = EWBGenericEffectOp::Unknown;
	Request.Payloads.Add(UnknownPayload);

	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("unknown_effect_payload_operation")));
	TestEqual(TEXT("Current armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	TestEqual(TEXT("No traces on failure"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestMissingTargetFailsTest, "Wandbound.Core.EffectRequestScaffolding.MissingTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestMissingTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2, -1));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("missing_effect_target_unit")));
	TestEqual(TEXT("Current armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestRemovedTargetFailsTest, "Wandbound.Core.EffectRequestScaffolding.RemovedTargetFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestRemovedTargetFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	State.GetMutableUnitById(2)->MarkUnitDefeated();
	State.GetMutableUnitById(2)->RemoveUnitFromBoard();

	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("effect_target_unit_removed")));
	TestEqual(TEXT("Current armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestUnknownOperationFailsTest, "Wandbound.Core.EffectRequestScaffolding.UnknownOperationFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestUnknownOperationFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	FWBEffectRequest Request = MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2);
	Request.Payloads.Reset();
	FWBGenericEffectPayload UnknownPayload;
	UnknownPayload.Operation = EWBGenericEffectOp::Unknown;
	Request.Payloads.Add(UnknownPayload);

	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("unknown_effect_payload_operation")));
	TestEqual(TEXT("Current armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestSourceUnitRemovedFailsTest, "Wandbound.Core.EffectRequestScaffolding.SourceUnitRemovedFailsIfProvided", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestSourceUnitRemovedFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	State.GetMutableUnitById(1)->MarkUnitDefeated();
	State.GetMutableUnitById(1)->RemoveUnitFromBoard();

	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestFalse(TEXT("Result fails"), Result.bOk);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("effect_source_unit_removed")));
	TestEqual(TEXT("Current armor unchanged"), State.GetUnitById(2)->GetCurrentArmor(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestSourceUnitOptionalTest, "Wandbound.Core.EffectRequestScaffolding.SourceUnitOptionalSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestSourceUnitOptionalTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2, 2, -1, -1));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Current armor"), State.GetUnitById(2)->GetCurrentArmor(), 3);
	TestEqual(TEXT("Parent source omitted"), Result.TraceEvents[0].SourceUnitId, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestParentTracePrecedesPayloadTraceTest, "Wandbound.Core.EffectRequestScaffolding.ParentTracePrecedesArmorModified", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestParentTracePrecedesPayloadTraceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	const FWBEffectRequestResult Result = WBEffectRunner::ApplyEffectRequest(State, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	TestTrue(TEXT("Result ok"), Result.bOk);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() == 2)
	{
		TestEqual(TEXT("Parent trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("effect_request_resolved")));
		TestEqual(TEXT("Parent player"), Result.TraceEvents[0].PlayerId, 0);
		TestEqual(TEXT("Parent source"), Result.TraceEvents[0].SourceUnitId, 1);
		TestEqual(TEXT("Parent target"), Result.TraceEvents[0].TargetUnitId, 2);
		TestEqual(TEXT("Child trace kind"), Result.TraceEvents[1].Kind, FName(TEXT("armor_modified")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestHiddenDataExclusionTest, "Wandbound.Core.EffectRequestScaffolding.HiddenDataExcludedFromRuntimeSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestHiddenDataExclusionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeEffectRequestState(1, 3);
	State.Players[0].Deck.Add(TEXT("secret_deck_card_token"));
	State.Players[0].Hand.Add(TEXT("secret_hand_card_token"));
	State.Players[0].Discard.Add(TEXT("secret_discard_card_token"));

	FWBEffectRequest Request = MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2);
	Request.Source.SourceCardId = TEXT("secret_source_card_token");
	Request.Source.SourceEffectId = TEXT("secret_source_effect_token");
	const FWBEffectRequestResult EffectResult = WBEffectRunner::ApplyEffectRequest(State, Request);
	TestTrue(TEXT("Effect request succeeds"), EffectResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("effect_request_test"));
	Envelope.SelectedActionId = TEXT("effect_request_test");
	Envelope.ApplyResult.bOk = EffectResult.bOk;
	Envelope.ApplyResult.Reason = EffectResult.Reason;
	Envelope.ApplyResult.TraceEvents = EffectResult.TraceEvents;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestTrue(TEXT("Runtime armor json"), ExpectRuntimeArmorJson(*this, TEXT("effect request runtime"), SerializedJson, 2, 3, 3));

	const TArray<FString> ForbiddenSubstrings = {
		TEXT("secret_deck_card_token"),
		TEXT("secret_hand_card_token"),
		TEXT("secret_discard_card_token"),
		TEXT("secret_source_card_token"),
		TEXT("secret_source_effect_token"),
		TEXT("\"deck\""),
		TEXT("\"hand\""),
		TEXT("\"discard\""),
		TEXT("pending_attack")
	};
	for (const FString& Forbidden : ForbiddenSubstrings)
	{
		TestFalse(*FString::Printf(TEXT("Forbidden substring excluded: %s"), *Forbidden), SerializedJson.Contains(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestLegalGenerationUnchangedTest, "Wandbound.Core.EffectRequestScaffolding.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeEffectRequestState(1, 3);
	FWBGameStateData MutatedState = MakeEffectRequestState(1, 3);
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));
	WBEffectRunner::ApplyEffectRequest(MutatedState, MakeEffectRequest(EWBArmorEffectOp::AddCurrentArmor, 2));
	const TArray<FString> MutatedIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(MutatedState, 0));
	CompareActionIdArrays(*this, TEXT("legal action ids"), MutatedIds, BaselineIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestActionCodecUnchangedTest, "Wandbound.Core.EffectRequestScaffolding.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeEffectRequestMoveAction()), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeEffectRequestAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::EndTurn, 0)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::Pass, 0)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakePlayerOnlyAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestFixtureScenariosTest, "Wandbound.Core.EffectRequestScaffolding.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("effect_request_armor_add_current.json"),
		TEXT("effect_request_armor_reduce_current.json"),
		TEXT("effect_request_armor_add_max_then_restore.json"),
		TEXT("effect_request_multiple_payloads_atomic_success.json"),
		TEXT("effect_request_multiple_payloads_atomic_failure.json"),
		TEXT("effect_request_missing_target_fails_no_mutation.json"),
		TEXT("effect_request_removed_target_fails_no_mutation.json"),
		TEXT("effect_request_unknown_operation_fails_no_mutation.json"),
		TEXT("runtime_result_serialization_after_effect_request_armor.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadEffectRequestFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			return false;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(
			*FString::Printf(TEXT("%s builds state"), *FixtureName),
			BuildGameStateFromFixture(Fixture, State, StateReason));
		TestTrue(*FString::Printf(TEXT("%s state reason empty: %s"), *FixtureName, *StateReason), StateReason.IsEmpty());
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
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), OperationKind, EWBFixtureOperationKind::ApplyEffectRequest);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBEffectRequestRuntimeSerializationFixtureTest, "Wandbound.Core.EffectRequestScaffolding.RuntimeSerializationFixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBEffectRequestRuntimeSerializationFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixtureName = TEXT("runtime_result_serialization_after_effect_request_armor.json");
	TSharedPtr<FJsonObject> Fixture;
	TestTrue(TEXT("Runtime effect request fixture loads"), LoadEffectRequestFixture(FixtureName, Fixture));
	if (!Fixture.IsValid())
	{
		return false;
	}

	FWBGameStateData State;
	FString StateReason;
	TestTrue(TEXT("State builds"), BuildGameStateFromFixture(Fixture, State, StateReason));
	TestTrue(*FString::Printf(TEXT("State reason empty: %s"), *StateReason), StateReason.IsEmpty());
	if (!StateReason.IsEmpty())
	{
		return false;
	}

	FWBApplyActionResult ApplyResult;
	EWBFixtureOperationKind OperationKind = EWBFixtureOperationKind::Unknown;
	FString ApplyReason;
	TestTrue(TEXT("Effect request applies"), ApplyFixtureOperation(Fixture, State, ApplyResult, OperationKind, ApplyReason));
	TestTrue(TEXT("Effect request succeeds"), ApplyResult.bOk);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(TEXT("effect_request_test"));
	Envelope.SelectedActionId = TEXT("effect_request_test");
	Envelope.ApplyResult = ApplyResult;
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);

	FString SerializedJson;
	TestTrue(TEXT("Runtime result serializes"), WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestTrue(TEXT("Runtime armor JSON matches"), ExpectRuntimeArmorJson(*this, FixtureName, SerializedJson, 2, 3, 3));

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
