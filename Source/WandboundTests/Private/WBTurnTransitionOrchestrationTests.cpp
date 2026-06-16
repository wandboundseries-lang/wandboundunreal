#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBEffectRunner.h"
#include "WBGameStateData.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeTransitionPlayer(
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

FWBUnitState MakeTransitionUnit(
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

FWBGameStateData MakeTransitionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeTransitionPlayer(0, 2, 2));
	State.Players.Add(MakeTransitionPlayer(1));
	return State;
}

FWBAction MakeEndTurnAction(const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = PlayerId;
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

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString TransitionFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadTransitionFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *TransitionFixturePath(FileName)))
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
		OutState.Players.Add(MakeTransitionPlayer(0));
		OutState.Players.Add(MakeTransitionPlayer(1));
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

bool BuildTransitionStateFromFixture(const TSharedPtr<FJsonObject>& Fixture, FWBGameStateData& OutState)
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

bool ReadFixtureOperation(const TSharedPtr<FJsonObject>& Fixture, int32& OutEndingPlayerId, int32& OutExplicitMPRoll)
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
		|| Kind != TEXT("apply_deterministic_turn_transition"))
	{
		return false;
	}

	return TryReadIntegerField(*OperationObject, TEXT("ending_player_id"), OutEndingPlayerId)
		&& TryReadIntegerField(*OperationObject, TEXT("next_player_explicit_mp_roll"), OutExplicitMPRoll);
}

