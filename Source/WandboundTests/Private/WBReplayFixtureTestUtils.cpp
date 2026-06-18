#include "WBReplayFixtureTestUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBActionCodec.h"
#include "WBArmorEffect.h"
#include "WBDamageResolution.h"
#include "WBEffectRunner.h"
#include "WBMPRollSource.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"

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

bool ReadOptionalStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	TArray<FString>& OutStrings,
	FString& OutReason)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->HasField(FieldName))
	{
		OutReason.Reset();
		return true;
	}

	if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
	{
		OutReason = FString::Printf(TEXT("malformed_%s"), *FieldPath);
		return false;
	}

	return ReadStringArrayValues(*Values, FieldPath, OutStrings, OutReason);
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

bool ParseActionFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FWBAction& OutAction,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ActionObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("action"), ActionObject)
		|| ActionObject == nullptr
		|| !ActionObject->IsValid())
	{
		OutReason = TEXT("missing_action");
		return false;
	}

	TSharedPtr<FJsonObject> NormalizedActionObject = MakeShared<FJsonObject>();
	NormalizedActionObject->Values = (*ActionObject)->Values;

	FString Type;
	if (!NormalizedActionObject->HasField(TEXT("kind")) && NormalizedActionObject->TryGetStringField(TEXT("type"), Type))
	{
		NormalizedActionObject->SetStringField(TEXT("kind"), Type);
	}

	int32 SourceUnitId = -1;
	if (!NormalizedActionObject->HasField(TEXT("unit_id"))
		&& TryReadIntegerField(NormalizedActionObject, TEXT("source_unit_id"), SourceUnitId))
	{
		NormalizedActionObject->SetNumberField(TEXT("unit_id"), SourceUnitId);
	}

	const FWBActionDecodeResult DecodeResult = WBActionCodec::DecodeAction(SerializeJsonObject(NormalizedActionObject), State);
	if (!DecodeResult.bOk)
	{
		OutReason = DecodeResult.Reason;
		return false;
	}

	OutAction = DecodeResult.Action;
	OutReason.Reset();
	return true;
}

bool ParseRuntimeRollSource(
	const TSharedPtr<FJsonObject>& RuntimeContextObject,
	TUniquePtr<FWBFixedMPRollSource>& OutFixedRollSource,
	TUniquePtr<FWBQueuedMPRollSource>& OutQueuedRollSource,
	IWBMPRollSource*& OutRollSource,
	FString& OutReason)
{
	OutFixedRollSource.Reset();
	OutQueuedRollSource.Reset();
	OutRollSource = nullptr;

	const bool bHasFixedRoll = RuntimeContextObject.IsValid()
		&& RuntimeContextObject->HasTypedField<EJson::Number>(TEXT("fixed_mp_roll"));
	const bool bHasQueuedRolls = RuntimeContextObject.IsValid()
		&& RuntimeContextObject->HasTypedField<EJson::Array>(TEXT("queued_mp_rolls"));

	if (bHasFixedRoll && bHasQueuedRolls)
	{
		OutReason = TEXT("multiple_mp_roll_sources");
		return false;
	}

	if (bHasFixedRoll)
	{
		int32 FixedRoll = 0;
		if (!TryReadIntegerField(RuntimeContextObject, TEXT("fixed_mp_roll"), FixedRoll))
		{
			OutReason = TEXT("malformed_fixed_mp_roll");
			return false;
		}

		OutFixedRollSource = MakeUnique<FWBFixedMPRollSource>(FixedRoll);
		OutRollSource = OutFixedRollSource.Get();
		OutReason.Reset();
		return true;
	}

	if (bHasQueuedRolls)
	{
		const TArray<TSharedPtr<FJsonValue>>* QueuedRollValues = nullptr;
		if (!RuntimeContextObject->TryGetArrayField(TEXT("queued_mp_rolls"), QueuedRollValues)
			|| QueuedRollValues == nullptr)
		{
			OutReason = TEXT("malformed_queued_mp_rolls");
			return false;
		}

		OutQueuedRollSource = MakeUnique<FWBQueuedMPRollSource>();
		for (int32 Index = 0; Index < QueuedRollValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& RollValue = (*QueuedRollValues)[Index];
			if (!RollValue.IsValid() || RollValue->Type != EJson::Number)
			{
				OutReason = FString::Printf(TEXT("malformed_queued_mp_rolls[%d]"), Index);
				return false;
			}

			OutQueuedRollSource->EnqueueRoll(static_cast<int32>(RollValue->AsNumber()));
		}

		OutRollSource = OutQueuedRollSource.Get();
		OutReason.Reset();
		return true;
	}

	OutReason.Reset();
	return true;
}

