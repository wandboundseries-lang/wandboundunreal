#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakeBoardSummaryUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId = TEXT("char_visible_public"),
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 RLTotal = 3,
	const int32 RLUsed = 0,
	const int32 AttacksLeft = 1)
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
	return Unit;
}

FWBGameStateData MakeBoardSummaryState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	State.Players.Add(Player0);
	State.Players.Add(Player1);
	return State;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString BoardSummaryFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadBoardSummaryFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *BoardSummaryFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
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

bool ExpectStatusMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicUnitStatusSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	FString ExpectedStatusId;
	int32 ExpectedTurnsRemaining = -1;
	Test.TestTrue(*FString::Printf(TEXT("%s expected status id reads"), *Label), Expected.IsValid() && Expected->TryGetStringField(TEXT("status_id"), ExpectedStatusId));
	Test.TestTrue(*FString::Printf(TEXT("%s expected status turns reads"), *Label), TryReadIntegerField(Expected, TEXT("turns_remaining"), ExpectedTurnsRemaining));
	Test.TestEqual(*FString::Printf(TEXT("%s status id"), *Label), Actual.StatusId.GetPlainNameString(), ExpectedStatusId);
	Test.TestEqual(*FString::Printf(TEXT("%s turns remaining"), *Label), Actual.TurnsRemaining, ExpectedTurnsRemaining);
	return Actual.StatusId.GetPlainNameString() == ExpectedStatusId && Actual.TurnsRemaining == ExpectedTurnsRemaining;
}

bool ExpectUnitMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicUnitBoardSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	int32 ExpectedInt = -1;
	FString ExpectedCardId;
	TryReadIntegerField(Expected, TEXT("unit_id"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s unit id"), *Label), Actual.UnitId, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("owner_id"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s owner id"), *Label), Actual.OwnerId, ExpectedInt);
	Expected->TryGetStringField(TEXT("card_id"), ExpectedCardId);
	Test.TestEqual(*FString::Printf(TEXT("%s card id"), *Label), Actual.CardId, ExpectedCardId);

	const TCHAR* IntegerFields[] = {
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
	const int32 ActualValues[] = {
		Actual.X,
		Actual.Y,
		Actual.HP,
		Actual.MaxHP,
		Actual.ATK,
		Actual.AR,
		Actual.RLTotal,
		Actual.RLUsed,
		Actual.AttacksLeft
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(IntegerFields); ++Index)
	{
		TryReadIntegerField(Expected, IntegerFields[Index], ExpectedInt);
		Test.TestEqual(
			*FString::Printf(TEXT("%s %s"), *Label, IntegerFields[Index]),
			ActualValues[Index],
			ExpectedInt);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedStatuses = nullptr;
	Test.TestTrue(
		*FString::Printf(TEXT("%s statuses exist"), *Label),
		Expected->TryGetArrayField(TEXT("statuses"), ExpectedStatuses) && ExpectedStatuses != nullptr);
	if (ExpectedStatuses != nullptr)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s status count"), *Label), Actual.Statuses.Num(), ExpectedStatuses->Num());
		const int32 NumToCompare = FMath::Min(Actual.Statuses.Num(), ExpectedStatuses->Num());
		for (int32 StatusIndex = 0; StatusIndex < NumToCompare; ++StatusIndex)
		{
			ExpectStatusMatches(
				Test,
				FString::Printf(TEXT("%s status %d"), *Label, StatusIndex),
				Actual.Statuses[StatusIndex],
				(*ExpectedStatuses)[StatusIndex].IsValid() ? (*ExpectedStatuses)[StatusIndex]->AsObject() : nullptr);
		}
	}

	return true;
}

