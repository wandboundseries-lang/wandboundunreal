#include "WBReplayFixtureTestUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"

namespace WandboundTest
{
namespace
{
FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
{
	FString Output;
	if (!Object.IsValid())
	{
		return Output;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Output;
}

bool ReadStringArrayValues(
	const TArray<TSharedPtr<FJsonValue>>& Values,
	const FString& FieldPath,
	TArray<FString>& OutStrings,
	FString& OutReason)
{
	OutStrings.Reset();
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = Values[Index];
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			OutReason = FString::Printf(TEXT("malformed_%s[%d]"), *FieldPath, Index);
			return false;
		}

		OutStrings.Add(Value->AsString());
	}

	OutReason.Reset();
	return true;
}

bool TryGetReplayLogObject(
	const TSharedPtr<FJsonObject>& Fixture,
	TSharedPtr<FJsonObject>& OutReplayLogObject,
	FString& OutReason)
{
	OutReplayLogObject.Reset();

	const TSharedPtr<FJsonObject>* ReplayLogObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("replay_log"), ReplayLogObject)
		|| ReplayLogObject == nullptr
		|| !ReplayLogObject->IsValid())
	{
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	OutReplayLogObject = *ReplayLogObject;
	OutReason.Reset();
	return true;
}

bool TryGetReplayDecisions(
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<TSharedPtr<FJsonValue>>*& OutDecisions,
	FString& OutReason)
{
	OutDecisions = nullptr;

	TSharedPtr<FJsonObject> ReplayLogObject;
	if (!TryGetReplayLogObject(Fixture, ReplayLogObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!ReplayLogObject->TryGetArrayField(TEXT("decisions"), Decisions) || Decisions == nullptr)
	{
		OutReason = TEXT("missing_replay_log_decisions");
		return false;
	}

	OutDecisions = Decisions;
	OutReason.Reset();
	return true;
}

bool RemoveObjectFieldAndExtractReplayLog(
	const TSharedPtr<FJsonObject>& Fixture,
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldPath,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	if (!Object.IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_%s"), *FieldPath);
		return false;
	}

	const FString FieldNameString(FieldName);
	const TSharedPtr<FJsonValue>* OriginalValuePtr = Object->Values.Find(FieldNameString);
	if (OriginalValuePtr == nullptr || !OriginalValuePtr->IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_%s_%s"), *FieldPath, FieldName);
		return false;
	}

	const TSharedPtr<FJsonValue> OriginalValue = *OriginalValuePtr;
	Object->Values.Remove(FieldNameString);
	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		Object->Values.Add(FieldNameString, OriginalValue);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	Object->Values.Add(FieldNameString, OriginalValue);
	OutReason.Reset();
	return true;
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

bool TryGetExpectedObject(const TSharedPtr<FJsonObject>& Fixture, TSharedPtr<FJsonObject>& OutExpectedObject, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expected"), ExpectedObject)
		&& ExpectedObject != nullptr
		&& ExpectedObject->IsValid())
	{
		OutExpectedObject = *ExpectedObject;
		OutReason.Reset();
		return true;
	}

	const TSharedPtr<FJsonObject>* ExpectObject = nullptr;
	if (Fixture.IsValid()
		&& Fixture->TryGetObjectField(TEXT("expect"), ExpectObject)
		&& ExpectObject != nullptr
		&& ExpectObject->IsValid())
	{
		OutExpectedObject = *ExpectObject;
		OutReason.Reset();
		return true;
	}

	OutReason = TEXT("missing_expected");
	return false;
}

bool TryGetOperationKindAndObject(
	const TSharedPtr<FJsonObject>& Fixture,
	FString& OutOperationKind,
	TSharedPtr<FJsonObject>& OutOperationObject,
	FString& OutReason)
{
	OutOperationKind.Reset();
	OutOperationObject.Reset();

	if (!Fixture.IsValid())
	{
		OutReason = TEXT("missing_fixture");
		return false;
	}

	const TSharedPtr<FJsonValue>* OperationValue = Fixture->Values.Find(TEXT("operation"));
	if (OperationValue == nullptr || !OperationValue->IsValid())
	{
		OutReason = TEXT("missing_operation");
		return false;
	}

	if ((*OperationValue)->Type == EJson::String)
	{
		OutOperationKind = (*OperationValue)->AsString();
		OutOperationObject = Fixture;
		OutReason.Reset();
		return true;
	}

	if ((*OperationValue)->Type == EJson::Object)
	{
		OutOperationObject = (*OperationValue)->AsObject();
		if (!OutOperationObject.IsValid() || !OutOperationObject->TryGetStringField(TEXT("kind"), OutOperationKind))
		{
			OutReason = TEXT("missing_operation_kind");
			return false;
		}

		OutReason.Reset();
		return true;
	}

	OutReason = TEXT("malformed_operation");
	return false;
}

FWBApplyActionResult MakeFixtureFailure(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

bool ParseFixtureTile(const TSharedPtr<FJsonObject>& Object, FWBTile& OutTile)
{
	return TryReadIntegerField(Object, TEXT("x"), OutTile.X)
		&& TryReadIntegerField(Object, TEXT("y"), OutTile.Y);
}

void ApplyFixtureStatuses(FWBGameStateData& State, const TSharedPtr<FJsonObject>& InitialStateObject)
{
	const TSharedPtr<FJsonObject>* StatusesByUnit = nullptr;
	if (!InitialStateObject.IsValid()
		|| !InitialStateObject->TryGetObjectField(TEXT("statuses_by_unit"), StatusesByUnit)
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
			FString StatusId = StatusPair.Key;
			int32 TurnsRemaining = 0;
			const TSharedPtr<FJsonObject> StatusObject = StatusPair.Value.IsValid()
				? StatusPair.Value->AsObject()
				: nullptr;
			if (StatusObject.IsValid())
			{
				StatusObject->TryGetStringField(TEXT("status_id"), StatusId);
				TurnsRemaining = ReadIntegerFieldOrDefault(
					StatusObject,
					TEXT("turns_remaining"),
					ReadIntegerFieldOrDefault(StatusObject, TEXT("ticks_remaining"), 0));
			}
			else if (StatusPair.Value.IsValid() && StatusPair.Value->Type == EJson::Number)
			{
				TurnsRemaining = static_cast<int32>(StatusPair.Value->AsNumber());
			}

			Unit->AddStatus(FName(*StatusId), TurnsRemaining);
		}
	}
}

bool ApplyFixturePlayers(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("players"), Players) || Players == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
	{
		const TSharedPtr<FJsonObject> PlayerObject = PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr;
		if (!PlayerObject.IsValid())
		{
			OutReason = TEXT("malformed_initial_state_player");
			return false;
		}

		FWBPlayerStateData Player;
		if (!TryReadIntegerField(PlayerObject, TEXT("player_id"), Player.PlayerId))
		{
			OutReason = TEXT("missing_initial_state_player_id");
			return false;
		}

		Player.RemainingMP = ReadIntegerFieldOrDefault(PlayerObject, TEXT("remaining_mp"), 0);
		Player.LastMPRoll = ReadIntegerFieldOrDefault(PlayerObject, TEXT("last_mp_roll"), 0);
		Player.WallsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("walls_left"), 0);
		Player.WallRemovalsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("wall_removals_left"), 0);
		OutState.Players.Add(Player);
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixtureUnits(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
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
			OutReason = TEXT("malformed_initial_state_unit");
			return false;
		}

		FWBUnitState Unit;
		if (!TryReadIntegerField(UnitObject, TEXT("id"), Unit.UnitId)
			|| !TryReadIntegerField(UnitObject, TEXT("owner_id"), Unit.OwnerId)
			|| !TryReadIntegerField(UnitObject, TEXT("x"), Unit.X)
			|| !TryReadIntegerField(UnitObject, TEXT("y"), Unit.Y))
		{
			OutReason = TEXT("missing_initial_state_unit_required_field");
			return false;
		}

		UnitObject->TryGetStringField(TEXT("card_id"), Unit.CardId);
		Unit.HP = ReadIntegerFieldOrDefault(UnitObject, TEXT("hp"), Unit.HP);
		Unit.MaxHP = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_hp"), Unit.MaxHP);
		Unit.AttacksLeft = ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), Unit.AttacksLeft);
		Unit.MaxAttacksPerTurn = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_attacks_per_turn"), Unit.MaxAttacksPerTurn);
		Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 0);

		if (!OutState.AddUnitForTest(Unit))
		{
			OutReason = FString::Printf(TEXT("could_not_add_initial_state_unit_%d"), Unit.UnitId);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixtureWalls(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Walls = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("walls"), Walls) || Walls == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& WallValue : *Walls)
	{
		const TSharedPtr<FJsonObject> WallObject = WallValue.IsValid() ? WallValue->AsObject() : nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Between = nullptr;
		if (!WallObject.IsValid()
			|| !WallObject->TryGetArrayField(TEXT("between"), Between)
			|| Between == nullptr
			|| Between->Num() != 2)
		{
			OutReason = TEXT("malformed_initial_state_wall");
			return false;
		}

		FWBTile A;
		FWBTile B;
		if (!ParseFixtureTile((*Between)[0]->AsObject(), A) || !ParseFixtureTile((*Between)[1]->AsObject(), B))
		{
			OutReason = TEXT("malformed_initial_state_wall_tile");
			return false;
		}

		OutState.AddWallForTest(FWBWallEdge(A, B));
	}

	OutReason.Reset();
	return true;
}

