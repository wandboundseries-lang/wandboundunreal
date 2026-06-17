#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBAction.h"
#include "WBMPRollSource.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBPlayerStateData MakeRuntimeBoardPlayer(const int32 PlayerId, const int32 RemainingMP = 0, const int32 LastMPRoll = 0)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = LastMPRoll;
	return Player;
}

FWBUnitState MakeRuntimeBoardUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 RLTotal = 3,
	const int32 RLUsed = 0,
	const int32 AttacksLeft = 1,
	const int32 MaxAttacksPerTurn = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = ATK;
	Unit.AR = AR;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = MaxAttacksPerTurn;
	return Unit;
}

FWBGameStateData MakeRuntimeBoardState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Players.Add(MakeRuntimeBoardPlayer(0, 2, 2));
	State.Players.Add(MakeRuntimeBoardPlayer(1, 0, 0));
	return State;
}

FWBAction MakeRuntimeBoardAction(const EWBActionType Type, const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	return Action;
}

FWBAction MakeRuntimeBoardMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
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

TSharedPtr<FJsonObject> FindUnitJson(const TSharedPtr<FJsonObject>& BoardSummary, const int32 UnitId)
{
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

TSharedPtr<FJsonObject> SerializeRuntimeBoardResultToObject(const FWBRuntimeSelectedActionResult& Envelope, FString& OutJson)
{
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, OutJson);
	return ParseJsonObject(OutJson);
}

FString RuntimeBoardFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadRuntimeBoardFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *RuntimeBoardFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

bool ExpectStatusJsonMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TSharedPtr<FJsonObject>& Actual)
{
	FString ExpectedStatusId;
	FString ActualStatusId;
	int32 ExpectedTurns = -1;
	int32 ActualTurns = -1;
	Expected->TryGetStringField(TEXT("status_id"), ExpectedStatusId);
	Actual->TryGetStringField(TEXT("status_id"), ActualStatusId);
	TryReadIntegerField(Expected, TEXT("turns_remaining"), ExpectedTurns);
	TryReadIntegerField(Actual, TEXT("turns_remaining"), ActualTurns);
	Test.TestEqual(*FString::Printf(TEXT("%s status id"), *Label), ActualStatusId, ExpectedStatusId);
	Test.TestEqual(*FString::Printf(TEXT("%s turns"), *Label), ActualTurns, ExpectedTurns);
	return ActualStatusId == ExpectedStatusId && ActualTurns == ExpectedTurns;
}

bool ExpectUnitJsonMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TSharedPtr<FJsonObject>& Actual)
{
	const TCHAR* IntegerFields[] = {
		TEXT("unit_id"),
		TEXT("owner_id"),
		TEXT("x"),
		TEXT("y"),
		TEXT("hp"),
		TEXT("max_hp"),
		TEXT("atk"),
		TEXT("ar"),
		TEXT("rl_total"),
		TEXT("rl_used"),
		TEXT("attacks_left")
	};

	for (const TCHAR* FieldName : IntegerFields)
	{
		int32 ExpectedValue = -1;
		int32 ActualValue = -1;
		TryReadIntegerField(Expected, FieldName, ExpectedValue);
		TryReadIntegerField(Actual, FieldName, ActualValue);
		Test.TestEqual(*FString::Printf(TEXT("%s %s"), *Label, FieldName), ActualValue, ExpectedValue);
	}

	FString ExpectedCardId;
	FString ActualCardId;
	Expected->TryGetStringField(TEXT("card_id"), ExpectedCardId);
	Actual->TryGetStringField(TEXT("card_id"), ActualCardId);
	Test.TestEqual(*FString::Printf(TEXT("%s card id"), *Label), ActualCardId, ExpectedCardId);

	const TArray<TSharedPtr<FJsonValue>>* ExpectedStatuses = GetArrayField(Expected, TEXT("statuses"));
	const TArray<TSharedPtr<FJsonValue>>* ActualStatuses = GetArrayField(Actual, TEXT("statuses"));
	Test.TestTrue(*FString::Printf(TEXT("%s expected statuses"), *Label), ExpectedStatuses != nullptr);
	Test.TestTrue(*FString::Printf(TEXT("%s actual statuses"), *Label), ActualStatuses != nullptr);
	if (ExpectedStatuses != nullptr && ActualStatuses != nullptr)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s status count"), *Label), ActualStatuses->Num(), ExpectedStatuses->Num());
		const int32 NumToCompare = FMath::Min(ActualStatuses->Num(), ExpectedStatuses->Num());
		for (int32 Index = 0; Index < NumToCompare; ++Index)
		{
			ExpectStatusJsonMatches(
				Test,
				FString::Printf(TEXT("%s status %d"), *Label, Index),
				(*ExpectedStatuses)[Index].IsValid() ? (*ExpectedStatuses)[Index]->AsObject() : nullptr,
				(*ActualStatuses)[Index].IsValid() ? (*ActualStatuses)[Index]->AsObject() : nullptr);
		}
	}

	return true;
}

bool ExpectBoardSummaryJsonMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const TSharedPtr<FJsonObject>& Expected,
	const TSharedPtr<FJsonObject>& Actual)
{
	int32 ExpectedInt = -1;
	int32 ActualInt = -1;
	TryReadIntegerField(Expected, TEXT("board_width"), ExpectedInt);
	TryReadIntegerField(Actual, TEXT("board_width"), ActualInt);
	Test.TestEqual(*FString::Printf(TEXT("%s board width"), *Label), ActualInt, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("board_height"), ExpectedInt);
	TryReadIntegerField(Actual, TEXT("board_height"), ActualInt);
	Test.TestEqual(*FString::Printf(TEXT("%s board height"), *Label), ActualInt, ExpectedInt);

	const TArray<TSharedPtr<FJsonValue>>* ExpectedUnits = GetArrayField(Expected, TEXT("units"));
	const TArray<TSharedPtr<FJsonValue>>* ActualUnits = GetArrayField(Actual, TEXT("units"));
	Test.TestTrue(*FString::Printf(TEXT("%s expected units"), *Label), ExpectedUnits != nullptr);
	Test.TestTrue(*FString::Printf(TEXT("%s actual units"), *Label), ActualUnits != nullptr);
	if (ExpectedUnits == nullptr || ActualUnits == nullptr)
	{
		return false;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s unit count"), *Label), ActualUnits->Num(), ExpectedUnits->Num());
	const int32 NumToCompare = FMath::Min(ActualUnits->Num(), ExpectedUnits->Num());
	for (int32 Index = 0; Index < NumToCompare; ++Index)
	{
		ExpectUnitJsonMatches(
			Test,
			FString::Printf(TEXT("%s unit %d"), *Label, Index),
			(*ExpectedUnits)[Index].IsValid() ? (*ExpectedUnits)[Index]->AsObject() : nullptr,
			(*ActualUnits)[Index].IsValid() ? (*ActualUnits)[Index]->AsObject() : nullptr);
	}

	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultBoardSummaryAfterMoveTest, "Wandbound.Core.RuntimeResultBoardSummary.AfterMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultBoardSummaryAfterMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeBoardState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeRuntimeBoardUnit(1, 0, FWBTile(4, 4), TEXT("char_mover_public"))));
	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeRuntimeBoardMoveAction(),
		Context);

	FString Json;
	const TSharedPtr<FJsonObject> Root = SerializeRuntimeBoardResultToObject(Envelope, Json);
	const TSharedPtr<FJsonObject> BoardSummary = GetObjectField(Root, TEXT("final_public_board_summary"));
	TestTrue(TEXT("Board summary exists"), BoardSummary.IsValid());
	const TSharedPtr<FJsonObject> Unit = FindUnitJson(BoardSummary, 1);
	TestTrue(TEXT("Moved unit exists"), Unit.IsValid());
	if (Unit.IsValid())
	{
		int32 X = -1;
		int32 Y = -1;
		TryReadIntegerField(Unit, TEXT("x"), X);
		TryReadIntegerField(Unit, TEXT("y"), Y);
		TestEqual(TEXT("Moved unit x"), X, 5);
		TestEqual(TEXT("Moved unit y"), Y, 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultBoardSummaryAfterFullTurnTest, "Wandbound.Core.RuntimeResultBoardSummary.AfterFullEndTurn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultBoardSummaryAfterFullTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeBoardState();
	FWBUnitState BurnUnit = MakeRuntimeBoardUnit(1, 0, FWBTile(4, 4), TEXT("char_burn_public"), 5, 5, 2, 1, 3, 0, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeRuntimeBoardUnit(2, 1, FWBTile(6, 4), TEXT("char_poison_public"), 5, 5, 3, 2, 4, 1, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeRuntimeBoardAction(EWBActionType::EndTurn, 0),
		Context);

	FString Json;
	const TSharedPtr<FJsonObject> Root = SerializeRuntimeBoardResultToObject(Envelope, Json);
	const TSharedPtr<FJsonObject> BoardSummary = GetObjectField(Root, TEXT("final_public_board_summary"));
	const TSharedPtr<FJsonObject> BurnJson = FindUnitJson(BoardSummary, 1);
	const TSharedPtr<FJsonObject> PoisonJson = FindUnitJson(BoardSummary, 2);
	TestTrue(TEXT("Burn unit summary exists"), BurnJson.IsValid());
	TestTrue(TEXT("Poison unit summary exists"), PoisonJson.IsValid());
	if (BurnJson.IsValid())
	{
		int32 HP = -1;
		TryReadIntegerField(BurnJson, TEXT("hp"), HP);
		TestEqual(TEXT("Burn HP after full turn"), HP, 4);
	}
	if (PoisonJson.IsValid())
	{
		int32 HP = -1;
		int32 MaxHP = -1;
		int32 AttacksLeft = -1;
		TryReadIntegerField(PoisonJson, TEXT("hp"), HP);
		TryReadIntegerField(PoisonJson, TEXT("max_hp"), MaxHP);
		TryReadIntegerField(PoisonJson, TEXT("attacks_left"), AttacksLeft);
		TestEqual(TEXT("Poison HP clamped"), HP, 4);
		TestEqual(TEXT("Poison max HP reduced"), MaxHP, 4);
		TestEqual(TEXT("Next player attacks reset"), AttacksLeft, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultBoardSummaryFailedRollTest, "Wandbound.Core.RuntimeResultBoardSummary.FailedInvalidRollSummarizesUnchangedBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultBoardSummaryFailedRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeBoardState();
	FWBUnitState BurnUnit = MakeRuntimeBoardUnit(1, 0, FWBTile(4, 4), TEXT("char_burn_public"));
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(BurnUnit));

	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(7);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeRuntimeBoardAction(EWBActionType::EndTurn, 0),
		Context);

	TestFalse(TEXT("Invalid roll fails"), Envelope.ApplyResult.bOk);
	TestEqual(TEXT("Board summary unit count"), Envelope.FinalPublicBoardSummary.Units.Num(), 1);
	TestEqual(TEXT("Unit HP unchanged"), Envelope.FinalPublicBoardSummary.Units[0].HP, 5);
	TestEqual(TEXT("Unit X unchanged"), Envelope.FinalPublicBoardSummary.Units[0].X, 4);
	TestEqual(TEXT("Unit Y unchanged"), Envelope.FinalPublicBoardSummary.Units[0].Y, 4);
	TestEqual(TEXT("Burn duration unchanged"), Envelope.FinalPublicBoardSummary.Units[0].Statuses[0].TurnsRemaining, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultBoardSummaryJsonFieldTest, "Wandbound.Core.RuntimeResultBoardSummary.SerializedJsonHasField", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultBoardSummaryJsonFieldTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeBoardState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeRuntimeBoardUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public"))));
	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeRuntimeBoardMoveAction(),
		Context);

	FString Json;
	const TSharedPtr<FJsonObject> Root = SerializeRuntimeBoardResultToObject(Envelope, Json);
	TestTrue(TEXT("final_public_board_summary exists"), GetObjectField(Root, TEXT("final_public_board_summary")).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultBoardSummaryHiddenDataTest, "Wandbound.Core.RuntimeResultBoardSummary.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultBoardSummaryHiddenDataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRuntimeBoardState();
	State.Players[0].Deck.Add(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	State.Players[0].Hand.Add(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK"));
	State.Players[0].Discard.Add(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK"));
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeRuntimeBoardUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public_identity"))));

	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;
	const FWBRuntimeSelectedActionResult Envelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		MakeRuntimeBoardAction(EWBActionType::EndTurn, 0),
		Context);

	FString Json;
	SerializeRuntimeBoardResultToObject(Envelope, Json);
	TestFalse(TEXT("Deck secret excluded"), Json.Contains(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hand secret excluded"), Json.Contains(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Discard secret excluded"), Json.Contains(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hidden marker placeholder excluded"), Json.Contains(TEXT("SECRET_HIDDEN_MARKER_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Pending choice placeholder excluded"), Json.Contains(TEXT("SECRET_PENDING_CHOICE_DO_NOT_LEAK")));
	TestTrue(TEXT("Visible unit card id included"), Json.Contains(TEXT("char_visible_public_identity")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeResultBoardSummaryFixtureScenariosTest, "Wandbound.Core.RuntimeResultBoardSummary.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeResultBoardSummaryFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("runtime_result_serialization_public_board_summary_after_move.json"),
		TEXT("runtime_result_serialization_public_board_summary_after_full_turn.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadRuntimeBoardFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FWBGameStateData State;
		FString Reason;
		TestTrue(*FString::Printf(TEXT("%s state builds: %s"), *FixtureName, *Reason), BuildGameStateFromFixture(Fixture, State, Reason));
		if (!Reason.IsEmpty())
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

		const TSharedPtr<FJsonObject> Root = ParseJsonObject(SerializedJson);
		const TSharedPtr<FJsonObject> ExpectedJson = GetObjectField(Fixture, TEXT("expected_json"));
		const TSharedPtr<FJsonObject> ExpectedBoard = GetObjectField(ExpectedJson, TEXT("final_public_board_summary"));
		const TSharedPtr<FJsonObject> ActualBoard = GetObjectField(Root, TEXT("final_public_board_summary"));
		TestTrue(*FString::Printf(TEXT("%s expected board exists"), *FixtureName), ExpectedBoard.IsValid());
		TestTrue(*FString::Printf(TEXT("%s actual board exists"), *FixtureName), ActualBoard.IsValid());
		if (ExpectedBoard.IsValid() && ActualBoard.IsValid())
		{
			ExpectBoardSummaryJsonMatches(*this, FixtureName, ExpectedBoard, ActualBoard);
		}
	}

	return true;
}

#endif