bool ParseRuntimeSelectedActionFixtureInputs(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBGameStateData& State,
	FWBAction& OutSelectedAction,
	FWBRuntimeTurnResolutionContext& OutRuntimeContext,
	TUniquePtr<FWBFixedMPRollSource>& OutFixedRollSource,
	TUniquePtr<FWBQueuedMPRollSource>& OutQueuedRollSource,
	FString& OutReason)
{
	if (!ParseActionFromFixture(Fixture, State, OutSelectedAction, OutReason))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* RuntimeContextObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("runtime_context"), RuntimeContextObject)
		|| RuntimeContextObject == nullptr
		|| !RuntimeContextObject->IsValid())
	{
		OutReason = TEXT("missing_runtime_context");
		return false;
	}

	if (!(*RuntimeContextObject)->TryGetBoolField(
		TEXT("resolve_end_turn_as_full_transition"),
		OutRuntimeContext.bResolveEndTurnAsFullTransition))
	{
		OutReason = TEXT("missing_runtime_context_resolve_end_turn_as_full_transition");
		return false;
	}

	IWBMPRollSource* RollSource = nullptr;
	if (!ParseRuntimeRollSource(*RuntimeContextObject, OutFixedRollSource, OutQueuedRollSource, RollSource, OutReason))
	{
		return false;
	}

	OutRuntimeContext.MPRollSource = RollSource;
	OutReason.Reset();
	return true;
}

