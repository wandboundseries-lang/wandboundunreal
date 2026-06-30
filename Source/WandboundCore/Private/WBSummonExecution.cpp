#include "WBSummonExecution.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBCardZoneState.h"

namespace
{
constexpr int32 SummonBoardSize = 9;
constexpr int32 MaxOwnedUnitsIncludingHero = 4;

bool IsTileInBounds(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.X < SummonBoardSize && Tile.Y >= 0 && Tile.Y < SummonBoardSize;
}

bool AreOrthogonallyAdjacent(const FWBTile& A, const FWBTile& B)
{
	const int32 DeltaX = FMath::Abs(A.X - B.X);
	const int32 DeltaY = FMath::Abs(A.Y - B.Y);
	return DeltaX + DeltaY == 1;
}

bool HasMarkerAtTile(const FWBCardZoneState& ZoneState, const FWBTile& Tile)
{
	for (const FWBMarkerPlaceholderEntry& Marker : ZoneState.MarkerPlaceholders)
	{
		if (Marker.Tile == Tile)
		{
			return true;
		}
	}

	return false;
}

bool AreCharacterStatsValid(const FWBCardCharacterStatsDefinition& Stats)
{
	return Stats.HP > 0
		&& Stats.ATK >= 0
		&& Stats.AR >= 0
		&& Stats.RL >= 0;
}

FWBSummonExecutionResult MakeResult(
	const EWBSummonExecutionResultCode Code,
	const FWBSummonExecutionRequest& Request,
	const FString& Reason = FString())
{
	FWBSummonExecutionResult Result;
	Result.bOk = Code == EWBSummonExecutionResultCode::Success;
	Result.Code = Code;
	Result.Reason = Reason.IsEmpty()
		? WBSummonExecution::ResultCodeToString(Code)
		: Reason;
	Result.PlayerId = Request.PlayerId;
	Result.SourceInstanceId = Request.SourceInstanceId;
	Result.SourceCardId = Request.SourceCardId;
	Result.TargetTile = Request.TargetTile;
	return Result;
}

const FWBUnitState* FindHeroUnit(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
	if (Player == nullptr || Player->HeroUnitId < 0)
	{
		return nullptr;
	}

	const FWBUnitState* Hero = State.GetUnitById(Player->HeroUnitId);
	if (Hero == nullptr
		|| Hero->OwnerId != PlayerId
		|| !Hero->IsUnitOnBoard())
	{
		return nullptr;
	}

	return Hero;
}

int32 AllocateNextUnitId(const FWBGameStateData& State)
{
	int32 MaxUnitId = -1;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId == MAX_int32)
		{
			return INDEX_NONE;
		}

		MaxUnitId = FMath::Max(MaxUnitId, Unit.UnitId);
	}

	return MaxUnitId + 1;
}

void SortHandAndNormalizeIndexes(FWBPlayerCardZoneState& PlayerZones)
{
	PlayerZones.Hand.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
	{
		if (A.ZoneIndex != B.ZoneIndex)
		{
			return A.ZoneIndex < B.ZoneIndex;
		}
		if (A.Card.InstanceId != B.Card.InstanceId)
		{
			return A.Card.InstanceId < B.Card.InstanceId;
		}
		return A.Card.CardId < B.Card.CardId;
	});

	for (int32 Index = 0; Index < PlayerZones.Hand.Num(); ++Index)
	{
		PlayerZones.Hand[Index].Zone = EWBCardZone::Hand;
		PlayerZones.Hand[Index].ZoneIndex = Index;
		PlayerZones.Hand[Index].Card.OwnerPlayerId = PlayerZones.PlayerId;
	}
}

TSharedRef<FJsonObject> SummonTileToJsonObject(const FWBTile& Tile)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("x"), Tile.X);
	Object->SetNumberField(TEXT("y"), Tile.Y);
	return Object;
}

TSharedRef<FJsonObject> TraceEventToJsonObject(const FWBSummonExecutionTraceEvent& Event)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("event_type"), Event.EventType);
	Object->SetNumberField(TEXT("player_id"), Event.PlayerId);
	Object->SetStringField(TEXT("source_instance_id"), Event.SourceInstanceId);
	Object->SetStringField(TEXT("source_card_id"), Event.SourceCardId);
	Object->SetNumberField(TEXT("created_unit_id"), Event.CreatedUnitId);
	Object->SetObjectField(TEXT("target_tile"), SummonTileToJsonObject(Event.TargetTile));
	return Object;
}
}