bool ReadExpectedFinalIntegerField(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const TCHAR* FieldName,
	const TCHAR* FinalStateFieldName,
	int32& OutValue)
{
	if (TryReadIntegerField(ExpectedObject, FieldName, OutValue))
	{
		return true;
	}

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (ExpectedObject.IsValid()
		&& ExpectedObject->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		&& FinalStateObject != nullptr
		&& FinalStateObject->IsValid())
	{
		return TryReadIntegerField(*FinalStateObject, FinalStateFieldName, OutValue);
	}

	return false;
}

bool ReadExpectedFinalPhase(const TSharedPtr<FJsonObject>& ExpectedObject, EWBGamePhase& OutPhase)
{
	FString PhaseString;
	if (ExpectedObject.IsValid() && ExpectedObject->TryGetStringField(TEXT("final_phase"), PhaseString))
	{
		OutPhase = PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
		return true;
	}

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (ExpectedObject.IsValid()
		&& ExpectedObject->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		&& FinalStateObject != nullptr
		&& FinalStateObject->IsValid()
		&& (*FinalStateObject)->TryGetStringField(TEXT("phase"), PhaseString))
	{
		OutPhase = PhaseString == TEXT("response") ? EWBGamePhase::Response : EWBGamePhase::NormalTurn;
		return true;
	}

	return false;
}