bool ExpectBoardSummaryMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicBoardSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	int32 ExpectedInt = -1;
	TryReadIntegerField(Expected, TEXT("board_width"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s board width"), *Label), Actual.BoardWidth, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("board_height"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s board height"), *Label), Actual.BoardHeight, ExpectedInt);

	const TArray<TSharedPtr<FJsonValue>>* ExpectedUnits = nullptr;
	Test.TestTrue(
		*FString::Printf(TEXT("%s units exist"), *Label),
		Expected->TryGetArrayField(TEXT("units"), ExpectedUnits) && ExpectedUnits != nullptr);
	if (ExpectedUnits == nullptr)
	{
		return false;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s unit count"), *Label), Actual.Units.Num(), ExpectedUnits->Num());
	const int32 NumToCompare = FMath::Min(Actual.Units.Num(), ExpectedUnits->Num());
	for (int32 UnitIndex = 0; UnitIndex < NumToCompare; ++UnitIndex)
	{
		ExpectUnitMatches(
			Test,
			FString::Printf(TEXT("%s unit %d"), *Label, UnitIndex),
			Actual.Units[UnitIndex],
			(*ExpectedUnits)[UnitIndex].IsValid() ? (*ExpectedUnits)[UnitIndex]->AsObject() : nullptr);
	}

	return true;
}