FWBSummonExecutionResult WBSummonExecution::ExecuteCharacterSummonFromHand(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBSummonExecutionRequest& Request)
{
	if (!FWBGameStateData::IsValidPlayerId(Request.PlayerId)
		|| State.GetPlayerById(Request.PlayerId) == nullptr)
	{
		return MakeResult(EWBSummonExecutionResultCode::InvalidPlayer, Request);
	}

	FString ZoneStateReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(State.GetCardZoneState(), ZoneStateReason))
	{
		return MakeResult(EWBSummonExecutionResultCode::ZoneStateInvalid, Request, ZoneStateReason);
	}

	const FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), Request.PlayerId);
	if (PlayerZones == nullptr)
	{
		return MakeResult(EWBSummonExecutionResultCode::PlayerZonesMissing, Request);
	}

	if (Request.SourceInstanceId.IsEmpty())
	{
		return MakeResult(EWBSummonExecutionResultCode::SourceCardMissing, Request);
	}

	int32 SourceHandIndex = INDEX_NONE;
	for (int32 Index = 0; Index < PlayerZones->Hand.Num(); ++Index)
	{
		if (PlayerZones->Hand[Index].Card.InstanceId == Request.SourceInstanceId)
		{
			SourceHandIndex = Index;
			break;
		}
	}

	if (SourceHandIndex == INDEX_NONE)
	{
		FWBZoneCardEntry ExistingEntry;
		const bool bKnownCardInstance = WBCardZoneState::FindCardByInstanceId(
			State.GetCardZoneState(),
			Request.SourceInstanceId,
			ExistingEntry);
		return MakeResult(
			bKnownCardInstance
				? EWBSummonExecutionResultCode::SourceCardNotInHand
				: EWBSummonExecutionResultCode::SourceCardMissing,
			Request);
	}

	const FWBZoneCardEntry SourceEntry = PlayerZones->Hand[SourceHandIndex];
	if (SourceEntry.Card.CardId != Request.SourceCardId)
	{
		return MakeResult(EWBSummonExecutionResultCode::SourceCardIdMismatch, Request);
	}

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, Request.SourceCardId);
	if (!Lookup.bFound)
	{
		return MakeResult(EWBSummonExecutionResultCode::CardDefinitionNotFound, Request);
	}

	if (Lookup.Definition.Kind != EWBCardDefinitionKind::Character)
	{
		return MakeResult(EWBSummonExecutionResultCode::SourceCardNotCharacter, Request);
	}

	if (!AreCharacterStatsValid(Lookup.Definition.CharacterStats))
	{
		return MakeResult(EWBSummonExecutionResultCode::InvalidCharacterStats, Request);
	}

	const FWBUnitState* Hero = FindHeroUnit(State, Request.PlayerId);
	if (Hero == nullptr)
	{
		return MakeResult(EWBSummonExecutionResultCode::HeroNotFound, Request);
	}

	const FWBTile HeroTile(Hero->X, Hero->Y);
	if (!IsTileInBounds(HeroTile))
	{
		return MakeResult(EWBSummonExecutionResultCode::HeroNotFound, Request);
	}

	if (State.GetUnitsForPlayer(Request.PlayerId).Num() >= MaxOwnedUnitsIncludingHero)
	{
		return MakeResult(EWBSummonExecutionResultCode::UnitCapReached, Request);
	}

	if (!IsTileInBounds(Request.TargetTile))
	{
		return MakeResult(EWBSummonExecutionResultCode::TargetTileOutOfBounds, Request);
	}

	if (!AreOrthogonallyAdjacent(HeroTile, Request.TargetTile))
	{
		return MakeResult(EWBSummonExecutionResultCode::TargetTileNotAdjacentToHero, Request);
	}

	if (State.IsTileOccupied(Request.TargetTile))
	{
		return MakeResult(EWBSummonExecutionResultCode::TargetTileOccupied, Request);
	}

	if (HasMarkerAtTile(State.GetCardZoneState(), Request.TargetTile))
	{
		return MakeResult(EWBSummonExecutionResultCode::MarkerTriggerDeferred, Request);
	}

	const int32 NewUnitId = AllocateNextUnitId(State);
	if (NewUnitId < 0 || State.GetUnitById(NewUnitId) != nullptr)
	{
		return MakeResult(EWBSummonExecutionResultCode::UnitIdAllocationFailed, Request);
	}

	FWBUnitState NewUnit;
	NewUnit.UnitId = NewUnitId;
	NewUnit.OwnerId = Request.PlayerId;
	NewUnit.CardId = Request.SourceCardId;
	NewUnit.X = Request.TargetTile.X;
	NewUnit.Y = Request.TargetTile.Y;
	NewUnit.HP = Lookup.Definition.CharacterStats.HP;
	NewUnit.MaxHP = Lookup.Definition.CharacterStats.HP;
	NewUnit.ATK = Lookup.Definition.CharacterStats.ATK;
	NewUnit.AR = Lookup.Definition.CharacterStats.AR;
	NewUnit.RLTotal = Lookup.Definition.CharacterStats.RL;
	NewUnit.RLUsed = 0;
	NewUnit.AttacksLeft = 0;
	NewUnit.MaxAttacksPerTurn = 1;
	NewUnit.MPRemaining = 0;

	FWBPlayerCardZoneState* MutablePlayerZones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), Request.PlayerId);
	if (MutablePlayerZones == nullptr || SourceHandIndex < 0 || SourceHandIndex >= MutablePlayerZones->Hand.Num())
	{
		return MakeResult(EWBSummonExecutionResultCode::PlayerZonesMissing, Request);
	}

	State.Units.Add(NewUnit);
	MutablePlayerZones->Hand.RemoveAt(SourceHandIndex, 1, EAllowShrinking::No);
	SortHandAndNormalizeIndexes(*MutablePlayerZones);
	WBCardZoneState::SortOrderedZonesDeterministically(State.GetMutableCardZoneStateForTest());

	FWBSummonExecutionTraceEvent TraceEvent;
	TraceEvent.EventType = TEXT("summon_unit");
	TraceEvent.PlayerId = Request.PlayerId;
	TraceEvent.SourceInstanceId = Request.SourceInstanceId;
	TraceEvent.SourceCardId = Request.SourceCardId;
	TraceEvent.CreatedUnitId = NewUnitId;
	TraceEvent.TargetTile = Request.TargetTile;

	FWBSummonExecutionResult Result = MakeResult(EWBSummonExecutionResultCode::Success, Request);
	Result.CreatedUnitId = NewUnitId;
	Result.TraceEvents.Add(TraceEvent);
	return Result;
}

