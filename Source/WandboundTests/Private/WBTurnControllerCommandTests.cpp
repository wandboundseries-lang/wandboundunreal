#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBRules.h"
#include "WBTurnController.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeCommandPlayer(
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

FWBUnitState MakeCommandUnit(
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

FWBGameStateData MakeCommandState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeCommandPlayer(0, 2, 2));
	State.Players.Add(MakeCommandPlayer(1));
	return State;
}

FWBTurnCommand MakeTurnCommand(
	const EWBTurnCommandMode Mode,
	const int32 ActingPlayerId,
	const int32 NextPlayerExplicitMPRoll = 0)
{
	FWBTurnCommand Command;
	Command.Mode = Mode;
	Command.ActingPlayerId = ActingPlayerId;
	Command.NextPlayerExplicitMPRoll = NextPlayerExplicitMPRoll;
	return Command;
}

bool ContainsActionType(const TArray<FWBAction>& Actions, const EWBActionType Type)
{
	for (const FWBAction& Action : Actions)
	{
		if (Action.Type == Type)
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

bool ReadBoolFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const bool bDefaultValue)
{
	bool bValue = bDefaultValue;
	if (Object.IsValid())
	{
		Object->TryGetBoolField(FieldName, bValue);
	}
	return bValue;
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

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString TurnCommandFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadTurnCommandFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *TurnCommandFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

bool ApplyFixturePlayers(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState)
{
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("players"), Players) || Players == nullptr)
	{
		OutState.Players.Add(MakeCommandPlayer(0));
		OutState.Players.Add(MakeCommandPlayer(1));
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

		Unit.HP = ReadIntegerFieldOrDefault(UnitObject, TEXT("hp"), Unit.HP);
		Unit.MaxHP = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_hp"), Unit.MaxHP);
		Unit.AttacksLeft = ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), Unit.AttacksLeft);
		Unit.MaxAttacksPerTurn = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_attacks_per_turn"), Unit.MaxAttacksPerTurn);
		Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 0);
		if (!OutState.AddUnitForTest(Unit))
		{
			return false;
		}
	}

	return true;
}

void ApplyFixtureStatuses(FWBGameStateData& State, const TSharedPtr<FJsonObject>& StateObject)
{
	const TSharedPtr<FJsonObject>* StatusesByUnit = nullptr;
	if (!StateObject->TryGetObjectField(TEXT("statuses_by_unit"), StatusesByUnit)
		|| StatusesByUnit == nullptr
		|| !StatusesByUnit->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& UnitStatusesPair : (*StatusesByUnit)->Values)
	{
		FWBUnitState* Unit = State.GetMutableUnitById(FCString::Atoi(*UnitStatusesPair.Key));
		const TSharedPtr<FJsonObject> UnitStatuses = UnitStatusesPair.Value.IsValid()
			? UnitStatusesPair.Value->AsObject()
			: nullptr;
		if (Unit == nullptr || !UnitStatuses.IsValid())
		{
			continue;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& StatusPair : UnitStatuses->Values)
		{
			const TSharedPtr<FJsonObject> StatusObject = StatusPair.Value.IsValid()
				? StatusPair.Value->AsObject()
				: nullptr;
			FString StatusId = StatusPair.Key;
			int32 TurnsRemaining = 0;
			if (StatusObject.IsValid())
			{
				StatusObject->TryGetStringField(TEXT("status_id"), StatusId);
				TurnsRemaining = ReadIntegerFieldOrDefault(
					StatusObject,
					TEXT("turns_remaining"),
					ReadIntegerFieldOrDefault(StatusObject, TEXT("ticks_remaining"), 0));
			}

			Unit->AddStatus(FName(*StatusId), TurnsRemaining);
		}
	}
}

bool BuildTurnCommandStateFromFixture(const TSharedPtr<FJsonObject>& Fixture, FWBGameStateData& OutState)
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
	OutState.bGameOver = ReadBoolFieldOrDefault(*StateObject, TEXT("game_over"), false);

	if (!ApplyFixturePlayers(*StateObject, OutState) || !ApplyFixtureUnits(*StateObject, OutState))
	{
		return false;
	}

	ApplyFixtureStatuses(OutState, *StateObject);
	return true;
}