bool ExpectPublicPlayerSummary(
	const TSharedPtr<FJsonObject>& PlayerSummaryObject,
	const FWBPublicTurnSummary& Summary,
	FString& OutReason)
{
	int32 PlayerId = -1;
	if (!PlayerSummaryObject.IsValid() || !TryReadIntegerField(PlayerSummaryObject, TEXT("player_id"), PlayerId))
	{
		OutReason = TEXT("malformed_final_public_turn_summary_player");
		return false;
	}

	const FWBPublicPlayerTurnSummary* ActualPlayerSummary = nullptr;
	for (const FWBPublicPlayerTurnSummary& Candidate : Summary.Players)
	{
		if (Candidate.PlayerId == PlayerId)
		{
			ActualPlayerSummary = &Candidate;
			break;
		}
	}

	if (ActualPlayerSummary == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_final_public_turn_summary_player_%d"), PlayerId);
		return false;
	}

	if (ActualPlayerSummary->RemainingMP != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("remaining_mp"), ActualPlayerSummary->RemainingMP)
		|| ActualPlayerSummary->LastMPRoll != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("last_mp_roll"), ActualPlayerSummary->LastMPRoll)
		|| ActualPlayerSummary->WallsLeft != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("walls_left"), ActualPlayerSummary->WallsLeft)
		|| ActualPlayerSummary->WallRemovalsLeft != ReadIntegerFieldOrDefault(PlayerSummaryObject, TEXT("wall_removals_left"), ActualPlayerSummary->WallRemovalsLeft))
	{
		OutReason = FString::Printf(TEXT("final_public_turn_summary_player_%d_mismatch"), PlayerId);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ExpectFinalPublicTurnSummary(
	const TSharedPtr<FJsonObject>& ExpectedObject,
	const FWBPublicTurnSummary& Summary,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* SummaryObject = nullptr;
	if (!ExpectedObject.IsValid()
		|| !ExpectedObject->TryGetObjectField(TEXT("final_public_turn_summary"), SummaryObject)
		|| SummaryObject == nullptr
		|| !SummaryObject->IsValid())
	{
		OutReason = TEXT("missing_expected_final_public_turn_summary");
		return false;
	}

	if (Summary.CurrentPlayerId != ReadIntegerFieldOrDefault(*SummaryObject, TEXT("current_player_id"), Summary.CurrentPlayerId)
		|| Summary.PriorityPlayerId != ReadIntegerFieldOrDefault(*SummaryObject, TEXT("priority_player_id"), Summary.PriorityPlayerId)
		|| Summary.TurnNumber != ReadIntegerFieldOrDefault(*SummaryObject, TEXT("turn_number"), Summary.TurnNumber))
	{
		OutReason = TEXT("final_public_turn_summary_turn_state_mismatch");
		return false;
	}

	bool bExpectedGameOver = Summary.bGameOver;
	(*SummaryObject)->TryGetBoolField(TEXT("game_over"), bExpectedGameOver);
	if (Summary.bGameOver != bExpectedGameOver)
	{
		OutReason = TEXT("final_public_turn_summary_game_over_mismatch");
		return false;
	}

	const int32 ExpectedWinnerPlayerId = ReadIntegerFieldOrDefault(*SummaryObject, TEXT("winner_player_id"), Summary.WinnerPlayerId);
	if (Summary.WinnerPlayerId != ExpectedWinnerPlayerId)
	{
		OutReason = TEXT("final_public_turn_summary_winner_mismatch");
		return false;
	}

	FString ExpectedPhase;
	if ((*SummaryObject)->TryGetStringField(TEXT("phase"), ExpectedPhase) && Summary.Phase.ToString() != ExpectedPhase)
	{
		OutReason = TEXT("final_public_turn_summary_phase_mismatch");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
	if ((*SummaryObject)->TryGetArrayField(TEXT("players"), Players) && Players != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& PlayerValue : *Players)
		{
			if (!ExpectPublicPlayerSummary(PlayerValue.IsValid() ? PlayerValue->AsObject() : nullptr, Summary, OutReason))
			{
				return false;
			}
		}
	}

	OutReason.Reset();
	return true;
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
		Player.HeroUnitId = ReadIntegerFieldOrDefault(PlayerObject, TEXT("hero_unit_id"), Player.HeroUnitId);
		Player.WallsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("walls_left"), 0);
		Player.WallRemovalsLeft = ReadIntegerFieldOrDefault(PlayerObject, TEXT("wall_removals_left"), 0);
		if (!ReadOptionalStringArrayField(
				PlayerObject,
				TEXT("deck"),
				FString::Printf(TEXT("initial_state.players[%d].deck"), Player.PlayerId),
				Player.Deck,
				OutReason)
			|| !ReadOptionalStringArrayField(
				PlayerObject,
				TEXT("hand"),
				FString::Printf(TEXT("initial_state.players[%d].hand"), Player.PlayerId),
				Player.Hand,
				OutReason)
			|| !ReadOptionalStringArrayField(
				PlayerObject,
				TEXT("discard"),
				FString::Printf(TEXT("initial_state.players[%d].discard"), Player.PlayerId),
				Player.Discard,
				OutReason))
		{
			return false;
		}
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
		Unit.CurrentArmor = ReadIntegerFieldOrDefault(UnitObject, TEXT("current_armor"), Unit.CurrentArmor);
		Unit.MaxArmor = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_armor"), Unit.MaxArmor);
		Unit.ATK = ReadIntegerFieldOrDefault(UnitObject, TEXT("atk"), Unit.ATK);
		Unit.AR = ReadIntegerFieldOrDefault(UnitObject, TEXT("ar"), Unit.AR);
		Unit.RLTotal = ReadIntegerFieldOrDefault(UnitObject, TEXT("rl_total"), Unit.RLTotal);
		Unit.RLUsed = ReadIntegerFieldOrDefault(UnitObject, TEXT("rl_used"), Unit.RLUsed);
		Unit.AttacksLeft = ReadIntegerFieldOrDefault(UnitObject, TEXT("attacks_left"), Unit.AttacksLeft);
		Unit.MaxAttacksPerTurn = ReadIntegerFieldOrDefault(UnitObject, TEXT("max_attacks_per_turn"), Unit.MaxAttacksPerTurn);
		Unit.MPRemaining = ReadIntegerFieldOrDefault(UnitObject, TEXT("mp_remaining"), 0);
		Unit.bDefeated = ReadBoolFieldOrDefault(UnitObject, TEXT("defeated"), false);
		Unit.bRemovedFromBoard = ReadBoolFieldOrDefault(UnitObject, TEXT("removed_from_board"), false);

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

bool ApplyFixtureTerrain(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	FString DefaultTerrainId;
	if (StateObject->TryGetStringField(TEXT("default_terrain_id"), DefaultTerrainId) && !DefaultTerrainId.IsEmpty())
	{
		OutState.DefaultTerrainId = FName(*DefaultTerrainId);
	}

	const TArray<TSharedPtr<FJsonValue>>* TerrainTiles = nullptr;
	if (!StateObject->TryGetArrayField(TEXT("terrain_tiles"), TerrainTiles) || TerrainTiles == nullptr)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& TerrainValue : *TerrainTiles)
	{
		const TSharedPtr<FJsonObject> TerrainObject = TerrainValue.IsValid() ? TerrainValue->AsObject() : nullptr;
		if (!TerrainObject.IsValid())
		{
			OutReason = TEXT("malformed_initial_state_terrain_tile");
			return false;
		}

		FWBTile Tile;
		FString TerrainId;
		if (!ParseFixtureTile(TerrainObject, Tile) || !TerrainObject->TryGetStringField(TEXT("terrain_id"), TerrainId) || TerrainId.IsEmpty())
		{
			OutReason = TEXT("malformed_initial_state_terrain_tile");
			return false;
		}

		OutState.SetTerrainForTest(Tile, FName(*TerrainId));
	}

	OutReason.Reset();
	return true;
}

