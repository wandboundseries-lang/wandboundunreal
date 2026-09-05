#include "WBSummonExecution.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBCharacterSummon.h"

namespace
{
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
EWBSummonExecutionResultCode ResultCodeFromGenericReason(
	const FString& Reason)
{
	if (Reason == TEXT("invalid_player"))
		return EWBSummonExecutionResultCode::InvalidPlayer;
	if (Reason == TEXT("player_zones_missing"))
		return EWBSummonExecutionResultCode::PlayerZonesMissing;
	if (Reason == TEXT("source_card_missing"))
		return EWBSummonExecutionResultCode::SourceCardMissing;
	if (Reason == TEXT("source_card_not_in_hand"))
		return EWBSummonExecutionResultCode::SourceCardNotInHand;
	if (Reason == TEXT("source_card_id_mismatch"))
		return EWBSummonExecutionResultCode::SourceCardIdMismatch;
	if (Reason == TEXT("card_definition_not_found"))
		return EWBSummonExecutionResultCode::CardDefinitionNotFound;
	if (Reason == TEXT("source_card_not_character"))
		return EWBSummonExecutionResultCode::SourceCardNotCharacter;
	if (Reason == TEXT("invalid_character_stats"))
		return EWBSummonExecutionResultCode::InvalidCharacterStats;
	if (Reason == TEXT("hero_not_found"))
		return EWBSummonExecutionResultCode::HeroNotFound;
	if (Reason == TEXT("unit_cap_reached"))
		return EWBSummonExecutionResultCode::UnitCapReached;
	if (Reason == TEXT("target_tile_out_of_bounds"))
		return EWBSummonExecutionResultCode::TargetTileOutOfBounds;
	if (Reason == TEXT("target_tile_not_adjacent_to_hero"))
		return EWBSummonExecutionResultCode::TargetTileNotAdjacentToHero;
	if (Reason == TEXT("target_tile_occupied"))
		return EWBSummonExecutionResultCode::TargetTileOccupied;
	if (Reason == TEXT("unit_id_allocation_failed"))
		return EWBSummonExecutionResultCode::UnitIdAllocationFailed;
	if (Reason.Contains(TEXT("zone")) || Reason.Contains(TEXT("duplicate")))
		return EWBSummonExecutionResultCode::ZoneStateInvalid;
	return EWBSummonExecutionResultCode::TargetTileNotAllowed;
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
	FWBCharacterSummonRequest GenericRequest;
	GenericRequest.OwnerPlayerId = Request.PlayerId;
	GenericRequest.ControllerPlayerId = Request.PlayerId;
	GenericRequest.SourceZone = EWBCardZone::Hand;
	GenericRequest.CardInstanceId = Request.SourceInstanceId;
	GenericRequest.ExpectedCardId = Request.SourceCardId;
	GenericRequest.TargetTile = Request.TargetTile;
	GenericRequest.ConditionPolicy = EWBCharacterSummonConditionPolicy::Normal;
	GenericRequest.TraceKind = FName(TEXT("summon_unit"));
	const FWBCharacterSummonResult Generic =
		WBCharacterSummon::SummonExactCharacter(
			State, Repository, GenericRequest);
	if (!Generic.bOk)
	{
		return MakeResult(
			ResultCodeFromGenericReason(Generic.Reason),
			Request,
			Generic.Reason);
	}

	FWBSummonExecutionTraceEvent TraceEvent;
	TraceEvent.EventType = TEXT("summon_unit");
	TraceEvent.PlayerId = Request.PlayerId;
	TraceEvent.SourceInstanceId = Request.SourceInstanceId;
	TraceEvent.SourceCardId = Request.SourceCardId;
	TraceEvent.CreatedUnitId = Generic.CreatedUnitId;
	TraceEvent.TargetTile = Request.TargetTile;

	FWBSummonExecutionResult Result = MakeResult(EWBSummonExecutionResultCode::Success, Request);
	Result.CreatedUnitId = Generic.CreatedUnitId;
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
