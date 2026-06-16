#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBAction.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeResourcePlayer(
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

FWBUnitState MakeResourceUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 AttacksLeft = 0,
	const int32 MaxAttacksPerTurn = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = MaxAttacksPerTurn;
	return Unit;
}

FWBAction MakeResourceMove(const int32 PlayerId, const int32 UnitId, const FWBTile& FromTile, const FWBTile& ToTile)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = UnitId;
	Action.FromTile = FromTile;
	Action.ToTile = ToTile;
	return Action;
}

FWBGameStateData MakeTurnStartState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 1;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeResourcePlayer(0));
	State.Players.Add(MakeResourcePlayer(1));
	return State;
}

bool ContainsMoveAction(const TArray<FWBAction>& Actions)
{
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == EWBActionType::Move)
		{
			return true;
		}
	}

	return false;
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

int32 ReadIntegerFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const int32 DefaultValue)
{
	int32 Value = DefaultValue;
	TryReadIntegerField(Object, FieldName, Value);
	return Value;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString TurnStartFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadTurnStartFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *TurnStartFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

EWBGamePhase ReadPhaseOrDefault(const TSharedPtr<FJsonObject>& Object, const EWBGamePhase DefaultPhase)
{
	FString PhaseString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("phase"), PhaseString))
	{
		return DefaultPhase;
	}

	return PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
}

bool ApplyFixturePlayers(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState)
{
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("players"), Players) || Players == nullptr)
	{
		OutState.Players.Add(MakeResourcePlayer(0));
		OutState.Players.Add(MakeResourcePlayer(1));
		return true;
	}

	for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
	{
		const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
		if (!PlayerObject.IsValid())
		{
			return false;
		}

		FWBPlayerStateData Player;
		if (!TryReadIntegerField(PlayerObject, TEXT("player_id"), Player.PlayerId))
		{
			return false;
		}
		Player.RemainingMP = ReadIntegerFieldOrDefault(PlayerObject, TEXT("remaining_mp"), 0);
		Player.LastMPRoll = ReadIntegerFieldOrDefault(PlayerObject, TEXT("last_mp_roll"), 0);
		Player.WallsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("walls_left"), 0);
		Player.WallRemovalsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("wall_removals_left"), 0);
		OutState.Players.Add(Player);
	}

	return true;
}

bool ApplyFixtureUnits(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState)
{
	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("units"), Units) || Units == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
	{
		const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
		if (!UnitObject.IsValid())
		{
			return false;
		}

		FWBUnitState Unit;
		if (!TryReadIntegerField(UnitObject, TEXT("id"), Unit.UnitId)
			|| !TryReadIntegerField(UnitObject, TEXT("owner_id"), Unit.OwnerId)
			|| !TryReadIntegerField(UnitObject, TEXT("x"), Unit.X)
			|| !TryReadIntegerField(UnitObject, TEXT("y"), Unit.Y))
		{
			return false;
		}

		Unit.AttacksLeft = ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), 0);
		Unit.MaxAttacksPerTurn = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_attacks_per_turn"), 1);
		Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 0);
		if (!OutState.AddUnitForTest(Unit))
		{
			return false;
		}
	}

	return true;
}

bool BuildTurnStartStateFromFixture(const TSharedPtr<FJsonObject>& Fixture, FWBGameStateData& OutState)
{
	const TSharedPtr<FJsonObject>* StateObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("initial_state"), StateObject)
		|| StateObject == nullptr
		|| !StateObject->IsValid())
	{
		return false;
	}

	OutState = FWBGameStateData();
	OutState.CurrentPlayer = ReadIntegerFieldOrDefault(*StateObject, TEXT("current_player"), 0);
	OutState.PriorityPlayer = ReadIntegerFieldOrDefault(*StateObject, TEXT("priority_player"), OutState.CurrentPlayer);
	OutState.TurnNumber = ReadIntegerFieldOrDefault(*StateObject, TEXT("turn_number"), 1);
	OutState.Phase = ReadPhaseOrDefault(*StateObject, EWBGamePhase::NormalTurn);

	return ApplyFixturePlayers(*StateObject, OutState) && ApplyFixtureUnits(*StateObject, OutState);
}