bool ApplyFixturePendingAttack(const TSharedPtr<FJsonObject>& StateObject, FWBGameStateData& OutState, FString& OutReason)
{
	const TSharedPtr<FJsonObject>* PendingAttackObject = nullptr;
	if (!StateObject->TryGetObjectField(TEXT("pending_attack"), PendingAttackObject)
		|| PendingAttackObject == nullptr
		|| !PendingAttackObject->IsValid())
	{
		return true;
	}

	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = ReadBoolFieldOrDefault(*PendingAttackObject, TEXT("active"), false);
	if (!PendingAttack.bActive)
	{
		OutState.ClearPendingAttack();
		OutReason.Reset();
		return true;
	}

	const TSharedPtr<FJsonObject>* AttackerTileObject = nullptr;
	const TSharedPtr<FJsonObject>* DefenderTileObject = nullptr;
	if (!TryReadIntegerField(*PendingAttackObject, TEXT("attacker_unit_id"), PendingAttack.AttackerUnitId)
		|| !TryReadIntegerField(*PendingAttackObject, TEXT("defender_unit_id"), PendingAttack.DefenderUnitId)
		|| !TryReadIntegerField(*PendingAttackObject, TEXT("attacking_player_id"), PendingAttack.AttackingPlayerId)
		|| !(*PendingAttackObject)->TryGetObjectField(TEXT("attacker_tile"), AttackerTileObject)
		|| AttackerTileObject == nullptr
		|| !AttackerTileObject->IsValid()
		|| !ParseFixtureTile(*AttackerTileObject, PendingAttack.AttackerTile)
		|| !(*PendingAttackObject)->TryGetObjectField(TEXT("defender_tile"), DefenderTileObject)
		|| DefenderTileObject == nullptr
		|| !DefenderTileObject->IsValid()
		|| !ParseFixtureTile(*DefenderTileObject, PendingAttack.DefenderTile))
	{
		OutReason = TEXT("malformed_initial_state_pending_attack");
		return false;
	}

	PendingAttack.DeclarationActionId = TEXT("");
	(*PendingAttackObject)->TryGetStringField(TEXT("declaration_action_id"), PendingAttack.DeclarationActionId);
	OutState.SetPendingAttackForTest(PendingAttack);
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
	OutState.WinnerPlayerId = ReadIntegerFieldOrDefault(*InitialStateObject, TEXT("winner_player_id"), -1);

	if (!ApplyFixturePlayers(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureUnits(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureWalls(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixtureTerrain(*InitialStateObject, OutState, OutReason)
		|| !ApplyFixturePendingAttack(*InitialStateObject, OutState, OutReason))
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

bool ApplyRuntimeSelectedActionFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBApplyActionResult& OutResult,
	int32& OutRollSourceRemainingCount,
	FString& OutReason)
{
	OutRollSourceRemainingCount = -1;

	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	if (OperationKind != TEXT("apply_runtime_selected_action"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_runtime_selected_action");
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	FWBAction SelectedAction;
	FWBRuntimeTurnResolutionContext RuntimeContext;
	TUniquePtr<FWBFixedMPRollSource> FixedRollSource;
	TUniquePtr<FWBQueuedMPRollSource> QueuedRollSource;
	if (!ParseRuntimeSelectedActionFixtureInputs(
		Fixture,
		State,
		SelectedAction,
		RuntimeContext,
		FixedRollSource,
		QueuedRollSource,
		OutReason))
	{
		OutResult = MakeFixtureFailure(OutReason);
		return false;
	}

	OutResult = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(State, SelectedAction, RuntimeContext);
	if (QueuedRollSource.IsValid())
	{
		OutRollSourceRemainingCount = QueuedRollSource->NumRemainingRolls();
	}

	OutReason.Reset();
	return true;
}

bool ApplyRuntimeSelectedActionWithResultFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	int32& OutRollSourceRemainingCount,
	FString& OutReason)
{
	OutRollSourceRemainingCount = -1;
	OutEnvelope = FWBRuntimeSelectedActionResult();

	FString OperationKind;
	TSharedPtr<FJsonObject> OperationObject;
	if (!TryGetOperationKindAndObject(Fixture, OperationKind, OperationObject, OutReason))
	{
		OutEnvelope.ApplyResult = MakeFixtureFailure(OutReason);
		return false;
	}

	if (OperationKind != TEXT("apply_runtime_selected_action_with_result")
		&& OperationKind != TEXT("apply_runtime_selected_action_with_result_and_serialize"))
	{
		OutReason = TEXT("fixture_operation_is_not_apply_runtime_selected_action_with_result");
		OutEnvelope.ApplyResult = MakeFixtureFailure(OutReason);
		return false;
	}

	FWBAction SelectedAction;
	FWBRuntimeTurnResolutionContext RuntimeContext;
	TUniquePtr<FWBFixedMPRollSource> FixedRollSource;
	TUniquePtr<FWBQueuedMPRollSource> QueuedRollSource;
	if (!ParseRuntimeSelectedActionFixtureInputs(
		Fixture,
		State,
		SelectedAction,
		RuntimeContext,
		FixedRollSource,
		QueuedRollSource,
		OutReason))
	{
		OutEnvelope.ApplyResult = MakeFixtureFailure(OutReason);
		return false;
	}

	OutEnvelope = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		SelectedAction,
		RuntimeContext);
	if (QueuedRollSource.IsValid())
	{
		OutRollSourceRemainingCount = QueuedRollSource->NumRemainingRolls();
	}

	OutReason.Reset();
	return true;
}

bool ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBGameStateData& State,
	FWBRuntimeSelectedActionResult& OutEnvelope,
	FString& OutSerializedJson,
	int32& OutRollSourceRemainingCount,
	FString& OutReason)
{
	OutSerializedJson.Reset();
	if (!ApplyRuntimeSelectedActionWithResultFixture(
			Fixture,
			State,
			OutEnvelope,
			OutRollSourceRemainingCount,
			OutReason))
	{
		return false;
	}

	if (!WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(OutEnvelope, OutSerializedJson))
	{
		OutReason = TEXT("runtime_result_serialization_failed");
		return false;
	}

	OutReason.Reset();
	return true;
}

EWBDamageKind ReadDamageKindOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString DamageKindString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("damage_kind"), DamageKindString))
	{
		return EWBDamageKind::Unknown;
	}

	if (DamageKindString == TEXT("attack"))
	{
		return EWBDamageKind::Attack;
	}

	if (DamageKindString == TEXT("burn"))
	{
		return EWBDamageKind::Burn;
	}

	if (DamageKindString == TEXT("effect"))
	{
		return EWBDamageKind::Effect;
	}

	return EWBDamageKind::Unknown;
}

