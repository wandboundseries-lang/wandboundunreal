#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBMPRollSource.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeSerializationPlayer(
	const int32 PlayerId,
	const int32 RemainingMP = 0,
	const int32 LastMPRoll = 0,
	const int32 WallsLeft = 0,
	const int32 WallRemovalsLeft = 0)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = LastMPRoll;
	Player.WallsLeft = WallsLeft;
	Player.WallRemovalsLeft = WallRemovalsLeft;
	return Player;
}

FWBUnitState MakeSerializationUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 AttacksLeft = 0,
	const int32 MaxAttacksPerTurn = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = MaxAttacksPerTurn;
	return Unit;
}

FWBGameStateData MakeSerializationState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeSerializationPlayer(0, 2, 2));
	State.Players.Add(MakeSerializationPlayer(1));
	return State;
}

FWBGameStateData MakeSerializationStatusState()
{
	FWBGameStateData State = MakeSerializationState();
	FWBUnitState BurnUnit = MakeSerializationUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeSerializationUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(BurnUnit);
	State.AddUnitForTest(PoisonUnit);
	return State;
}

FWBAction MakeSerializationAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FWBAction MakeSerializationMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
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

TSharedPtr<FJsonObject> FindPlayerSummaryObject(
	const TArray<TSharedPtr<FJsonValue>>& Players,
	const int32 PlayerId)
{
	for (const TSharedPtr<FJsonValue>& PlayerValue : Players)
	{
		const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
		int32 CandidatePlayerId = -1;
		if (TryReadIntegerField(PlayerObject, TEXT("player_id"), CandidatePlayerId) && CandidatePlayerId == PlayerId)
		{
			return PlayerObject;
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> SerializeEnvelopeToObject(const FWBRuntimeSelectedActionResult& Envelope)
{
	return WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonObject(Envelope);
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString RuntimeResultSerializationFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadRuntimeResultSerializationFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *RuntimeResultSerializationFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

FString TraceLabelFromJsonObject(const TSharedPtr<FJsonObject>& TraceObject)
{
	FString Kind;
	if (!TraceObject.IsValid() || !TraceObject->TryGetStringField(TEXT("kind"), Kind))
	{
		return FString();
	}

	FString StatusId;
	if (TraceObject->TryGetStringField(TEXT("status_id"), StatusId) && !StatusId.IsEmpty())
	{
		return FString::Printf(TEXT("%s:%s"), *Kind, *StatusId);
	}

	return Kind;
}

bool TracesContainKind(const TSharedPtr<FJsonObject>& Root, const FString& ExpectedKind)
{
	const TArray<TSharedPtr<FJsonValue>>* Traces = GetArrayField(Root, TEXT("traces"));
	if (Traces == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& TraceValue : *Traces)
	{
		const TSharedPtr<FJsonObject> TraceObject = TraceValue.IsValid() ? TraceValue->AsObject() : nullptr;
		FString Kind;
		if (TraceObject.IsValid() && TraceObject->TryGetStringField(TEXT("kind"), Kind) && Kind == ExpectedKind)
		{
			return true;
		}
	}

	return false;
}

void ExpectSerializedPublicPlayer(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& SummaryObject,
	const int32 PlayerId,
	const int32 ExpectedRemainingMP,
	const int32 ExpectedLastMPRoll)
{
	const TArray<TSharedPtr<FJsonValue>>* Players = GetArrayField(SummaryObject, TEXT("players"));
	Test.TestTrue(*FString::Printf(TEXT("%s players array exists"), *Label), Players != nullptr);
	if (Players == nullptr)
	{
		return;
	}

	const TSharedPtr<FJsonObject> PlayerObject = FindPlayerSummaryObject(*Players, PlayerId);
	Test.TestTrue(*FString::Printf(TEXT("%s player %d summary exists"), *Label, PlayerId), PlayerObject.IsValid());
	if (!PlayerObject.IsValid())
	{
		return;
	}

	int32 RemainingMP = -1;
	int32 LastMPRoll = -1;
	Test.TestTrue(*FString::Printf(TEXT("%s remaining MP reads"), *Label), TryReadIntegerField(PlayerObject, TEXT("remaining_mp"), RemainingMP));
	Test.TestTrue(*FString::Printf(TEXT("%s last MP roll reads"), *Label), TryReadIntegerField(PlayerObject, TEXT("last_mp_roll"), LastMPRoll));
	Test.TestEqual(*FString::Printf(TEXT("%s player %d remaining MP"), *Label, PlayerId), RemainingMP, ExpectedRemainingMP);
	Test.TestEqual(*FString::Printf(TEXT("%s player %d last MP roll"), *Label, PlayerId), LastMPRoll, ExpectedLastMPRoll);
}

bool ExpectFixtureTraceOrder(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& ExpectedJson,
	const TSharedPtr<FJsonObject>& SerializedRoot)
{
	const TArray<TSharedPtr<FJsonValue>>* ExpectedTraceValues = GetArrayField(ExpectedJson, TEXT("trace_order"));
	const TArray<TSharedPtr<FJsonValue>>* ActualTraceValues = GetArrayField(SerializedRoot, TEXT("traces"));
	Test.TestTrue(*FString::Printf(TEXT("%s expected trace order exists"), *FixtureName), ExpectedTraceValues != nullptr);
	Test.TestTrue(*FString::Printf(TEXT("%s serialized traces exist"), *FixtureName), ActualTraceValues != nullptr);
	if (ExpectedTraceValues == nullptr || ActualTraceValues == nullptr)
	{
		return false;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s trace count"), *FixtureName), ActualTraceValues->Num(), ExpectedTraceValues->Num());
	const int32 NumToCompare = FMath::Min(ActualTraceValues->Num(), ExpectedTraceValues->Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		const FString ExpectedLabel = (*ExpectedTraceValues)[Index].IsValid() ? (*ExpectedTraceValues)[Index]->AsString() : FString();
		const TSharedPtr<FJsonObject> TraceObject = (*ActualTraceValues)[Index].IsValid()
			? (*ActualTraceValues)[Index]->AsObject()
			: nullptr;
		Test.TestEqual(
			*FString::Printf(TEXT("%s trace label %d"), *FixtureName, Index),
			TraceLabelFromJsonObject(TraceObject),
			ExpectedLabel);
	}

	return ActualTraceValues->Num() == ExpectedTraceValues->Num();
}

bool ExpectFixturePublicSummary(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& ExpectedJson,
	const TSharedPtr<FJsonObject>& SerializedRoot)
{
	const TSharedPtr<FJsonObject> ExpectedSummary = GetObjectField(ExpectedJson, TEXT("final_public_turn_summary"));
	const TSharedPtr<FJsonObject> ActualSummary = GetObjectField(SerializedRoot, TEXT("final_public_turn_summary"));
	Test.TestTrue(*FString::Printf(TEXT("%s expected summary exists"), *FixtureName), ExpectedSummary.IsValid());
	Test.TestTrue(*FString::Printf(TEXT("%s serialized summary exists"), *FixtureName), ActualSummary.IsValid());
	if (!ExpectedSummary.IsValid() || !ActualSummary.IsValid())
	{
		return false;
	}

	int32 ExpectedInt = -1;
	int32 ActualInt = -1;
	TryReadIntegerField(ExpectedSummary, TEXT("current_player_id"), ExpectedInt);
	TryReadIntegerField(ActualSummary, TEXT("current_player_id"), ActualInt);
	Test.TestEqual(*FString::Printf(TEXT("%s current player"), *FixtureName), ActualInt, ExpectedInt);

	TryReadIntegerField(ExpectedSummary, TEXT("priority_player_id"), ExpectedInt);
	TryReadIntegerField(ActualSummary, TEXT("priority_player_id"), ActualInt);
	Test.TestEqual(*FString::Printf(TEXT("%s priority player"), *FixtureName), ActualInt, ExpectedInt);

	TryReadIntegerField(ExpectedSummary, TEXT("turn_number"), ExpectedInt);
	TryReadIntegerField(ActualSummary, TEXT("turn_number"), ActualInt);
	Test.TestEqual(*FString::Printf(TEXT("%s turn number"), *FixtureName), ActualInt, ExpectedInt);

	FString ExpectedPhase;
	FString ActualPhase;
	ExpectedSummary->TryGetStringField(TEXT("phase"), ExpectedPhase);
	ActualSummary->TryGetStringField(TEXT("phase"), ActualPhase);
	Test.TestEqual(*FString::Printf(TEXT("%s phase"), *FixtureName), ActualPhase, ExpectedPhase);

	bool bExpectedGameOver = false;
	bool bActualGameOver = true;
	ExpectedSummary->TryGetBoolField(TEXT("game_over"), bExpectedGameOver);
	ActualSummary->TryGetBoolField(TEXT("game_over"), bActualGameOver);
	Test.TestEqual(*FString::Printf(TEXT("%s game over"), *FixtureName), bActualGameOver, bExpectedGameOver);

	const TArray<TSharedPtr<FJsonValue>>* ExpectedPlayers = GetArrayField(ExpectedSummary, TEXT("players"));
	const TArray<TSharedPtr<FJsonValue>>* ActualPlayers = GetArrayField(ActualSummary, TEXT("players"));
	Test.TestTrue(*FString::Printf(TEXT("%s expected players exists"), *FixtureName), ExpectedPlayers != nullptr);
	Test.TestTrue(*FString::Printf(TEXT("%s actual players exists"), *FixtureName), ActualPlayers != nullptr);
	if (ExpectedPlayers == nullptr || ActualPlayers == nullptr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ExpectedPlayerValue : *ExpectedPlayers)
	{
		const TSharedPtr<FJsonObject> ExpectedPlayer = ExpectedPlayerValue.IsValid()
			? ExpectedPlayerValue->AsObject()
			: nullptr;
		int32 PlayerId = -1;
		if (!TryReadIntegerField(ExpectedPlayer, TEXT("player_id"), PlayerId))
		{
			Test.AddError(*FString::Printf(TEXT("%s malformed expected player"), *FixtureName));
			return false;
		}

		const TSharedPtr<FJsonObject> ActualPlayer = FindPlayerSummaryObject(*ActualPlayers, PlayerId);
		Test.TestTrue(*FString::Printf(TEXT("%s actual player %d exists"), *FixtureName, PlayerId), ActualPlayer.IsValid());
		if (!ActualPlayer.IsValid())
		{
			return false;
		}

		const TCHAR* Fields[] = {
			TEXT("remaining_mp"),
			TEXT("last_mp_roll"),
			TEXT("walls_left"),
			TEXT("wall_removals_left")
		};

		for (const TCHAR* FieldName : Fields)
		{
			int32 ExpectedValue = -1;
			int32 ActualValue = -1;
			TryReadIntegerField(ExpectedPlayer, FieldName, ExpectedValue);
			TryReadIntegerField(ActualPlayer, FieldName, ActualValue);
			Test.TestEqual(
				*FString::Printf(TEXT("%s player %d %s"), *FixtureName, PlayerId, FieldName),
				ActualValue,
				ExpectedValue);
		}
	}

	return true;
}

bool ExpectRuntimeResultSerializedFixture(
	FAutomationTestBase& Test,
	const FString& FixtureName,
	const TSharedPtr<FJsonObject>& Fixture,
	const FString& SerializedJson,
	const TSharedPtr<FJsonObject>& SerializedRoot)
{
	const TSharedPtr<FJsonObject> ExpectedJson = GetObjectField(Fixture, TEXT("expected_json"));
	Test.TestTrue(*FString::Printf(TEXT("%s expected_json exists"), *FixtureName), ExpectedJson.IsValid());
	if (!ExpectedJson.IsValid())
	{
		return false;
	}

	int32 SchemaVersion = 0;
	Test.TestTrue(*FString::Printf(TEXT("%s schema version reads"), *FixtureName), TryReadIntegerField(SerializedRoot, TEXT("schema_version"), SchemaVersion));
	Test.TestEqual(*FString::Printf(TEXT("%s schema version"), *FixtureName), SchemaVersion, WBRuntimeResultSerializer::RuntimeSelectedActionResultSchemaVersion);

	const TSharedPtr<FJsonObject> SelectedAction = GetObjectField(SerializedRoot, TEXT("selected_action"));
	const TSharedPtr<FJsonObject> Result = GetObjectField(SerializedRoot, TEXT("result"));
	const TSharedPtr<FJsonObject> MPRoll = GetObjectField(SerializedRoot, TEXT("mp_roll"));
	Test.TestTrue(*FString::Printf(TEXT("%s selected action exists"), *FixtureName), SelectedAction.IsValid());
	Test.TestTrue(*FString::Printf(TEXT("%s result exists"), *FixtureName), Result.IsValid());
	Test.TestTrue(*FString::Printf(TEXT("%s mp roll exists"), *FixtureName), MPRoll.IsValid());
	if (!SelectedAction.IsValid() || !Result.IsValid() || !MPRoll.IsValid())
	{
		return false;
	}

	FString ExpectedString;
	FString ActualString;
	ExpectedJson->TryGetStringField(TEXT("selected_action_type"), ExpectedString);
	SelectedAction->TryGetStringField(TEXT("type"), ActualString);
	Test.TestEqual(*FString::Printf(TEXT("%s selected action type"), *FixtureName), ActualString, ExpectedString);

	ExpectedJson->TryGetStringField(TEXT("selected_action_id"), ExpectedString);
	SelectedAction->TryGetStringField(TEXT("action_id"), ActualString);
	Test.TestEqual(*FString::Printf(TEXT("%s selected action id"), *FixtureName), ActualString, ExpectedString);

	bool bExpectedBool = false;
	bool bActualBool = true;
	ExpectedJson->TryGetBoolField(TEXT("ok"), bExpectedBool);
	Result->TryGetBoolField(TEXT("ok"), bActualBool);
	Test.TestEqual(*FString::Printf(TEXT("%s result ok"), *FixtureName), bActualBool, bExpectedBool);

	ExpectedJson->TryGetStringField(TEXT("reason"), ExpectedString);
	Result->TryGetStringField(TEXT("reason"), ActualString);
	Test.TestEqual(*FString::Printf(TEXT("%s result reason"), *FixtureName), ActualString, ExpectedString);

	ExpectedJson->TryGetBoolField(TEXT("consumed_mp_roll"), bExpectedBool);
	MPRoll->TryGetBoolField(TEXT("consumed"), bActualBool);
	Test.TestEqual(*FString::Printf(TEXT("%s consumed roll"), *FixtureName), bActualBool, bExpectedBool);

	int32 ExpectedInt = 0;
	int32 ActualInt = -1;
	TryReadIntegerField(ExpectedJson, TEXT("consumed_mp_roll_value"), ExpectedInt);
	TryReadIntegerField(MPRoll, TEXT("value"), ActualInt);
	Test.TestEqual(*FString::Printf(TEXT("%s consumed roll value"), *FixtureName), ActualInt, ExpectedInt);

	ExpectFixtureTraceOrder(Test, FixtureName, ExpectedJson, SerializedRoot);
	ExpectFixturePublicSummary(Test, FixtureName, ExpectedJson, SerializedRoot);

	const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = GetArrayField(ExpectedJson, TEXT("forbidden_substrings"));
	if (ForbiddenSubstrings != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& ForbiddenValue : *ForbiddenSubstrings)
		{
			const FString Forbidden = ForbiddenValue.IsValid() ? ForbiddenValue->AsString() : FString();
			Test.TestFalse(
				*FString::Printf(TEXT("%s excludes %s"), *FixtureName, *Forbidden),
				SerializedJson.Contains(Forbidden));
		}
	}

	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationFullEndTurnTest, "Wandbound.Core.RuntimeResultSerialization.FullEndTurnSerializesActionIdAndRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationFullEndTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBAction EndTurnAction = MakeSerializationAction(EWBActionType::EndTurn, 0);

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		EndTurnAction,
		Context);
	const TSharedPtr<FJsonObject> Root = SerializeEnvelopeToObject(Envelope);

	int32 SchemaVersion = 0;
	TestTrue(TEXT("Schema version exists"), TryReadIntegerField(Root, TEXT("schema_version"), SchemaVersion));
	TestEqual(TEXT("Schema version"), SchemaVersion, 1);

	const TSharedPtr<FJsonObject> SelectedAction = GetObjectField(Root, TEXT("selected_action"));
	TestTrue(TEXT("Selected action object exists"), SelectedAction.IsValid());
	if (SelectedAction.IsValid())
	{
		FString ActionType;
		FString ActionId;
		SelectedAction->TryGetStringField(TEXT("type"), ActionType);
		SelectedAction->TryGetStringField(TEXT("action_id"), ActionId);
		TestEqual(TEXT("Selected action type"), ActionType, FString(TEXT("end_turn")));
		TestEqual(TEXT("Selected action id"), ActionId, WBActionCodec::MakeActionId(EndTurnAction));
	}

	const TSharedPtr<FJsonObject> Result = GetObjectField(Root, TEXT("result"));
	TestTrue(TEXT("Result object exists"), Result.IsValid());
	if (Result.IsValid())
	{
		bool bOk = false;
		FString Reason;
		Result->TryGetBoolField(TEXT("ok"), bOk);
		Result->TryGetStringField(TEXT("reason"), Reason);
		TestTrue(TEXT("Result ok"), bOk);
		TestEqual(TEXT("Result reason"), Reason, FString());
	}

	const TSharedPtr<FJsonObject> MPRoll = GetObjectField(Root, TEXT("mp_roll"));
	TestTrue(TEXT("MP roll object exists"), MPRoll.IsValid());
	if (MPRoll.IsValid())
	{
		bool bConsumed = false;
		int32 RollValue = 0;
		MPRoll->TryGetBoolField(TEXT("consumed"), bConsumed);
		TryReadIntegerField(MPRoll, TEXT("value"), RollValue);
		TestTrue(TEXT("MP roll consumed"), bConsumed);
		TestEqual(TEXT("MP roll value"), RollValue, 4);
	}

	const TArray<TSharedPtr<FJsonValue>>* Traces = GetArrayField(Root, TEXT("traces"));
	TestTrue(TEXT("Traces array exists"), Traces != nullptr);
	if (Traces != nullptr)
	{
		TestTrue(TEXT("Full transition emits multiple traces"), Traces->Num() > 1);
	}
	TestTrue(TEXT("Transition trace serialized"), TracesContainKind(Root, TEXT("turn_transition")));

	const TSharedPtr<FJsonObject> Summary = GetObjectField(Root, TEXT("final_public_turn_summary"));
	TestTrue(TEXT("Summary object exists"), Summary.IsValid());
	if (Summary.IsValid())
	{
		int32 CurrentPlayer = -1;
		TryReadIntegerField(Summary, TEXT("current_player_id"), CurrentPlayer);
		TestEqual(TEXT("Summary current player"), CurrentPlayer, 1);
		ExpectSerializedPublicPlayer(*this, TEXT("full end turn"), Summary, 1, 4, 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationBasicEndTurnTest, "Wandbound.Core.RuntimeResultSerialization.BasicEndTurnSerializesNoRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationBasicEndTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationStatusState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeSerializationAction(EWBActionType::EndTurn, 0),
		Context);
	const TSharedPtr<FJsonObject> Root = SerializeEnvelopeToObject(Envelope);

	const TSharedPtr<FJsonObject> MPRoll = GetObjectField(Root, TEXT("mp_roll"));
	TestTrue(TEXT("MP roll object exists"), MPRoll.IsValid());
	if (MPRoll.IsValid())
	{
		bool bConsumed = true;
		int32 RollValue = -1;
		MPRoll->TryGetBoolField(TEXT("consumed"), bConsumed);
		TryReadIntegerField(MPRoll, TEXT("value"), RollValue);
		TestFalse(TEXT("MP roll not consumed"), bConsumed);
		TestEqual(TEXT("MP roll value zero"), RollValue, 0);
	}

	const TArray<TSharedPtr<FJsonValue>>* Traces = GetArrayField(Root, TEXT("traces"));
	TestTrue(TEXT("Traces array exists"), Traces != nullptr);
	if (Traces != nullptr)
	{
		TestEqual(TEXT("Basic EndTurn trace count"), Traces->Num(), 1);
		TestEqual(TEXT("Basic EndTurn trace kind"), TraceLabelFromJsonObject((*Traces)[0]->AsObject()), FString(TEXT("end_turn")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationMoveTest, "Wandbound.Core.RuntimeResultSerialization.MoveSerializesFinalMP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationState();
	TestTrue(TEXT("Move unit added"), State.AddUnitForTest(MakeSerializationUnit(1, 0, FWBTile(4, 4))));
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	const FWBAction MoveAction = MakeSerializationMoveAction();

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MoveAction,
		Context);
	const TSharedPtr<FJsonObject> Root = SerializeEnvelopeToObject(Envelope);

	const TSharedPtr<FJsonObject> SelectedAction = GetObjectField(Root, TEXT("selected_action"));
	TestTrue(TEXT("Selected action exists"), SelectedAction.IsValid());
	if (SelectedAction.IsValid())
	{
		FString Type;
		SelectedAction->TryGetStringField(TEXT("type"), Type);
		TestEqual(TEXT("Move action type"), Type, FString(TEXT("move")));
	}

	const TSharedPtr<FJsonObject> MPRoll = GetObjectField(Root, TEXT("mp_roll"));
	bool bConsumed = true;
	MPRoll->TryGetBoolField(TEXT("consumed"), bConsumed);
	TestFalse(TEXT("Move consumes no runtime roll"), bConsumed);

	const TSharedPtr<FJsonObject> Summary = GetObjectField(Root, TEXT("final_public_turn_summary"));
	ExpectSerializedPublicPlayer(*this, TEXT("move"), Summary, 0, 1, 2);
	TestTrue(TEXT("Move trace serialized"), TracesContainKind(Root, TEXT("move")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationPassResponseTest, "Wandbound.Core.RuntimeResultSerialization.PassResponseSerializes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationPassResponseTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationState();
	State.PriorityPlayer = 1;
	State.TurnNumber = 6;
	State.Phase = EWBGamePhase::Response;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeSerializationAction(EWBActionType::PassResponse, 1),
		Context);
	const TSharedPtr<FJsonObject> Root = SerializeEnvelopeToObject(Envelope);

	const TSharedPtr<FJsonObject> SelectedAction = GetObjectField(Root, TEXT("selected_action"));
	FString Type;
	SelectedAction->TryGetStringField(TEXT("type"), Type);
	TestEqual(TEXT("PassResponse action type"), Type, FString(TEXT("pass_response")));

	const TSharedPtr<FJsonObject> MPRoll = GetObjectField(Root, TEXT("mp_roll"));
	bool bConsumed = true;
	MPRoll->TryGetBoolField(TEXT("consumed"), bConsumed);
	TestFalse(TEXT("PassResponse consumes no runtime roll"), bConsumed);
	TestTrue(TEXT("PassResponse trace serialized"), TracesContainKind(Root, TEXT("pass_response")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationFailureTest, "Wandbound.Core.RuntimeResultSerialization.FailureSerializesReason", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationStatusState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeSerializationAction(EWBActionType::EndTurn, 0),
		Context);
	const TSharedPtr<FJsonObject> Root = SerializeEnvelopeToObject(Envelope);

	const TSharedPtr<FJsonObject> Result = GetObjectField(Root, TEXT("result"));
	bool bOk = true;
	FString Reason;
	Result->TryGetBoolField(TEXT("ok"), bOk);
	Result->TryGetStringField(TEXT("reason"), Reason);
	TestFalse(TEXT("Failure result is not ok"), bOk);
	TestFalse(TEXT("Failure reason is non-empty"), Reason.IsEmpty());

	const TSharedPtr<FJsonObject> Summary = GetObjectField(Root, TEXT("final_public_turn_summary"));
	int32 CurrentPlayer = -1;
	int32 TurnNumber = -1;
	TryReadIntegerField(Summary, TEXT("current_player_id"), CurrentPlayer);
	TryReadIntegerField(Summary, TEXT("turn_number"), TurnNumber);
	TestEqual(TEXT("Failure summary current player unchanged"), CurrentPlayer, 0);
	TestEqual(TEXT("Failure summary turn unchanged"), TurnNumber, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationHiddenDataTest, "Wandbound.Core.RuntimeResultSerialization.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationHiddenDataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationState();
	State.Players[0].Deck.Add(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	State.Players[0].Hand.Add(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK"));
	State.Players[0].Discard.Add(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK"));
	FWBUnitState SecretUnit = MakeSerializationUnit(1, 0, FWBTile(4, 4));
	SecretUnit.CardId = TEXT("SECRET_UNIT_CARD_DO_NOT_LEAK");
	TestTrue(TEXT("Secret unit added"), State.AddUnitForTest(SecretUnit));

	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;
	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeSerializationAction(EWBActionType::EndTurn, 0),
		Context);

	FString SerializedJson;
	TestTrue(
		TEXT("Result serializes to string"),
		WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	TestFalse(TEXT("Deck secret excluded"), SerializedJson.Contains(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hand secret excluded"), SerializedJson.Contains(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Discard secret excluded"), SerializedJson.Contains(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Unit card secret excluded"), SerializedJson.Contains(TEXT("SECRET_UNIT_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationActionIdStabilityTest, "Wandbound.Core.RuntimeResultSerialization.ActionIdStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationActionIdStabilityTest::RunTest(const FString& Parameters)
{
	const FWBAction EndTurnAction = MakeSerializationAction(EWBActionType::EndTurn, 0);

	FWBGameStateData FullState = MakeSerializationStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext FullContext;
	FullContext.bResolveEndTurnAsFullTransition = true;
	FullContext.MPRollSource = &RollSource;
	const FWBRuntimeSelectedActionResult FullEnvelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		FullState,
		EndTurnAction,
		FullContext);

	FWBGameStateData BasicState = MakeSerializationStatusState();
	FWBRuntimeTurnResolutionContext BasicContext;
	BasicContext.bResolveEndTurnAsFullTransition = false;
	const FWBRuntimeSelectedActionResult BasicEnvelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		BasicState,
		EndTurnAction,
		BasicContext);

	const TSharedPtr<FJsonObject> FullSelectedAction = GetObjectField(SerializeEnvelopeToObject(FullEnvelope), TEXT("selected_action"));
	const TSharedPtr<FJsonObject> BasicSelectedAction = GetObjectField(SerializeEnvelopeToObject(BasicEnvelope), TEXT("selected_action"));
	FString FullActionId;
	FString BasicActionId;
	FullSelectedAction->TryGetStringField(TEXT("action_id"), FullActionId);
	BasicSelectedAction->TryGetStringField(TEXT("action_id"), BasicActionId);

	TestEqual(TEXT("Full and basic action ids match"), FullActionId, BasicActionId);
	TestEqual(TEXT("Selected action id remains codec id"), FullActionId, WBActionCodec::MakeActionId(EndTurnAction));
	TestFalse(TEXT("Roll value not in action id"), FullActionId.Contains(TEXT("4")));
	TestFalse(TEXT("Full transition not in action id"), FullActionId.Contains(TEXT("full_transition")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationExistingBehaviorTest, "Wandbound.Core.RuntimeResultSerialization.ExistingBehaviorUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationExistingBehaviorTest::RunTest(const FString& Parameters)
{
	FWBGameStateData RuntimeState = MakeSerializationStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext RuntimeContext;
	RuntimeContext.bResolveEndTurnAsFullTransition = true;
	RuntimeContext.MPRollSource = &RollSource;
	const FWBAction EndTurnAction = MakeSerializationAction(EWBActionType::EndTurn, 0);

	const FWBApplyActionResult RuntimeResult = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
		RuntimeState,
		EndTurnAction,
		RuntimeContext);
	TestTrue(TEXT("Legacy runtime adapter still succeeds"), RuntimeResult.bOk);

	FWBGameStateData CodecState = MakeSerializationState();
	const FString SerializedAction = WBActionCodec::SerializeAction(EndTurnAction);
	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializedAction, CodecState);
	TestTrue(TEXT("WBActionCodec still decodes EndTurn"), DecodeResult.bOk);
	TestEqual(TEXT("WBActionCodec action id unchanged"), WBActionCodec::MakeActionId(EndTurnAction), FString(TEXT("end_turn:p0")));

	FWBGameStateData ReplayState = MakeSerializationState();
	const FWBApplyActionResult ReplayResult = WBEffectRunner::ApplyAction(ReplayState, EndTurnAction);
	TestTrue(TEXT("Replay apply_action path still applies basic EndTurn"), ReplayResult.bOk);
	TestEqual(TEXT("Replay apply_action basic trace count"), ReplayResult.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationRoundtripTest, "Wandbound.Core.RuntimeResultSerialization.JsonParseRoundtripSmoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationRoundtripTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeSerializationStatusState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeSerializationAction(EWBActionType::EndTurn, 0),
		Context);

	FString SerializedJson;
	TestTrue(
		TEXT("Runtime result serializes"),
		WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, SerializedJson));
	const TSharedPtr<FJsonObject> ParsedRoot = ParseJsonObject(SerializedJson);
	TestTrue(TEXT("Serialized result parses back"), ParsedRoot.IsValid());
	TestTrue(TEXT("schema_version exists"), ParsedRoot.IsValid() && ParsedRoot->HasField(TEXT("schema_version")));
	TestTrue(TEXT("selected_action exists"), GetObjectField(ParsedRoot, TEXT("selected_action")).IsValid());
	TestTrue(TEXT("result exists"), GetObjectField(ParsedRoot, TEXT("result")).IsValid());
	TestTrue(TEXT("mp_roll exists"), GetObjectField(ParsedRoot, TEXT("mp_roll")).IsValid());
	TestTrue(TEXT("traces exists"), GetArrayField(ParsedRoot, TEXT("traces")) != nullptr);
	TestTrue(TEXT("final_public_turn_summary exists"), GetObjectField(ParsedRoot, TEXT("final_public_turn_summary")).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultSerializationFixtureScenariosTest, "Wandbound.Core.RuntimeResultSerialization.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultSerializationFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("runtime_result_serialization_full_transition.json"),
		TEXT("runtime_result_serialization_basic_end_turn.json"),
		TEXT("runtime_result_serialization_hidden_data_exclusion.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadRuntimeResultSerializationFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FWBGameStateData State;
		FString StateReason;
		TestTrue(
			*FString::Printf(TEXT("%s builds initial state: %s"), *FixtureName, *StateReason),
			BuildGameStateFromFixture(Fixture, State, StateReason));
		if (!StateReason.IsEmpty())
		{
			continue;
		}

		FWBRuntimeSelectedActionResult Envelope;
		FString SerializedJson;
		int32 RollSourceRemainingCount = -1;
		FString ApplyReason;
		const bool bApplied = ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
			Fixture,
			State,
			Envelope,
			SerializedJson,
			RollSourceRemainingCount,
			ApplyReason);
		TestTrue(*FString::Printf(TEXT("%s applies and serializes: %s"), *FixtureName, *ApplyReason), bApplied);
		if (!bApplied)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> SerializedRoot = ParseJsonObject(SerializedJson);
		TestTrue(*FString::Printf(TEXT("%s serialized JSON parses"), *FixtureName), SerializedRoot.IsValid());
		if (!SerializedRoot.IsValid())
		{
			continue;
		}

		ExpectRuntimeResultSerializedFixture(*this, FixtureName, Fixture, SerializedJson, SerializedRoot);
	}

	return true;
}

#endif