bool ReadFixtureOperation(const TSharedPtr<FJsonObject>& Fixture, FWBTurnCommand& OutCommand)
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
	if (!(*OperationObject)->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("apply_turn_command"))
	{
		return false;
	}

	FString Mode;
	if (!(*OperationObject)->TryGetStringField(TEXT("mode"), Mode)
		|| !TryReadIntegerField(*OperationObject, TEXT("acting_player_id"), OutCommand.ActingPlayerId))
	{
		return false;
	}

	if (Mode == TEXT("basic_end_turn"))
	{
		OutCommand.Mode = EWBTurnCommandMode::BasicEndTurn;
	}
	else if (Mode == TEXT("deterministic_full_transition"))
	{
		OutCommand.Mode = EWBTurnCommandMode::DeterministicFullTransition;
	}
	else
	{
		return false;
	}

	OutCommand.NextPlayerExplicitMPRoll = ReadIntegerFieldOrDefault(
		*OperationObject,
		TEXT("next_player_explicit_mp_roll"),
		0);
	return true;
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

	if (State.CurrentPlayer != ReadIntegerFieldOrDefault(*FinalStateObject, TEXT("current_player"), State.CurrentPlayer)
		|| State.PriorityPlayer != ReadIntegerFieldOrDefault(*FinalStateObject, TEXT("priority_player"), State.PriorityPlayer)
		|| State.TurnNumber != ReadIntegerFieldOrDefault(*FinalStateObject, TEXT("turn_number"), State.TurnNumber)
		|| State.Phase != ReadPhaseOrDefault(*FinalStateObject, State.Phase))
	{
		OutReason = TEXT("turn_state_mismatch");
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
				OutReason = FString::Printf(TEXT("player_%d_mismatch"), PlayerId);
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
				OutReason = FString::Printf(TEXT("missing_unit_%d"), UnitId);
				return false;
			}

			if (Unit->HP != ReadIntegerFieldOrDefault(UnitObject, TEXT("hp"), Unit->HP)
				|| Unit->MaxHP != ReadIntegerFieldOrDefault(UnitObject, TEXT("max_hp"), Unit->MaxHP)
				|| Unit->AttacksLeft != ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), Unit->AttacksLeft))
			{
				OutReason = FString::Printf(TEXT("unit_%d_state_mismatch"), UnitId);
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>* Statuses = nullptr;
			if (UnitObject->TryGetArrayField(TEXT("statuses"), Statuses) && Statuses != nullptr)
			{
				if (Unit->Statuses.Num() != Statuses->Num())
				{
					OutReason = FString::Printf(TEXT("unit_%d_status_count_mismatch"), UnitId);
					return false;
				}

				for (const TSharedPtr<FJsonValue>& StatusValue : *Statuses)
				{
					if (!StatusValue.IsValid() || StatusValue->Type != EJson::String || !Unit->HasStatus(FName(*StatusValue->AsString())))
					{
						OutReason = FString::Printf(TEXT("unit_%d_status_mismatch"), UnitId);
						return false;
					}
				}
			}

			const TSharedPtr<FJsonObject>* StatusTurns = nullptr;
			if (UnitObject->TryGetObjectField(TEXT("status_turns"), StatusTurns)
				&& StatusTurns != nullptr
				&& StatusTurns->IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& StatusTurnsPair : (*StatusTurns)->Values)
				{
					if (!StatusTurnsPair.Value.IsValid() || StatusTurnsPair.Value->Type != EJson::Number)
					{
						OutReason = FString::Printf(TEXT("unit_%d_malformed_status_turns"), UnitId);
						return false;
					}

					if (Unit->GetStatusTurnsRemaining(FName(*StatusTurnsPair.Key)) != static_cast<int32>(StatusTurnsPair.Value->AsNumber()))
					{
						OutReason = FString::Printf(TEXT("unit_%d_status_turns_mismatch"), UnitId);
						return false;
					}
				}
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
		FString ExpectedKind;
		if (!ExpectedEvent->TryGetStringField(TEXT("kind"), ExpectedKind) || Event.Kind.ToString() != ExpectedKind)
		{
			OutReason = TEXT("trace_kind_mismatch");
			return false;
		}

		FString ExpectedStatusId;
		if (ExpectedEvent->TryGetStringField(TEXT("status_id"), ExpectedStatusId) && Event.StatusId.ToString() != ExpectedStatusId)
		{
			OutReason = TEXT("trace_status_id_mismatch");
			return false;
		}

		if (Event.PlayerId != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("player_id"), Event.PlayerId)
			|| Event.FromPlayer != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("from_player"), Event.FromPlayer)
			|| Event.ToPlayer != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("to_player"), Event.ToPlayer)
			|| Event.NextPlayerId != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("next_player_id"), Event.NextPlayerId)
			|| Event.TurnNumber != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("turn_number"), Event.TurnNumber)
			|| Event.TurnNumberBefore != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("turn_number_before"), Event.TurnNumberBefore)
			|| Event.TurnNumberAfter != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("turn_number_after"), Event.TurnNumberAfter)
			|| Event.MPRoll != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("mp_roll"), Event.MPRoll)
			|| Event.RemainingMP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("remaining_mp"), Event.RemainingMP)
			|| Event.WallsLeft != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("walls_left"), Event.WallsLeft)
			|| Event.WallRemovalsLeft != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("wall_removals_left"), Event.WallRemovalsLeft)
			|| Event.TargetUnitId != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("target_unit_id"), Event.TargetUnitId)
			|| Event.PreviousHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("previous_hp"), Event.PreviousHP)
			|| Event.NewHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("new_hp"), Event.NewHP)
			|| Event.PreviousMaxHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("previous_max_hp"), Event.PreviousMaxHP)
			|| Event.NewMaxHP != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("new_max_hp"), Event.NewMaxHP)
			|| Event.PreviousStatusTurns != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("previous_status_turns"), Event.PreviousStatusTurns)
			|| Event.NewStatusTurns != ReadIntegerFieldOrDefault(ExpectedEvent, TEXT("new_status_turns"), Event.NewStatusTurns))
		{
			OutReason = TEXT("trace_field_mismatch");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandBasicEndTurnOnlyTest, "Wandbound.Core.TurnController.BasicEndTurnOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandBasicEndTurnOnlyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCommandState();
	FWBUnitState BurnUnit = MakeCommandUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeCommandUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	FString Reason;
	const FWBTurnCommand Command = MakeTurnCommand(EWBTurnCommandMode::BasicEndTurn, 0, 4);
	TestTrue(TEXT("Basic command validates"), WBTurnController::CanApplyTurnCommand(State, Command, Reason));

	const FWBApplyActionResult Result = WBTurnController::ApplyTurnCommand(State, Command);

	TestTrue(TEXT("Basic command succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Priority follows current player"), State.PriorityPlayer, 1);
	TestEqual(TEXT("Turn number increments"), State.TurnNumber, 4);
	TestEqual(TEXT("Burn does not tick"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("Poison does not tick"), State.GetUnitById(2)->MaxHP, 5);
	TestEqual(TEXT("Next player MP is not reset"), State.GetPlayerById(1)->RemainingMP, 0);
	TestEqual(TEXT("Next player roll is not recorded"), State.GetPlayerById(1)->LastMPRoll, 0);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 1);
	if (Result.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandFullTransitionTest, "Wandbound.Core.TurnController.FullTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandFullTransitionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCommandState();
	FWBUnitState BurnUnit = MakeCommandUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeCommandUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	const FWBApplyActionResult Result = WBTurnController::ApplyTurnCommand(
		State,
		MakeTurnCommand(EWBTurnCommandMode::DeterministicFullTransition, 0, 4));

	TestTrue(TEXT("Full command succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Burn applies"), State.GetUnitById(1)->HP, 4);
	TestEqual(TEXT("Poison MaxHP applies"), State.GetUnitById(2)->MaxHP, 4);
	TestEqual(TEXT("Poison HP clamps"), State.GetUnitById(2)->HP, 4);
	TestEqual(TEXT("Next player MP is setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestEqual(TEXT("Next player roll recorded"), State.GetPlayerById(1)->LastMPRoll, 4);
	TestEqual(TEXT("Next player attacks reset"), State.GetUnitById(2)->AttacksLeft, 2);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 7);
	if (Result.TraceEvents.Num() == 7)
	{
		TestEqual(TEXT("Trace 0 transition"), Result.TraceEvents[0].Kind, FName(TEXT("turn_transition")));
		TestEqual(TEXT("Trace 1 end status"), Result.TraceEvents[1].Kind, FName(TEXT("end_turn_status_ticks")));
		TestEqual(TEXT("Trace 2 Burn"), Result.TraceEvents[2].StatusId, FName(TEXT("Burn")));
		TestEqual(TEXT("Trace 3 end turn"), Result.TraceEvents[3].Kind, FName(TEXT("end_turn")));
		TestEqual(TEXT("Trace 4 start status"), Result.TraceEvents[4].Kind, FName(TEXT("start_turn_status_ticks")));
		TestEqual(TEXT("Trace 5 Poison"), Result.TraceEvents[5].StatusId, FName(TEXT("Poison")));
		TestEqual(TEXT("Trace 6 resources"), Result.TraceEvents[6].Kind, FName(TEXT("turn_start_resource_setup")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandInvalidRollNoMutationTest, "Wandbound.Core.TurnController.InvalidRollNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandInvalidRollNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCommandState();
	FWBUnitState BurnUnit = MakeCommandUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeCommandUnit(2, 1, FWBTile(6, 4), 5, 5);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));
	const FWBGameStateData Before = State;

	const FWBApplyActionResult Result = WBTurnController::ApplyTurnCommand(
		State,
		MakeTurnCommand(EWBTurnCommandMode::DeterministicFullTransition, 0, 0));

	TestFalse(TEXT("Full command fails"), Result.bOk);
	TestEqual(TEXT("Invalid roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("Burn HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("Poison MaxHP unchanged"), State.GetUnitById(2)->MaxHP, Before.GetUnitById(2)->MaxHP);
	TestEqual(TEXT("Next player MP unchanged"), State.GetPlayerById(1)->RemainingMP, Before.GetPlayerById(1)->RemainingMP);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandBasicIgnoresRollTest, "Wandbound.Core.TurnController.BasicIgnoresRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandBasicIgnoresRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCommandState();
	TestTrue(TEXT("Current unit added"), State.AddUnitForTest(MakeCommandUnit(1, 0, FWBTile(4, 4))));
	const FWBApplyActionResult Result = WBTurnController::ApplyTurnCommand(
		State,
		MakeTurnCommand(EWBTurnCommandMode::BasicEndTurn, 0, 4));

	TestTrue(TEXT("Basic command succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Roll is ignored"), State.GetPlayerById(1)->LastMPRoll, 0);
	TestEqual(TEXT("MP is not setup"), State.GetPlayerById(1)->RemainingMP, 0);
	TestEqual(TEXT("Walls are not reset"), State.GetPlayerById(1)->WallsLeft, 0);
	TestEqual(TEXT("Only basic trace"), Result.TraceEvents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandInvalidActingPlayerFailsTest, "Wandbound.Core.TurnController.InvalidActingPlayerFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandInvalidActingPlayerFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BasicState = MakeCommandState();
	const FWBApplyActionResult BasicResult = WBTurnController::ApplyTurnCommand(
		BasicState,
		MakeTurnCommand(EWBTurnCommandMode::BasicEndTurn, 1, 4));

	TestFalse(TEXT("Basic command fails for non-current player"), BasicResult.bOk);
	TestEqual(TEXT("Basic invalid player reason"), BasicResult.Reason, FString(TEXT("not_active_player")));
	TestEqual(TEXT("Basic current unchanged"), BasicState.CurrentPlayer, 0);
	TestEqual(TEXT("Basic turn unchanged"), BasicState.TurnNumber, 3);
	TestEqual(TEXT("Basic emits no traces"), BasicResult.TraceEvents.Num(), 0);

	FWBGameStateData FullState = MakeCommandState();
	FWBUnitState BurnUnit = MakeCommandUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Full burn unit added"), FullState.AddUnitForTest(BurnUnit));
	const FWBApplyActionResult FullResult = WBTurnController::ApplyTurnCommand(
		FullState,
		MakeTurnCommand(EWBTurnCommandMode::DeterministicFullTransition, 1, 4));

	TestFalse(TEXT("Full command fails for non-current player"), FullResult.bOk);
	TestEqual(TEXT("Full invalid player reason"), FullResult.Reason, FString(TEXT("not_active_player")));
	TestEqual(TEXT("Full current unchanged"), FullState.CurrentPlayer, 0);
	TestEqual(TEXT("Full burn HP unchanged"), FullState.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("Full emits no traces"), FullResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandGameOverFailsTest, "Wandbound.Core.TurnController.GameOverFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandGameOverFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BasicState = MakeCommandState();
	BasicState.bGameOver = true;
	const FWBApplyActionResult BasicResult = WBTurnController::ApplyTurnCommand(
		BasicState,
		MakeTurnCommand(EWBTurnCommandMode::BasicEndTurn, 0, 4));
	TestFalse(TEXT("Basic command fails after game over"), BasicResult.bOk);
	TestEqual(TEXT("Basic game over reason"), BasicResult.Reason, FString(TEXT("game_over")));
	TestEqual(TEXT("Basic current unchanged"), BasicState.CurrentPlayer, 0);
	TestEqual(TEXT("Basic emits no traces"), BasicResult.TraceEvents.Num(), 0);

	FWBGameStateData FullState = MakeCommandState();
	FullState.bGameOver = true;
	const FWBApplyActionResult FullResult = WBTurnController::ApplyTurnCommand(
		FullState,
		MakeTurnCommand(EWBTurnCommandMode::DeterministicFullTransition, 0, 4));
	TestFalse(TEXT("Full command fails after game over"), FullResult.bOk);
	TestEqual(TEXT("Full game over reason"), FullResult.Reason, FString(TEXT("game_over")));
	TestEqual(TEXT("Full current unchanged"), FullState.CurrentPlayer, 0);
	TestEqual(TEXT("Full emits no traces"), FullResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandInvalidModeFailsTest, "Wandbound.Core.TurnController.InvalidModeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandInvalidModeFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCommandState();
	FWBTurnCommand Command = MakeTurnCommand(EWBTurnCommandMode::BasicEndTurn, 0);
	Command.Mode = static_cast<EWBTurnCommandMode>(255);

	const FWBApplyActionResult Result = WBTurnController::ApplyTurnCommand(State, Command);

	TestFalse(TEXT("Invalid mode fails"), Result.bOk);
	TestEqual(TEXT("Invalid mode reason"), Result.Reason, FString(TEXT("unsupported_turn_command_mode")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, 0);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, 3);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandLegalActionsRemainPlayerActionsTest, "Wandbound.Core.TurnController.LegalActionsRemainPlayerActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandLegalActionsRemainPlayerActionsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeCommandState();
	State.GetMutablePlayerById(0)->RemainingMP = 1;
	TestTrue(TEXT("Current unit added"), State.AddUnitForTest(MakeCommandUnit(1, 0, FWBTile(4, 4))));

	const TArray<FWBAction> Actions = WBRules::GenerateLegalActions(State);
	const TArray<FString> ActionIds = WBActionCodec::MakeActionIds(Actions);

	TestTrue(TEXT("EndTurn action remains generated"), ContainsActionType(Actions, EWBActionType::EndTurn));
	TestTrue(TEXT("Movement remains generated before EndTurn"), Actions.Num() > 1 && Actions[0].Type == EWBActionType::Move);
	TestTrue(TEXT("EndTurn remains last"), Actions.Num() > 0 && Actions.Last().Type == EWBActionType::EndTurn);
	for (const FString& ActionId : ActionIds)
	{
		TestFalse(*FString::Printf(TEXT("%s is not a turn transition action"), *ActionId), ActionId.Contains(TEXT("turn_transition")));
		TestFalse(*FString::Printf(TEXT("%s is not a resource setup action"), *ActionId), ActionId.Contains(TEXT("turn_start_resource_setup")));
		TestFalse(*FString::Printf(TEXT("%s is not a status tick action"), *ActionId), ActionId.Contains(TEXT("status_tick")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBTurnCommandFixtureScenariosTest, "Wandbound.Core.TurnController.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTurnCommandFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("turn_command_basic_end_turn_only.json"),
		TEXT("turn_command_full_transition.json"),
		TEXT("turn_command_full_transition_invalid_roll_no_mutation.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadTurnCommandFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FWBGameStateData State;
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), BuildTurnCommandStateFromFixture(Fixture, State));

		FWBTurnCommand Command;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), ReadFixtureOperation(Fixture, Command));

		const FWBApplyActionResult Result = WBTurnController::ApplyTurnCommand(State, Command);

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