bool TraceEventMatchesLabel(const FWBTraceEvent& Event, const FString& ExpectedLabel)
{
	FString NormalizedLabel = ExpectedLabel;
	NormalizedLabel.ReplaceInline(TEXT(" "), TEXT(":"));

	FString ExpectedKind = NormalizedLabel;
	FString ExpectedStatusId;
	if (NormalizedLabel.Split(TEXT(":"), &ExpectedKind, &ExpectedStatusId))
	{
		return Event.Kind.ToString() == ExpectedKind && Event.StatusId.ToString() == ExpectedStatusId;
	}

	return Event.Kind.ToString() == ExpectedKind;
}
}

bool BuildGameStateFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& OutState,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* InitialStateObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("initial_state"), InitialStateObject)
		|| InitialStateObject == nullptr
		|| !InitialStateObject->IsValid())
	{
		OutReason = TEXT("missing_initial_state");
		return false;
	}

	OutState = FWBGameStateData();

	int32 CurrentPlayer = 0;
	if (!TryReadIntegerField(*InitialStateObject, TEXT("current_player"), CurrentPlayer))
	{
		TryReadIntegerField(*InitialStateObject, TEXT("active_player"), CurrentPlayer);
	}

	OutState.CurrentPlayer = CurrentPlayer;
	OutState.PriorityPlayer = ReadIntegerFieldOrDefault(*InitialStateObject, TEXT("priority_player"), CurrentPlayer);
	OutState.TurnNumber = ReadIntegerFieldOrDefault(*InitialStateObject, TEXT("turn_number"), 1);
	OutState.Phase = ReadPhaseOrDefault(*InitialStateObject, EWBGamePhase::NormalTurn);
	OutState.bGameOver = ReadBoolFieldOrDefault(*InitialStateObject, TEXT("game_over"), false);

	if (!ApplyFixturePlayers(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureUnits(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureWalls(*InitialStateObject, OutState, OutReason))
	{
		return false;
	}

	ApplyFixtureStatuses(OutState, *InitialStateObject);
	OutReason.Reset();
	return true;
}

bool ParseTurnCommandFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBTurnCommand& OutCommand,
	FString& OutReason)
{
	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		return false;
	}

	if (OperationKind != TEXT("apply_turn_command"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_turn_command");
		return false;
	}

	FString Mode;
	if (!OperationObject->TryGetStringField(TEXT("mode"), Mode))
	{
		OutReason = TEXT("missing_turn_command_mode");
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
		OutReason = FString::Printf(TEXT("unsupported_turn_command_mode:%s"), *Mode);
		return false;
	}

	if (!TryReadIntegerField(OperationObject, TEXT("acting_player_id"), OutCommand.ActingPlayerId))
	{
		OutReason = TEXT("missing_turn_command_acting_player_id");
		return false;
	}

	OutCommand.NextPlayerExplicitMPRoll = ReadIntegerFieldOrDefault(
		OperationObject,
		TEXT("next_player_explicit_mp_roll"),
		0);

	if (OutCommand.Mode == EWBTurnCommandMode::DeterministicFullTransition
		&& !OperationObject->HasTypedField<EJson::Number>(TEXT("next_player_explicit_mp_roll")))
	{
		OutReason = TEXT("missing_turn_command_next_player_explicit_mp_roll");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ApplyTurnCommandFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	FString& OutReason)
{
	FWBTurnCommand Command;
	if (!ParseTurnCommandFromFixture(Fixture, Command, OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	OutResult = WBTurnController::ApplyTurnCommand(State, Command);
	OutReason.Reset();
	return true;
}

bool ApplyFixtureOperation(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	EWBFixtureOperationKind& OutOperationKind,
	FString& OutReason)
{
	OutOperationKind = EWBFixtureOperationKind::Unknown;

	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	if (OperationKind == TEXT("apply_turn_command"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyTurnCommand;
		return ApplyTurnCommandFixture(Fixture, State, OutResult, OutReason);
	}

	if (OperationKind == TEXT("apply_action"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyAction;
		const TSharedPtr<FJsonObject>* ActionObject = nullptr;
		if (!Fixture.IsValid()
			|| !Fixture->TryGetObjectField(TEXT("action"), ActionObject)
			|| ActionObject == nullptr
			|| !ActionObject->IsValid())
		{
			OutReason = TEXT("missing_action");
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializeJsonObject(*ActionObject), State);
		if (!DecodeResult.bOk)
		{
			OutReason = DecodeResult.Reason;
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyAction(State, DecodeResult.Action);
		OutReason.Reset();
		return true;
	}

	OutReason = FString::Printf(TEXT("unsupported_fixture_operation:%s"), *OperationKind);
	OutResult = MakeFixtureFailure(OutReason);
	return false;
}

bool ExpectTraceOrder(
	const TSharedPtr<FJsonObject>& Fixture,
	const TArray<FWBTraceEvent>& TraceEvents,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* TraceOrderValues = nullptr;
	if (!ExpectedObject->TryGetArrayField(TEXT("expected_trace_order"), TraceOrderValues) || TraceOrderValues == nullptr)
	{
		OutReason = TEXT("missing_expected_trace_order");
		return false;
	}

	TArray<FString> ExpectedTraceOrder;
	if (!ReadStringArrayValues(*TraceOrderValues, TEXT("expected.expected_trace_order"), ExpectedTraceOrder, OutReason))
	{
		return false;
	}

	if (TraceEvents.Num() != ExpectedTraceOrder.Num())
	{
		OutReason = FString::Printf(TEXT("trace_count_expected_%d_got_%d"), ExpectedTraceOrder.Num(), TraceEvents.Num());
		return false;
	}

	for (int32 Index = 0; Index < ExpectedTraceOrder.Num(); ++Index)
	{
		if (!TraceEventMatchesLabel(TraceEvents[Index], ExpectedTraceOrder[Index]))
		{
			OutReason = FString::Printf(
				TEXT("trace_order_%d_expected_%s_got_%s:%s"),
				Index,
				*ExpectedTraceOrder[Index],
				*TraceEvents[Index].Kind.ToString(),
				*TraceEvents[Index].StatusId.ToString());
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectPlayerResources(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PlayerResourcesObject = nullptr;
	if (!ExpectedObject->TryGetObjectField(TEXT("player_resources"), PlayerResourcesObject)
		|| PlayerResourcesObject == nullptr
		|| !PlayerResourcesObject->IsValid())
	{
		OutReason = TEXT("missing_expected_player_resources");
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& PlayerResourcePair : (*PlayerResourcesObject)->Values)
	{
		const int32 PlayerId = FCString::Atoi(*PlayerResourcePair.Key);
		const TSharedPtr<FJsonObject> PlayerResourceObject = PlayerResourcePair.Value.IsValid()
			? PlayerResourcePair.Value->AsObject()
			: nullptr;
		const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
		if (Player == nullptr || !PlayerResourceObject.IsValid())
		{
			OutReason = FString::Printf(TEXT("missing_expected_player_resource_%d"), PlayerId);
			return false;
		}

		if (Player->RemainingMP != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("remaining_mp"), Player->RemainingMP)
			|| Player->LastMPRoll != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("last_mp_roll"), Player->LastMPRoll)
			|| Player->WallsLeft != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("walls_left"), Player->WallsLeft)
			|| Player->WallRemovalsLeft != ReadIntegerFieldOrDefault(PlayerResourceObject, TEXT("wall_removals_left"), Player->WallRemovalsLeft))
		{
			OutReason = FString::Printf(TEXT("player_%d_resource_mismatch"), PlayerId);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectUnitStatusSummary(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* FinalUnits = nullptr;
	if (!ExpectedObject->TryGetArrayField(TEXT("final_units"), FinalUnits) || FinalUnits == nullptr)
	{
		OutReason = TEXT("missing_expected_final_units");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FinalUnitValue : *FinalUnits)
	{
		const TSharedPtr<FJsonObject> FinalUnitObject = FinalUnitValue.IsValid() ? FinalUnitValue->AsObject() : nullptr;
		int32 UnitId = -1;
		if (!FinalUnitObject.IsValid() || !TryReadIntegerField(FinalUnitObject, TEXT("id"), UnitId))
		{
			OutReason = TEXT("malformed_expected_final_unit");
			return false;
		}

		const FWBUnitState* Unit = State.GetUnitById(UnitId);
		if (Unit == nullptr)
		{
			OutReason = FString::Printf(TEXT("missing_unit_%d"), UnitId);
			return false;
		}

		if (Unit->X != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("x"), Unit->X)
			|| Unit->Y != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("y"), Unit->Y)
			|| Unit->HP != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("hp"), Unit->HP)
			|| Unit->MaxHP != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("max_hp"), Unit->MaxHP)
			|| Unit->AttacksLeft != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("attacks_left"), Unit->AttacksLeft))
		{
			OutReason = FString::Printf(TEXT("unit_%d_state_mismatch"), UnitId);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Statuses = nullptr;
		if (FinalUnitObject->TryGetArrayField(TEXT("statuses"), Statuses) && Statuses != nullptr)
		{
			if (Unit->Statuses.Num() != Statuses->Num())
			{
				OutReason = FString::Printf(TEXT("unit_%d_status_count_mismatch"), UnitId);
				return false;
			}

			for (const TSharedPtr<FJsonValue>& StatusValue : *Statuses)
			{
				if (!StatusValue.IsValid()
					|| StatusValue->Type != EJson::String
					|| !Unit->HasStatus(FName(*StatusValue->AsString())))
				{
					OutReason = FString::Printf(TEXT("unit_%d_status_mismatch"), UnitId);
					return false;
				}
			}
		}

		const TSharedPtr<FJsonObject>* StatusTurns = nullptr;
		if (FinalUnitObject->TryGetObjectField(TEXT("status_turns"), StatusTurns)
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

	OutReason.Reset();
	return true;
}

bool ExpectFinalTurnState(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	int32 ExpectedCurrentPlayer = State.CurrentPlayer;
	int32 ExpectedPriorityPlayer = State.PriorityPlayer;
	int32 ExpectedTurnNumber = State.TurnNumber;
	EWBGamePhase ExpectedPhase = State.Phase;

	if (ReadExpectedFinalIntegerField(ExpectedObject, TEXT("final_current_player"), TEXT("current_player"), ExpectedCurrentPlayer)
		&& State.CurrentPlayer != ExpectedCurrentPlayer)
	{
		OutReason = TEXT("final_current_player_mismatch");
		return false;
	}

	if (ReadExpectedFinalIntegerField(ExpectedObject, TEXT("final_priority_player"), TEXT("priority_player"), ExpectedPriorityPlayer)
		&& State.PriorityPlayer != ExpectedPriorityPlayer)
	{
		OutReason = TEXT("final_priority_player_mismatch");
		return false;
	}

	if (ReadExpectedFinalIntegerField(ExpectedObject, TEXT("final_turn_number"), TEXT("turn_number"), ExpectedTurnNumber)
		&& State.TurnNumber != ExpectedTurnNumber)
	{
		OutReason = TEXT("final_turn_number_mismatch");
		return false;
	}

	if (ReadExpectedFinalPhase(ExpectedObject, ExpectedPhase) && State.Phase != ExpectedPhase)
	{
		OutReason = TEXT("final_phase_mismatch");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ExtractReplayLogJson(const TSharedPtr<FJsonObject>& Fixture, FString& OutReplayLogJson)
{
	TSharedPtr<FJsonObject> ReplayLogObject;
	FString Reason;
	if (!TryGetReplayLogObject(Fixture, ReplayLogObject, Reason))
	{
		return false;
	}

	OutReplayLogJson = SerializeJsonObject(ReplayLogObject);
	return !OutReplayLogJson.IsEmpty();
}

bool TryGetReplayDecisionObject(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	TSharedPtr<FJsonObject>& OutDecisionObject,
	FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!TryGetReplayDecisions(Fixture, Decisions, OutReason))
	{
		return false;
	}

	if (!Decisions->IsValidIndex(DecisionIndex))
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d"), DecisionIndex);
		return false;
	}

	OutDecisionObject = (*Decisions)[DecisionIndex].IsValid() ? (*Decisions)[DecisionIndex]->AsObject() : nullptr;
	if (!OutDecisionObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_replay_log_decision_%d"), DecisionIndex);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ReadReplayDecisionCount(
	const TSharedPtr<FJsonObject>& Fixture,
	int32& OutDecisionCount,
	FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Decisions = nullptr;
	if (!TryGetReplayDecisions(Fixture, Decisions, OutReason))
	{
		OutDecisionCount = 0;
		return false;
	}

	OutDecisionCount = Decisions->Num();
	return true;
}

bool RemoveReplayLogField(
	const TSharedPtr<FJsonObject>& Fixture,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ReplayLogObject;
	if (!TryGetReplayLogObject(Fixture, ReplayLogObject, OutReason))
	{
		return false;
	}

	return RemoveObjectFieldAndExtractReplayLog(Fixture, ReplayLogObject, TEXT("replay_log"), FieldName, OutReplayLogJson, OutReason);
}

bool RemoveReplayDecisionField(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	return RemoveObjectFieldAndExtractReplayLog(
		Fixture,
		DecisionObject,
		FString::Printf(TEXT("replay_log_decision_%d"), DecisionIndex),
		FieldName,
		OutReplayLogJson,
		OutReason);
}

bool RemoveReplayDecisionChosenActionField(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	const TCHAR* FieldName,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ChosenActionObject = nullptr;
	if (!DecisionObject->TryGetObjectField(TEXT("chosen_action"), ChosenActionObject)
		|| ChosenActionObject == nullptr
		|| !ChosenActionObject->IsValid())
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_chosen_action"), DecisionIndex);
		return false;
	}

	return RemoveObjectFieldAndExtractReplayLog(
		Fixture,
		*ChosenActionObject,
		FString::Printf(TEXT("replay_log_decision_%d_chosen_action"), DecisionIndex),
		FieldName,
		OutReplayLogJson,
		OutReason);
}

bool ReadReplayDecisionLegalActionIds(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	TArray<FString>& OutActionIds,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ActionIdValues = nullptr;
	if (!DecisionObject->TryGetArrayField(TEXT("legal_action_ids"), ActionIdValues) || ActionIdValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_legal_action_ids"), DecisionIndex);
		return false;
	}

	return ReadStringArrayValues(
		*ActionIdValues,
		FString::Printf(TEXT("replay_log_decision_%d_legal_action_ids"), DecisionIndex),
		OutActionIds,
		OutReason);
}

bool AppendTamperedReplayDecisionLegalActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ExistingActionIdValues = nullptr;
	if (!DecisionObject->TryGetArrayField(TEXT("legal_action_ids"), ExistingActionIdValues) || ExistingActionIdValues == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_legal_action_ids"), DecisionIndex);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>> OriginalActionIdValues = *ExistingActionIdValues;
	TArray<TSharedPtr<FJsonValue>> TamperedActionIdValues = OriginalActionIdValues;
	TamperedActionIdValues.Add(MakeShared<FJsonValueString>(TEXT("tampered:legal_action_ids")));
	DecisionObject->SetArrayField(TEXT("legal_action_ids"), TamperedActionIdValues);

	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		DecisionObject->SetArrayField(TEXT("legal_action_ids"), OriginalActionIdValues);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	DecisionObject->SetArrayField(TEXT("legal_action_ids"), OriginalActionIdValues);
	OutReason.Reset();
	return true;
}

bool TamperReplayDecisionChosenActionId(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	FString OriginalChosenActionId;
	if (!DecisionObject->TryGetStringField(TEXT("chosen_action_id"), OriginalChosenActionId))
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_chosen_action_id"), DecisionIndex);
		return false;
	}

	DecisionObject->SetStringField(TEXT("chosen_action_id"), OriginalChosenActionId + TEXT(":tampered"));
	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		DecisionObject->SetStringField(TEXT("chosen_action_id"), OriginalChosenActionId);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	DecisionObject->SetStringField(TEXT("chosen_action_id"), OriginalChosenActionId);
	OutReason.Reset();
	return true;
}

bool TamperReplayDecisionTraceOk(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 DecisionIndex,
	FString& OutReplayLogJson,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> DecisionObject;
	if (!TryGetReplayDecisionObject(Fixture, DecisionIndex, DecisionObject, OutReason))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* TraceEventValues = nullptr;
	if (!DecisionObject->TryGetArrayField(TEXT("trace_events"), TraceEventValues)
		|| TraceEventValues == nullptr
		|| TraceEventValues->Num() == 0)
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_trace_events"), DecisionIndex);
		return false;
	}

	const TSharedPtr<FJsonObject> TraceEventObject = (*TraceEventValues)[0].IsValid()
		? (*TraceEventValues)[0]->AsObject()
		: nullptr;
	if (!TraceEventObject.IsValid())
	{
		OutReason = FString::Printf(TEXT("malformed_replay_log_decision_%d_trace_events[0]"), DecisionIndex);
		return false;
	}

	bool bOriginalOk = false;
	if (!TraceEventObject->TryGetBoolField(TEXT("ok"), bOriginalOk))
	{
		OutReason = FString::Printf(TEXT("missing_replay_log_decision_%d_trace_events[0]_ok"), DecisionIndex);
		return false;
	}

	TraceEventObject->SetBoolField(TEXT("ok"), !bOriginalOk);
	if (!ExtractReplayLogJson(Fixture, OutReplayLogJson))
	{
		TraceEventObject->SetBoolField(TEXT("ok"), bOriginalOk);
		OutReason = TEXT("missing_replay_log");
		return false;
	}

	TraceEventObject->SetBoolField(TEXT("ok"), bOriginalOk);
	OutReason.Reset();
	return true;
}
}