bool ReadFixtureOperation(
	const TSharedPtr<FJsonObject>& Fixture,
	int32& OutPlayerId,
	int32& OutExplicitMPRoll)
{
	const TSharedPtr<FJsonObject>* OperationObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("operation"), OperationObject)
		|| OperationObject == nullptr
		|| !OperationObject->IsValid())
	{
		return false;
	}

	FString Kind;
	if (!(*OperationObject)->TryGetStringField(TEXT("kind"), Kind)
		|| Kind != TEXT("turn_start_resource_setup"))
	{
		return false;
	}

	return TryReadIntegerField(*OperationObject, TEXT("player_id"), OutPlayerId)
		&& TryReadIntegerField(*OperationObject, TEXT("explicit_mp_roll"), OutExplicitMPRoll);
}

bool ExpectedStateMatches(const TSharedPtr<FJsonObject>& Fixture, const FWBGameStateData& State, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		|| ExpectObject == nullptr
		|| !ExpectObject->IsValid()
		|| !(*ExpectObject)->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		|| FinalStateObject == nullptr
		|| !FinalStateObject->IsValid())
	{
		OutReason = TEXT("missing_expect_final_state");
		return false;
	}

	if (State.CurrentPlayer != ReadIntegerFieldOrDefault(*FinalStateObject, TEXT("current_player"), State.CurrentPlayer))
	{
		OutReason = TEXT("current_player_mismatch");
		return false;
	}

	if (State.PriorityPlayer != ReadIntegerFieldOrDefault(*FinalStateObject, TEXT("priority_player"), State.PriorityPlayer))
	{
		OutReason = TEXT("priority_player_mismatch");
		return false;
	}

	if (State.TurnNumber != ReadIntegerFieldOrDefault(*FinalStateObject, TEXT("turn_number"), State.TurnNumber))
	{
		OutReason = TEXT("turn_number_mismatch");
		return false;
	}

	if (State.Phase != ReadPhaseOrDefault(*FinalStateObject, State.Phase))
	{
		OutReason = TEXT("phase_mismatch");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if ((*FinalStateObject)->TryGetArrayField(TEXT("players"), Players) && Players != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
		{
			const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
			int32 PlayerId = -1;
			if (!PlayerObject.IsValid() || !TryReadIntegerField(PlayerObject, TEXT("player_id"), PlayerId))
			{
				OutReason = TEXT("malformed_expected_player");
				return false;
			}

			const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
			if (Player == nullptr)
			{
				OutReason = TEXT("missing_expected_player");
				return false;
			}

			if (Player->RemainingMP != ReadIntegerFieldOrDefault(PlayerObject, TEXT("remaining_mp"), Player->RemainingMP)
				|| Player->LastMPRoll != ReadIntegerFieldOrDefault(PlayerObject, TEXT("last_mp_roll"), Player->LastMPRoll)
				|| Player->WallsLeft != ReadIntegerFieldOrDefault(PlayerObject, TEXT("walls_left"), Player->WallsLeft)
				|| Player->WallRemovalsLeft != ReadIntegerFieldOrDefault(PlayerObject, TEXT("wall_removals_left"), Player->WallRemovalsLeft))
			{
				OutReason = FString::Printf(TEXT("player_%d_resource_mismatch"), PlayerId);
				return false;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if ((*FinalStateObject)->TryGetArrayField(TEXT("units"), Units) && Units != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& UnitValue : *Units)
		{
			const TSharedPtr<FJsonObject> UnitObject = UnitValue.IsValid() ? UnitValue->AsObject() : nullptr;
			int32 UnitId = -1;
			if (!UnitObject.IsValid() || !TryReadIntegerField(UnitObject, TEXT("id"), UnitId))
			{
				OutReason = TEXT("malformed_expected_unit");
				return false;
			}

			const FWBUnitState* Unit = State.GetUnitById(UnitId);
			if (Unit == nullptr)
			{
				OutReason = TEXT("missing_expected_unit");
				return false;
			}

			if (Unit->AttacksLeft != ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), Unit->AttacksLeft))
			{
				OutReason = FString::Printf(TEXT("unit_%d_attacks_left_mismatch"), UnitId);
				return false;
			}
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectedTraceMatches(const TSharedPtr<FJsonObject>& Fixture, const TArray<FWBTraceEvent>& TraceEvents, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		|| ExpectObject == nullptr
		|| !ExpectObject->IsValid())
	{
		OutReason = TEXT("missing_expect");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedEvents = nullptr;
	if (!(*ExpectObject)->TryGetArrayField(TEXT("trace_events"), ExpectedEvents) || ExpectedEvents == nullptr)
	{
		OutReason = TEXT("missing_expect_trace_events");
		return false;
	}

	if (TraceEvents.Num() != ExpectedEvents->Num())
	{
		OutReason = FString::Printf(TEXT("trace_count_expected_%d_got_%d"), ExpectedEvents->Num(), TraceEvents.Num());
		return false;
	}

	for (int32 Index = 0; Index < ExpectedEvents->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> ExpectedEvent = (*ExpectedEvents)[Index].IsValid() ? (*ExpectedEvents)[Index]->AsObject() : nullptr;
		if (!ExpectedEvent.IsValid())
		{
			OutReason = TEXT("malformed_expected_trace_event");
			return false;
		}

		const FWBTraceEvent& Event = TraceEvents[Index];
		FString Kind;
		if (!ExpectedEvent->TryGetStringField(TEXT("kind"), Kind) || Event.Kind.ToString() != Kind)
		{
			OutReason = TEXT("trace_kind_mismatch");
			return false;
		}

		if (Event.PlayerId != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("player_id"), Event.PlayerId)
			|| Event.TurnNumber != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("turn_number"), Event.TurnNumber)
			|| Event.MPRoll != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("mp_roll"), Event.MPRoll)
			|| Event.RemainingMP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("remaining_mp"), Event.RemainingMP)
			|| Event.WallsLeft != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("walls_left"), Event.WallsLeft)
			|| Event.WallRemovalsLeft != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("wall_removals_left"), Event.WallRemovalsLeft))
		{
			OutReason = TEXT("trace_resource_mismatch");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartValidRollAppliesMPTest, "Wandbound.Core.TurnStart.ValidRollAppliesMP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartValidRollAppliesMPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	State.CurrentPlayer = 1;
	State.PriorityPlayer = 1;
	State.Phase = EWBGamePhase::Response;
	TestTrue(TEXT("Owned unit added"), State.AddUnitForTest(MakeResourceUnit(1, 0, FWBTile(4, 4), 0, 1)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 4);

	TestTrue(TEXT("Turn-start setup succeeds"), Result.bOk);
	TestEqual(TEXT("Current player normalized"), State.CurrentPlayer, 0);
	TestEqual(TEXT("Priority player normalized"), State.PriorityPlayer, 0);
	TestTrue(TEXT("Phase normalized"), State.IsNormalTurnPhase());
	const FWBPlayerStateData* Player = State.GetPlayerById(0);
	TestTrue(TEXT("Player exists"), Player != nullptr);
	if (Player != nullptr)
	{
		TestEqual(TEXT("Last MP roll recorded"), Player->LastMPRoll, 4);
		TestEqual(TEXT("Remaining MP applied"), Player->RemainingMP, 4);
	}
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("turn_start_resource_setup")));
		TestEqual(TEXT("Trace roll"), Result.TraceEvents[0].MPRoll, 4);
		TestEqual(TEXT("Trace remaining MP"), Result.TraceEvents[0].RemainingMP, 4);
		TestTrue(TEXT("Trace ok"), Result.TraceEvents[0].bOk);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartInvalidLowRollFailsTest, "Wandbound.Core.TurnStart.InvalidLowRollFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartInvalidLowRollFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	State.GetMutablePlayerById(0)->RemainingMP = 2;
	const FWBGameStateData Before = State;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 0);

	TestFalse(TEXT("Low roll fails"), Result.bOk);
	TestEqual(TEXT("Low roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Priority unchanged"), State.PriorityPlayer, Before.PriorityPlayer);
	TestEqual(TEXT("MP unchanged"), State.GetPlayerById(0)->RemainingMP, Before.GetPlayerById(0)->RemainingMP);
	TestEqual(TEXT("No trace"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartInvalidHighRollFailsTest, "Wandbound.Core.TurnStart.InvalidHighRollFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartInvalidHighRollFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	const FWBGameStateData Before = State;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 7);

	TestFalse(TEXT("High roll fails"), Result.bOk);
	TestEqual(TEXT("High roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Priority unchanged"), State.PriorityPlayer, Before.PriorityPlayer);
	TestEqual(TEXT("No trace"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartWallsResetTest, "Wandbound.Core.TurnStart.WallsReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartWallsResetTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	State.GetMutablePlayerById(0)->WallsLeft = 0;

	const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 3);

	TestTrue(TEXT("Turn-start setup succeeds"), Result.bOk);
	TestEqual(TEXT("Walls reset to one"), State.GetPlayerById(0)->WallsLeft, 1);
	TestEqual(TEXT("Wall removals remain locked before turn 30"), State.GetPlayerById(0)->WallRemovalsLeft, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartAttacksResetTest, "Wandbound.Core.TurnStart.AttacksReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartAttacksResetTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	TestTrue(TEXT("Owned unit added"), State.AddUnitForTest(MakeResourceUnit(1, 0, FWBTile(4, 4), 0, 2)));
	TestTrue(TEXT("Opponent unit added"), State.AddUnitForTest(MakeResourceUnit(2, 1, FWBTile(5, 4), 0, 3)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 4);

	TestTrue(TEXT("Turn-start setup succeeds"), Result.bOk);
	TestEqual(TEXT("Owned unit attacks reset to max"), State.GetUnitById(1)->AttacksLeft, 2);
	TestEqual(TEXT("Opponent unit attacks unchanged"), State.GetUnitById(2)->AttacksLeft, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartMovementUsesResetMPTest, "Wandbound.Core.TurnStart.MovementUsesResetMP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartMovementUsesResetMPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	TestTrue(TEXT("Owned unit added"), State.AddUnitForTest(MakeResourceUnit(1, 0, FWBTile(4, 4), 0, 1)));

	const FWBApplyActionResult SetupResult = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 2);
	TestTrue(TEXT("Turn-start setup succeeds"), SetupResult.bOk);
	TestEqual(TEXT("Player MP reset to roll"), State.GetPlayerById(0)->RemainingMP, 2);

	const TArray<FWBAction> LegalActions = WBRules::GenerateLegalActions(State);
	TestTrue(TEXT("Movement generated after MP reset"), ContainsMoveAction(LegalActions));

	const FWBApplyActionResult MoveResult = WBEffectRunner::ApplyMove(State, MakeResourceMove(0, 1, FWBTile(4, 4), FWBTile(5, 4)));
	TestTrue(TEXT("Move applies after setup"), MoveResult.bOk);
	TestEqual(TEXT("Player MP spends one"), State.GetPlayerById(0)->RemainingMP, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartFailedSetupDoesNotMutateTest, "Wandbound.Core.TurnStart.FailedSetupDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartFailedSetupDoesNotMutateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	State.CurrentPlayer = 1;
	State.PriorityPlayer = 1;
	State.Phase = EWBGamePhase::Response;
	State.GetMutablePlayerById(0)->RemainingMP = 5;
	State.GetMutablePlayerById(0)->LastMPRoll = 5;
	State.GetMutablePlayerById(0)->WallsLeft = 0;
	TestTrue(TEXT("Owned unit added"), State.AddUnitForTest(MakeResourceUnit(1, 0, FWBTile(4, 4), 0, 1)));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, 0, 9);

	TestFalse(TEXT("Invalid setup fails"), Result.bOk);
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Priority unchanged"), State.PriorityPlayer, 1);
	TestTrue(TEXT("Phase unchanged"), State.IsResponsePhase());
	TestEqual(TEXT("MP unchanged"), State.GetPlayerById(0)->RemainingMP, 5);
	TestEqual(TEXT("Last roll unchanged"), State.GetPlayerById(0)->LastMPRoll, 5);
	TestEqual(TEXT("Walls unchanged"), State.GetPlayerById(0)->WallsLeft, 0);
	TestEqual(TEXT("Attacks unchanged"), State.GetUnitById(1)->AttacksLeft, 0);
	TestEqual(TEXT("No trace"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartNoMovesWhenNoMPTest, "Wandbound.Core.TurnStart.NoMovesWhenNoMP", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartNoMovesWhenNoMPTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTurnStartState();
	State.GetMutablePlayerById(0)->RemainingMP = 0;
	TestTrue(TEXT("Owned unit added"), State.AddUnitForTest(MakeResourceUnit(1, 0, FWBTile(4, 4), 0, 1)));

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActions(State);

	TestFalse(TEXT("No movement when player MP is zero"), ContainsMoveAction(Actions));
	TestEqual(TEXT("Only end turn remains"), Actions.Num(), 1);
	if (Actions.Num() == 1)
	{
		TestTrue(TEXT("Remaining action is end turn"), Actions[0].Type == EWBActionType::EndTurn);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnStartFixtureScenariosTest, "Wandbound.Core.TurnStart.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnStartFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("turn_start_resource_setup_roll_4.json"),
		TEXT("turn_start_resource_setup_invalid_roll_fails.json"),
		TEXT("turn_start_resource_setup_resets_attacks_and_walls.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadTurnStartFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FWBGameStateData State;
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), BuildTurnStartStateFromFixture(Fixture, State));

		int32 PlayerId = -1;
		int32 ExplicitMPRoll = 0;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), ReadFixtureOperation(Fixture, PlayerId, ExplicitMPRoll));

		const FWBApplyActionResult Result = WBEffectRunner::ApplyTurnStartResourceSetup(State, PlayerId, ExplicitMPRoll);

		const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
		bool bExpectedOk = false;
		FString ExpectedReason;
		TestTrue(*FString::Printf(TEXT("%s expect object exists"), *FixtureName), Fixture->TryGetObjectField(TEXT("expect"), ExpectObject));
		if (ExpectObject != nullptr && ExpectObject->IsValid())
		{
			(*ExpectObject)->TryGetBoolField(TEXT("ok"), bExpectedOk);
			(*ExpectObject)->TryGetStringField(TEXT("reason"), ExpectedReason);
		}

		TestEqual(*FString::Printf(TEXT("%s result ok"), *FixtureName), Result.bOk, bExpectedOk);
		TestEqual(*FString::Printf(TEXT("%s result reason"), *FixtureName), Result.Reason, ExpectedReason);

		FString StateReason;
		TestTrue(*FString::Printf(TEXT("%s final state matches: %s"), *FixtureName, *StateReason), ExpectedStateMatches(Fixture, State, StateReason));

		FString TraceReason;
		TestTrue(*FString::Printf(TEXT("%s trace matches: %s"), *FixtureName, *TraceReason), ExpectedTraceMatches(Fixture, Result.TraceEvents, TraceReason));
	}

	return true;
}

#endif