FString SerializeBoardSummaryThroughRuntimeResult(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = Summary;
	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, Json);
	return Json;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryPositionTest, "Wandbound.Core.PublicBoardSummary.IncludesVisibleUnitPosition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryPositionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4))));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Unit count"), Summary.Units.Num(), 1);
	TestEqual(TEXT("Unit id"), Summary.Units[0].UnitId, 1);
	TestEqual(TEXT("Owner id"), Summary.Units[0].OwnerId, 0);
	TestEqual(TEXT("X"), Summary.Units[0].X, 4);
	TestEqual(TEXT("Y"), Summary.Units[0].Y, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryCombatFieldsTest, "Wandbound.Core.PublicBoardSummary.IncludesCombatFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryCombatFieldsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public"), 3, 6, 4, 2, 5, 1, 0)));

	const FWBPublicUnitBoardSummary Unit = WBPublicBoardSummary::Build(State).Units[0];
	TestEqual(TEXT("HP"), Unit.HP, 3);
	TestEqual(TEXT("MaxHP"), Unit.MaxHP, 6);
	TestEqual(TEXT("ATK"), Unit.ATK, 4);
	TestEqual(TEXT("AR"), Unit.AR, 2);
	TestEqual(TEXT("RL total"), Unit.RLTotal, 5);
	TestEqual(TEXT("RL used"), Unit.RLUsed, 1);
	TestEqual(TEXT("Attacks left"), Unit.AttacksLeft, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryStatusesTest, "Wandbound.Core.PublicBoardSummary.IncludesStatuses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryStatusesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	FWBUnitState Unit = MakeBoardSummaryUnit(1, 0, FWBTile(4, 4));
	Unit.AddStatus(FName(TEXT("Burn")), 1);
	Unit.AddStatus(FName(TEXT("root")), 3);
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const TArray<FWBPublicUnitStatusSummary>& Statuses = WBPublicBoardSummary::Build(State).Units[0].Statuses;
	TestEqual(TEXT("Status count"), Statuses.Num(), 2);
	TestEqual(TEXT("First status"), Statuses[0].StatusId.GetPlainNameString(), FString(TEXT("Burn")));
	TestEqual(TEXT("First status turns"), Statuses[0].TurnsRemaining, 1);
	TestEqual(TEXT("Second status canonical"), Statuses[1].StatusId.GetPlainNameString(), FString(TEXT("Rooted")));
	TestEqual(TEXT("Second status turns"), Statuses[1].TurnsRemaining, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryUnitSortTest, "Wandbound.Core.PublicBoardSummary.UnitsSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryUnitSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.Units.Add(MakeBoardSummaryUnit(5, 0, FWBTile(1, 1)));
	State.Units.Add(MakeBoardSummaryUnit(3, 0, FWBTile(1, 0)));
	State.Units.Add(MakeBoardSummaryUnit(2, 0, FWBTile(2, 0)));
	State.Units.Add(MakeBoardSummaryUnit(1, 0, FWBTile(1, 0)));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("First unit sorted by y/x/id"), Summary.Units[0].UnitId, 1);
	TestEqual(TEXT("Second unit sorted by y/x/id"), Summary.Units[1].UnitId, 3);
	TestEqual(TEXT("Third unit sorted by y/x/id"), Summary.Units[2].UnitId, 2);
	TestEqual(TEXT("Fourth unit sorted by y/x/id"), Summary.Units[3].UnitId, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryStatusSortTest, "Wandbound.Core.PublicBoardSummary.StatusesSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryStatusSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	FWBUnitState Unit = MakeBoardSummaryUnit(1, 0, FWBTile(4, 4));
	Unit.AddStatus(FName(TEXT("stun")), 2);
	Unit.AddStatus(FName(TEXT("Burn")), 1);
	Unit.AddStatus(FName(TEXT("root")), 3);
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const TArray<FWBPublicUnitStatusSummary>& Statuses = WBPublicBoardSummary::Build(State).Units[0].Statuses;
	TestEqual(TEXT("First status"), Statuses[0].StatusId.GetPlainNameString(), FString(TEXT("Burn")));
	TestEqual(TEXT("Second status"), Statuses[1].StatusId.GetPlainNameString(), FString(TEXT("Rooted")));
	TestEqual(TEXT("Third status"), Statuses[2].StatusId.GetPlainNameString(), FString(TEXT("Stunned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryHiddenDataTest, "Wandbound.Core.PublicBoardSummary.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryHiddenDataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.Players[0].Deck.Add(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	State.Players[0].Hand.Add(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK"));
	State.Players[0].Discard.Add(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK"));
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public_identity"))));

	const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(WBPublicBoardSummary::Build(State));
	TestFalse(TEXT("Deck secret excluded"), Serialized.Contains(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hand secret excluded"), Serialized.Contains(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Discard secret excluded"), Serialized.Contains(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hidden marker placeholder secret absent"), Serialized.Contains(TEXT("SECRET_HIDDEN_MARKER_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Pending choice placeholder secret absent"), Serialized.Contains(TEXT("SECRET_PENDING_CHOICE_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryVisibleCardIdentityTest, "Wandbound.Core.PublicBoardSummary.VisibleUnitCardIdentityPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryVisibleCardIdentityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public_identity"))));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Visible unit card id included"), Summary.Units[0].CardId, FString(TEXT("char_visible_public_identity")));
	const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(Summary);
	TestTrue(TEXT("Visible card id serializes"), Serialized.Contains(TEXT("char_visible_public_identity")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryFixtureScenariosTest, "Wandbound.Core.PublicBoardSummary.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("public_board_summary_units_sorted.json"),
		TEXT("public_board_summary_statuses_sorted.json"),
		TEXT("public_board_summary_hidden_data_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadBoardSummaryFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FString Operation;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), Fixture->TryGetStringField(TEXT("operation"), Operation));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), Operation, FString(TEXT("build_public_board_summary")));

		FWBGameStateData State;
		FString Reason;
		TestTrue(*FString::Printf(TEXT("%s state builds: %s"), *FixtureName, *Reason), BuildGameStateFromFixture(Fixture, State, Reason));
		if (!Reason.IsEmpty())
		{
			continue;
		}

		const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
		const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
		TestTrue(*FString::Printf(TEXT("%s expected exists"), *FixtureName), Expected.IsValid());
		if (Expected.IsValid())
		{
			ExpectBoardSummaryMatches(*this, FixtureName, Summary, Expected);

			const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(Summary);
			const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = nullptr;
			if (Expected->TryGetArrayField(TEXT("forbidden_substrings"), ForbiddenSubstrings) && ForbiddenSubstrings != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& ForbiddenValue : *ForbiddenSubstrings)
				{
					const FString Forbidden = ForbiddenValue.IsValid() ? ForbiddenValue->AsString() : FString();
					TestFalse(*FString::Printf(TEXT("%s excludes %s"), *FixtureName, *Forbidden), Serialized.Contains(Forbidden));
				}
			}
		}
	}

	return true;
}

#endif