int32 FindTraceKindIndex(const TArray<FWBTraceEvent>& TraceEvents, const FName Kind, const int32 StartIndex = 0)
{
	for (int32 Index = StartIndex; Index < TraceEvents.Num(); ++Index)
	{
		if (TraceEvents[Index].Kind == Kind)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 FindTraceStatusIndex(
	const TArray<FWBTraceEvent>& TraceEvents,
	const FName Kind,
	const FName StatusId,
	const int32 TargetUnitId,
	const int32 StartIndex = 0)
{
	for (int32 Index = StartIndex; Index < TraceEvents.Num(); ++Index)
	{
		if (TraceEvents[Index].Kind == Kind
			&& TraceEvents[Index].StatusId == StatusId
			&& (TargetUnitId == -1 || TraceEvents[Index].TargetUnitId == TargetUnitId))
		{
			return Index;
		}
	}

	return INDEX_NONE;
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

		bool bExpectedExpiredStatus = false;
		if (ExpectedEvent->TryGetBoolField(TEXT("expired_status"), bExpectedExpiredStatus)
			&& Event.bExpiredStatus != bExpectedExpiredStatus)
		{
			OutReason = TEXT("trace_expired_status_mismatch");
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionValidSequenceTest, "Wandbound.Core.TurnTransition.ValidSequence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionValidSequenceTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	FWBUnitState EndingUnit = MakeTransitionUnit(1, 0, FWBTile(4, 4), 5, 5, 1, 1);
	EndingUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState NextUnit = MakeTransitionUnit(2, 1, FWBTile(6, 4), 5, 5, 0, 2);
	NextUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Ending unit added"), State.AddUnitForTest(EndingUnit));
	TestTrue(TEXT("Next unit added"), State.AddUnitForTest(NextUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 0, 4);

	TestTrue(TEXT("Transition succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advanced"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Priority player advanced"), State.PriorityPlayer, 1);
	TestTrue(TEXT("Phase normal"), State.IsNormalTurnPhase());
	TestEqual(TEXT("Turn number increments using current baseline"), State.TurnNumber, 4);
	TestEqual(TEXT("Next player MP applied"), State.GetPlayerById(1)->RemainingMP, 4);
	TestEqual(TEXT("Next player last roll recorded"), State.GetPlayerById(1)->LastMPRoll, 4);
	TestEqual(TEXT("Next player walls reset"), State.GetPlayerById(1)->WallsLeft, 1);
	TestEqual(TEXT("Next player attacks reset"), State.GetUnitById(2)->AttacksLeft, 2);
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 7);
	if (Result.TraceEvents.Num() == 7)
	{
		TestEqual(TEXT("Trace 0 transition"), Result.TraceEvents[0].Kind, FName(TEXT("turn_transition")));
		TestEqual(TEXT("Trace 1 end status"), Result.TraceEvents[1].Kind, FName(TEXT("end_turn_status_ticks")));
		TestEqual(TEXT("Trace 2 burn"), Result.TraceEvents[2].StatusId, FName(TEXT("Burn")));
		TestEqual(TEXT("Trace 3 end turn"), Result.TraceEvents[3].Kind, FName(TEXT("end_turn")));
		TestEqual(TEXT("Trace 4 start status"), Result.TraceEvents[4].Kind, FName(TEXT("start_turn_status_ticks")));
		TestEqual(TEXT("Trace 5 poison"), Result.TraceEvents[5].StatusId, FName(TEXT("Poison")));
		TestEqual(TEXT("Trace 6 resources"), Result.TraceEvents[6].Kind, FName(TEXT("turn_start_resource_setup")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionBurnThenPoisonOrderTest, "Wandbound.Core.TurnTransition.BurnThenPoisonOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionBurnThenPoisonOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	FWBUnitState BurnUnit = MakeTransitionUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeTransitionUnit(2, 1, FWBTile(6, 4), 5, 5);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 0, 4);

	TestTrue(TEXT("Transition succeeds"), Result.bOk);
	TestEqual(TEXT("Burn damage applied"), State.GetUnitById(1)->HP, 4);
	TestEqual(TEXT("Poison MaxHP applied"), State.GetUnitById(2)->MaxHP, 4);
	const int32 BurnIndex = FindTraceStatusIndex(Result.TraceEvents, FName(TEXT("status_tick")), FName(TEXT("Burn")), 1);
	const int32 EndTurnIndex = FindTraceKindIndex(Result.TraceEvents, FName(TEXT("end_turn")));
	const int32 PoisonIndex = FindTraceStatusIndex(Result.TraceEvents, FName(TEXT("status_tick")), FName(TEXT("Poison")), 2);
	const int32 ResourceIndex = FindTraceKindIndex(Result.TraceEvents, FName(TEXT("turn_start_resource_setup")));
	TestTrue(TEXT("Burn trace precedes end turn"), BurnIndex != INDEX_NONE && EndTurnIndex != INDEX_NONE && BurnIndex < EndTurnIndex);
	TestTrue(TEXT("Poison trace follows end turn"), PoisonIndex != INDEX_NONE && EndTurnIndex != INDEX_NONE && EndTurnIndex < PoisonIndex);
	TestTrue(TEXT("Resources happen after poison"), ResourceIndex != INDEX_NONE && PoisonIndex != INDEX_NONE && PoisonIndex < ResourceIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionExpirationOrderTest, "Wandbound.Core.TurnTransition.ExpirationOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionExpirationOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	FWBUnitState BurnUnit = MakeTransitionUnit(1, 0, FWBTile(1, 1), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 1);
	FWBUnitState RootedUnit = MakeTransitionUnit(2, 0, FWBTile(3, 1), 5, 5);
	RootedUnit.AddStatus(FName(TEXT("Rooted")), 1);
	FWBUnitState StunnedUnit = MakeTransitionUnit(3, 0, FWBTile(5, 1), 5, 5);
	StunnedUnit.AddStatus(FName(TEXT("Stunned")), 1);
	FWBUnitState PoisonUnit = MakeTransitionUnit(4, 1, FWBTile(7, 1), 5, 5);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 1);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Rooted unit added"), State.AddUnitForTest(RootedUnit));
	TestTrue(TEXT("Stunned unit added"), State.AddUnitForTest(StunnedUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 0, 3);

	TestTrue(TEXT("Transition succeeds"), Result.bOk);
	TestFalse(TEXT("Burn expired"), State.GetUnitById(1)->HasStatus(FName(TEXT("Burn"))));
	TestFalse(TEXT("Rooted expired"), State.GetUnitById(2)->HasStatus(FName(TEXT("Rooted"))));
	TestFalse(TEXT("Stunned expired"), State.GetUnitById(3)->HasStatus(FName(TEXT("Stunned"))));
	TestFalse(TEXT("Poison expired"), State.GetUnitById(4)->HasStatus(FName(TEXT("Poison"))));

	const int32 EndTurnIndex = FindTraceKindIndex(Result.TraceEvents, FName(TEXT("end_turn")));
	const int32 BurnExpiredIndex = FindTraceStatusIndex(Result.TraceEvents, FName(TEXT("status_expired")), FName(TEXT("Burn")), 1);
	const int32 RootedExpiredIndex = FindTraceStatusIndex(Result.TraceEvents, FName(TEXT("status_expired")), FName(TEXT("Rooted")), 2);
	const int32 StunnedExpiredIndex = FindTraceStatusIndex(Result.TraceEvents, FName(TEXT("status_expired")), FName(TEXT("Stunned")), 3);
	const int32 PoisonExpiredIndex = FindTraceStatusIndex(Result.TraceEvents, FName(TEXT("status_expired")), FName(TEXT("Poison")), 4);
	const int32 ResourceIndex = FindTraceKindIndex(Result.TraceEvents, FName(TEXT("turn_start_resource_setup")));
	TestTrue(TEXT("End-turn expirations precede end turn"), BurnExpiredIndex < EndTurnIndex && RootedExpiredIndex < EndTurnIndex && StunnedExpiredIndex < EndTurnIndex);
	TestTrue(TEXT("Poison expiration precedes resource setup"), EndTurnIndex < PoisonExpiredIndex && PoisonExpiredIndex < ResourceIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionInvalidRollNoMutationTest, "Wandbound.Core.TurnTransition.InvalidRollNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionInvalidRollNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	FWBUnitState BurnUnit = MakeTransitionUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeTransitionUnit(2, 1, FWBTile(6, 4), 5, 5);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	const FWBGameStateData Before = State;
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 0, 0);

	TestFalse(TEXT("Transition fails"), Result.bOk);
	TestEqual(TEXT("Invalid roll reason"), Result.Reason, FString(TEXT("invalid_mp_roll")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn number unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("Burn HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("Poison MaxHP unchanged"), State.GetUnitById(2)->MaxHP, Before.GetUnitById(2)->MaxHP);
	TestEqual(TEXT("Next MP unchanged"), State.GetPlayerById(1)->RemainingMP, Before.GetPlayerById(1)->RemainingMP);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionInvalidPlayerNoMutationTest, "Wandbound.Core.TurnTransition.InvalidPlayerNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionInvalidPlayerNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	FWBUnitState BurnUnit = MakeTransitionUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));

	const FWBGameStateData Before = State;
	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 1, 4);

	TestFalse(TEXT("Transition fails"), Result.bOk);
	TestEqual(TEXT("Invalid player reason"), Result.Reason, FString(TEXT("not_active_player")));
	TestEqual(TEXT("Current unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn number unchanged"), State.TurnNumber, Before.TurnNumber);
	TestEqual(TEXT("Burn HP unchanged"), State.GetUnitById(1)->HP, Before.GetUnitById(1)->HP);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionGameOverNoMutationTest, "Wandbound.Core.TurnTransition.GameOverNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionGameOverNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	State.bGameOver = true;
	FWBUnitState BurnUnit = MakeTransitionUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 0, 4);

	TestFalse(TEXT("Transition fails"), Result.bOk);
	TestEqual(TEXT("Game over reason"), Result.Reason, FString(TEXT("game_over")));
	TestEqual(TEXT("HP unchanged"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("No traces"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBApplyEndTurnRemainsBasicTest, "Wandbound.Core.TurnTransition.ApplyEndTurnRemainsBasic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBApplyEndTurnRemainsBasicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	FWBUnitState BurnUnit = MakeTransitionUnit(1, 0, FWBTile(4, 4), 5, 5);
	BurnUnit.AddStatus(FName(TEXT("Burn")), 2);
	FWBUnitState PoisonUnit = MakeTransitionUnit(2, 1, FWBTile(6, 4), 5, 5);
	PoisonUnit.AddStatus(FName(TEXT("Poison")), 2);
	TestTrue(TEXT("Burn unit added"), State.AddUnitForTest(BurnUnit));
	TestTrue(TEXT("Poison unit added"), State.AddUnitForTest(PoisonUnit));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyEndTurn(State, MakeEndTurnAction(0));

	TestTrue(TEXT("Basic end turn succeeds"), Result.bOk);
	TestEqual(TEXT("Only end turn trace"), Result.TraceEvents.Num(), 1);
	TestEqual(TEXT("Trace kind"), Result.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Burn did not tick"), State.GetUnitById(1)->HP, 5);
	TestEqual(TEXT("Poison did not tick"), State.GetUnitById(2)->MaxHP, 5);
	TestEqual(TEXT("Resource setup did not run"), State.GetPlayerById(1)->RemainingMP, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionLegalActionsAfterTransitionTest, "Wandbound.Core.TurnTransition.LegalActionsAfterTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionLegalActionsAfterTransitionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeTransitionState();
	TestTrue(TEXT("Old player unit added"), State.AddUnitForTest(MakeTransitionUnit(1, 0, FWBTile(4, 4))));
	TestTrue(TEXT("New player unit added"), State.AddUnitForTest(MakeTransitionUnit(2, 1, FWBTile(6, 4))));

	const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, 0, 4);

	TestTrue(TEXT("Transition succeeds"), Result.bOk);
	const TArray<FWBAction> NewPlayerActions = WBRules::GenerateLegalActionsForPlayer(State, 1);
	TestTrue(TEXT("New player gets legal actions"), NewPlayerActions.Num() > 1);
	TestEqual(TEXT("Old player gets no normal actions"), WBRules::GenerateLegalActionsForPlayer(State, 0).Num(), 0);
	bool bHasMove = false;
	for (const FWBAction& Action : NewPlayerActions)
	{
		bHasMove = bHasMove || Action.Type == EWBActionType::Move;
	}
	TestTrue(TEXT("Explicit MP enables movement"), bHasMove);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBDeterministicTurnTransitionFixtureScenariosTest, "Wandbound.Core.TurnTransition.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBDeterministicTurnTransitionFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("turn_transition_burn_then_poison_then_setup.json"),
		TEXT("turn_transition_invalid_roll_no_mutation.json"),
		TEXT("turn_transition_invalid_player_no_mutation.json"),
		TEXT("turn_transition_status_expiration_order.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadTransitionFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FWBGameStateData State;
		TestTrue(*FString::Printf(TEXT("%s initial state builds"), *FixtureName), BuildTransitionStateFromFixture(Fixture, State));

		int32 EndingPlayerId = -1;
		int32 ExplicitRoll = 0;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), ReadFixtureOperation(Fixture, EndingPlayerId, ExplicitRoll));

		const FWBApplyActionResult Result = WBEffectRunner::ApplyDeterministicTurnTransition(State, EndingPlayerId, ExplicitRoll);

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