EWBArmorEffectOp ReadArmorEffectOperationOrDefault(const TSharedPtr<FJsonObject>& Object)
{
	FString OperationString;
	if (!Object.IsValid() || !Object->TryGetStringField(TEXT("operation"), OperationString))
	{
		return EWBArmorEffectOp::Unknown;
	}

	if (OperationString == TEXT("add_current_armor"))
	{
		return EWBArmorEffectOp::AddCurrentArmor;
	}

	if (OperationString == TEXT("reduce_current_armor"))
	{
		return EWBArmorEffectOp::ReduceCurrentArmor;
	}

	if (OperationString == TEXT("set_current_armor"))
	{
		return EWBArmorEffectOp::SetCurrentArmor;
	}

	if (OperationString == TEXT("add_max_armor"))
	{
		return EWBArmorEffectOp::AddMaxArmor;
	}

	if (OperationString == TEXT("reduce_max_armor"))
	{
		return EWBArmorEffectOp::ReduceMaxArmor;
	}

	if (OperationString == TEXT("set_max_armor"))
	{
		return EWBArmorEffectOp::SetMaxArmor;
	}

	if (OperationString == TEXT("restore_armor_to_max"))
	{
		return EWBArmorEffectOp::RestoreArmorToMax;
	}

	return EWBArmorEffectOp::Unknown;
}