bool WBSummonExecution::SummonExecutionResultToJsonStringForTest(
	const FWBSummonExecutionResult& Result,
	FString& OutJson)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("ok"), Result.bOk);
	Object->SetStringField(TEXT("reason"), Result.Reason);
	Object->SetNumberField(TEXT("player_id"), Result.PlayerId);
	Object->SetStringField(TEXT("source_instance_id"), Result.SourceInstanceId);
	Object->SetStringField(TEXT("source_card_id"), Result.SourceCardId);
	Object->SetNumberField(TEXT("created_unit_id"), Result.CreatedUnitId);
	Object->SetObjectField(TEXT("target_tile"), SummonTileToJsonObject(Result.TargetTile));

	TArray<TSharedPtr<FJsonValue>> TraceEvents;
	TraceEvents.Reserve(Result.TraceEvents.Num());
	for (const FWBSummonExecutionTraceEvent& TraceEvent : Result.TraceEvents)
	{
		TraceEvents.Add(MakeShared<FJsonValueObject>(TraceEventToJsonObject(TraceEvent)));
	}
	Object->SetArrayField(TEXT("trace_events"), TraceEvents);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object, Writer);
}

FString WBSummonExecution::ResultCodeToString(const EWBSummonExecutionResultCode Code)
{
	switch (Code)
	{
	case EWBSummonExecutionResultCode::Success:
		return TEXT("success");
	case EWBSummonExecutionResultCode::GameStateMissing:
		return TEXT("game_state_missing");
	case EWBSummonExecutionResultCode::RepositoryMissing:
		return TEXT("repository_missing");
	case EWBSummonExecutionResultCode::InvalidPlayer:
		return TEXT("invalid_player");
	case EWBSummonExecutionResultCode::PlayerZonesMissing:
		return TEXT("player_zones_missing");
	case EWBSummonExecutionResultCode::SourceCardMissing:
		return TEXT("source_card_missing");
	case EWBSummonExecutionResultCode::SourceCardNotInHand:
		return TEXT("source_card_not_in_hand");
	case EWBSummonExecutionResultCode::SourceCardIdMismatch:
		return TEXT("source_card_id_mismatch");
	case EWBSummonExecutionResultCode::CardDefinitionNotFound:
		return TEXT("card_definition_not_found");
	case EWBSummonExecutionResultCode::SourceCardNotCharacter:
		return TEXT("source_card_not_character");
	case EWBSummonExecutionResultCode::InvalidCharacterStats:
		return TEXT("invalid_character_stats");
	case EWBSummonExecutionResultCode::HeroNotFound:
		return TEXT("hero_not_found");
	case EWBSummonExecutionResultCode::UnitCapReached:
		return TEXT("unit_cap_reached");
	case EWBSummonExecutionResultCode::TargetTileOutOfBounds:
		return TEXT("target_tile_out_of_bounds");
	case EWBSummonExecutionResultCode::TargetTileNotAdjacentToHero:
		return TEXT("target_tile_not_adjacent_to_hero");
	case EWBSummonExecutionResultCode::TargetTileOccupied:
		return TEXT("target_tile_occupied");
	case EWBSummonExecutionResultCode::MarkerTriggerDeferred:
		return TEXT("marker_trigger_deferred");
	case EWBSummonExecutionResultCode::TargetTileNotAllowed:
		return TEXT("target_tile_not_allowed");
	case EWBSummonExecutionResultCode::UnitIdAllocationFailed:
		return TEXT("unit_id_allocation_failed");
	case EWBSummonExecutionResultCode::ZoneStateInvalid:
		return TEXT("zone_state_invalid");
	case EWBSummonExecutionResultCode::UnsupportedSummonOperation:
	default:
		return TEXT("unsupported_summon_operation");
	}
}