bool ParseArmorEffectRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBArmorEffectRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* ArmorRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("armor_effect_request"), ArmorRequestObject)
		|| ArmorRequestObject == nullptr
		|| !ArmorRequestObject->IsValid())
	{
		OutReason = TEXT("missing_armor_effect_request");
		return false;
	}

	OutRequest.Operation = ReadArmorEffectOperationOrDefault(*ArmorRequestObject);
	if (!TryReadIntegerField(*ArmorRequestObject, TEXT("target_unit_id"), OutRequest.TargetUnitId)
		|| !TryReadIntegerField(*ArmorRequestObject, TEXT("amount"), OutRequest.Amount))
	{
		OutReason = TEXT("malformed_armor_effect_request");
		return false;
	}

	FString SourceReason;
	if ((*ArmorRequestObject)->TryGetStringField(TEXT("source_reason"), SourceReason))
	{
		OutRequest.SourceReason = FName(*SourceReason);
	}

	OutReason.Reset();
	return true;
}

bool ParseDamageRequestFromFixture(
	const TSharedPtr<FJsonObject>& Fixture,
	FWBDamageRequest& OutRequest,
	FString& OutReason)
{
	const TSharedPtr<FJsonObject>* DamageRequestObject = nullptr;
	if (!Fixture.IsValid()
		|| !Fixture->TryGetObjectField(TEXT("damage_request"), DamageRequestObject)
		|| DamageRequestObject == nullptr
		|| !DamageRequestObject->IsValid())
	{
		OutReason = TEXT("missing_damage_request");
		return false;
	}

	OutRequest.DamageKind = ReadDamageKindOrDefault(*DamageRequestObject);
	if (!TryReadIntegerField(*DamageRequestObject, TEXT("source_unit_id"), OutRequest.SourceUnitId)
		|| !TryReadIntegerField(*DamageRequestObject, TEXT("target_unit_id"), OutRequest.TargetUnitId)
		|| !TryReadIntegerField(*DamageRequestObject, TEXT("source_player_id"), OutRequest.SourcePlayerId)
		|| !TryReadIntegerField(*DamageRequestObject, TEXT("base_damage"), OutRequest.BaseDamage))
	{
		OutReason = TEXT("malformed_damage_request");
		return false;
	}

	(*DamageRequestObject)->TryGetBoolField(TEXT("bypass_armor"), OutRequest.bBypassArmor);
	FString DamageCause;
	if ((*DamageRequestObject)->TryGetStringField(TEXT("damage_cause"), DamageCause))
	{
		OutRequest.DamageCause = FName(*DamageCause);
	}

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

	if (OperationKind == TEXT("apply_attack_declare"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyAttackDeclare;
		FWBAction Action;
		if (!ParseActionFromFixture(Fixture, State, Action, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyAttackDeclare(State, Action);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_pending_attack_damage"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyPendingAttackDamage;
		OutResult = WBEffectRunner::ApplyPendingAttackDamage(State);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_zero_hp_death_removal"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyZeroHPDeathRemoval;
		OutResult = WBEffectRunner::ApplyZeroHPDeathRemoval(State);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("resolve_damage_request"))
	{
		OutOperationKind = EWBFixtureOperationKind::ResolveDamageRequest;
		FWBDamageRequest DamageRequest;
		if (!ParseDamageRequestFromFixture(Fixture, DamageRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		const FWBDamageResolutionResult DamageResult = WBDamageResolution::ResolveDamageRequest(State, DamageRequest);
		OutResult.bOk = DamageResult.bOk;
		OutResult.Reason = DamageResult.Reason;
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_armor_effect"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyArmorEffect;
		FWBArmorEffectRequest ArmorRequest;
		if (!ParseArmorEffectRequestFromFixture(Fixture, ArmorRequest, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyArmorEffect(State, ArmorRequest);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_start_turn_status_ticks"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyStartOfTurnStatusTicks;
		int32 PlayerId = -1;
		if (!TryReadIntegerField(OperationObject, TEXT("player_id"), PlayerId))
		{
			OutReason = TEXT("missing_operation_player_id");
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyStartOfTurnStatusTicks(State, PlayerId);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_end_turn_status_ticks"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyEndOfTurnStatusTicks;
		int32 PlayerId = -1;
		if (!TryReadIntegerField(OperationObject, TEXT("player_id"), PlayerId))
		{
			OutReason = TEXT("missing_operation_player_id");
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		OutResult = WBEffectRunner::ApplyEndOfTurnStatusTicks(State, PlayerId);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("attack_declare_then_damage"))
	{
		OutOperationKind = EWBFixtureOperationKind::AttackDeclareThenDamage;
		FWBAction Action;
		if (!ParseActionFromFixture(Fixture, State, Action, OutReason))
		{
			OutResult = MakeFixtureFailure(OutReason);
			return false;
		}

		FWBApplyActionResult DeclareResult = WBEffectRunner::ApplyAttackDeclare(State, Action);
		OutResult = DeclareResult;
		if (!DeclareResult.bOk)
		{
			OutReason.Reset();
			return true;
		}

		FWBApplyActionResult DamageResult = WBEffectRunner::ApplyPendingAttackDamage(State);
		OutResult.bOk = DamageResult.bOk;
		OutResult.Reason = DamageResult.Reason;
		OutResult.TraceEvents.Append(DamageResult.TraceEvents);
		OutReason.Reset();
		return true;
	}

	if (OperationKind == TEXT("apply_turn_command"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyTurnCommand;
		return ApplyTurnCommandFixture(Fixture, State, OutResult, OutReason);
	}

	if (OperationKind == TEXT("apply_runtime_selected_action"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyRuntimeSelectedAction;
		int32 RollSourceRemainingCount = -1;
		return ApplyRuntimeSelectedActionFixture(Fixture, State, OutResult, RollSourceRemainingCount, OutReason);
	}

	if (OperationKind == TEXT("apply_runtime_selected_action_with_result"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyRuntimeSelectedActionWithResult;
		FWBRuntimeSelectedActionResult Envelope;
		int32 RollSourceRemainingCount = -1;
		const bool bApplied = ApplyRuntimeSelectedActionWithResultFixture(
			Fixture,
			State,
			Envelope,
			RollSourceRemainingCount,
			OutReason);
		OutResult = Envelope.ApplyResult;
		return bApplied;
	}

	if (OperationKind == TEXT("apply_runtime_selected_action_with_result_and_serialize"))
	{
		OutOperationKind = EWBFixtureOperationKind::ApplyRuntimeSelectedActionWithResultAndSerialize;
		FWBRuntimeSelectedActionResult Envelope;
		FString SerializedJson;
		int32 RollSourceRemainingCount = -1;
		const bool bApplied = ApplyRuntimeSelectedActionWithResultAndSerializeFixture(
			Fixture,
			State,
			Envelope,
			SerializedJson,
			RollSourceRemainingCount,
			OutReason);
		OutResult = Envelope.ApplyResult;
		return bApplied;
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
			|| Unit->GetCurrentArmor() != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("current_armor"), Unit->GetCurrentArmor())
			|| Unit->GetMaxArmor() != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("max_armor"), Unit->GetMaxArmor())
			|| Unit->AttacksLeft != ReadIntegerFieldOrDefault(FinalUnitObject, TEXT("attacks_left"), Unit->AttacksLeft)
			|| Unit->bDefeated != ReadBoolFieldOrDefault(FinalUnitObject, TEXT("defeated"), Unit->bDefeated)
			|| Unit->bRemovedFromBoard != ReadBoolFieldOrDefault(FinalUnitObject, TEXT("removed_from_board"), Unit->bRemovedFromBoard))
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
	int32 ExpectedWinnerPlayerId = State.WinnerPlayerId;
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

	const TSharedPtr<FJsonObject>* FinalStateObject = nullptr;
	if (ExpectedObject->TryGetObjectField(TEXT("final_state"), FinalStateObject)
		&& FinalStateObject != nullptr
		&& FinalStateObject->IsValid())
	{
		const bool bExpectedGameOver = ReadBoolFieldOrDefault(*FinalStateObject, TEXT("game_over"), State.bGameOver);
		if (State.bGameOver != bExpectedGameOver)
		{
			OutReason = TEXT("final_game_over_mismatch");
			return false;
		}

		if (TryReadIntegerField(*FinalStateObject, TEXT("winner_player_id"), ExpectedWinnerPlayerId)
			&& State.WinnerPlayerId != ExpectedWinnerPlayerId)
		{
			OutReason = TEXT("final_winner_player_id_mismatch");
			return false;
		}
	}

	OutReason.Reset();
	return true;
}

bool ExpectRuntimeRollSourceRemainingCount(
	const TSharedPtr<FJsonObject>& Fixture,
	const int32 RollSourceRemainingCount,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	int32 ExpectedRemainingCount = -1;
	if (!TryReadIntegerField(ExpectedObject, TEXT("roll_source_remaining_count"), ExpectedRemainingCount))
	{
		OutReason.Reset();
		return true;
	}

	if (RollSourceRemainingCount != ExpectedRemainingCount)
	{
		OutReason = FString::Printf(
			TEXT("roll_source_remaining_count_expected_%d_got_%d"),
			ExpectedRemainingCount,
			RollSourceRemainingCount);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool ExpectRuntimeSelectedActionEnvelope(
	const TSharedPtr<FJsonObject>& Fixture,
	const FWBRuntimeSelectedActionResult& Envelope,
	const int32 RollSourceRemainingCount,
	FString& OutReason)
{
	TSharedPtr<FJsonObject> ExpectedObject;
	if (!TryGetExpectedObject(Fixture, ExpectedObject, OutReason))
	{
		return false;
	}

	FString ExpectedSelectedActionId;
	if (ExpectedObject->TryGetStringField(TEXT("selected_action_id"), ExpectedSelectedActionId)
		&& Envelope.SelectedActionId != ExpectedSelectedActionId)
	{
		OutReason = TEXT("selected_action_id_mismatch");
		return false;
	}

	FString ExpectedSelectedActionType;
	if (ExpectedObject->TryGetStringField(TEXT("selected_action_type"), ExpectedSelectedActionType)
		&& Envelope.SelectedActionType.ToString() != ExpectedSelectedActionType)
	{
		OutReason = TEXT("selected_action_type_mismatch");
		return false;
	}

	bool bExpectedConsumedMPRoll = Envelope.bConsumedMPRoll;
	if (ExpectedObject->TryGetBoolField(TEXT("consumed_mp_roll"), bExpectedConsumedMPRoll)
		&& Envelope.bConsumedMPRoll != bExpectedConsumedMPRoll)
	{
		OutReason = TEXT("consumed_mp_roll_mismatch");
		return false;
	}

	int32 ExpectedConsumedMPRollValue = Envelope.ConsumedMPRoll;
	if (TryReadIntegerField(ExpectedObject, TEXT("consumed_mp_roll_value"), ExpectedConsumedMPRollValue)
		&& Envelope.ConsumedMPRoll != ExpectedConsumedMPRollValue)
	{
		OutReason = TEXT("consumed_mp_roll_value_mismatch");
		return false;
	}

	if (!ExpectFinalPublicTurnSummary(ExpectedObject, Envelope.FinalPublicTurnSummary, OutReason))
	{
		return false;
	}

	if (!ExpectRuntimeRollSourceRemainingCount(Fixture, RollSourceRemainingCount, OutReason))
	{
		return false;
	}

	if (!ExpectTraceOrder(Fixture, Envelope.ApplyResult.TraceEvents, OutReason))
	{
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
