#include "WBEquipExecution.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBCardZoneState.h"
#include "WBResonanceLoad.h"

namespace
{
FWBEquipExecutionResult MakeResult(
	const EWBEquipExecutionResultCode Code,
	const FWBEquipExecutionRequest& Request,
	const FString& Reason = FString())
{
	FWBEquipExecutionResult Result;
	Result.bOk = Code == EWBEquipExecutionResultCode::Success;
	Result.Code = Code;
	Result.Reason = Reason.IsEmpty()
		? WBEquipExecution::ResultCodeToString(Code)
		: Reason;
	Result.PlayerId = Request.PlayerId;
	Result.SourceInstanceId = Request.SourceInstanceId;
	Result.SourceCardId = Request.SourceCardId;
	Result.EquippedToUnitId = Request.TargetUnitId;
	return Result;
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

FString BuildNextWandSlotId(const FWBCardZoneState& ZoneState, const int32 UnitId)
{
	TSet<FString> UsedSlots;
	for (const FWBEquippedCardEntry& Entry : ZoneState.EquippedCards)
	{
		if (Entry.EquippedToUnitId == UnitId)
		{
			UsedSlots.Add(Entry.SlotId);
		}
	}

	if (!UsedSlots.Contains(TEXT("wand")))
	{
		return TEXT("wand");
	}

	for (int32 Index = 1; Index < MAX_int32; ++Index)
	{
		const FString Candidate = FString::Printf(TEXT("wand_%d"), Index);
		if (!UsedSlots.Contains(Candidate))
		{
			return Candidate;
		}
	}

	return TEXT("wand_overflow");
}

TSharedRef<FJsonObject> TraceEventToJsonObject(const FWBEquipExecutionTraceEvent& Event)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("event_type"), Event.EventType);
	Object->SetNumberField(TEXT("player_id"), Event.PlayerId);
	Object->SetStringField(TEXT("source_instance_id"), Event.SourceInstanceId);
	Object->SetStringField(TEXT("source_card_id"), Event.SourceCardId);
	Object->SetNumberField(TEXT("equipped_to_unit_id"), Event.EquippedToUnitId);
	Object->SetStringField(TEXT("slot_id"), Event.SlotId);
	Object->SetNumberField(TEXT("rr"), Event.RR);
	Object->SetNumberField(TEXT("rl_used_before"), Event.RLUsedBefore);
	Object->SetNumberField(TEXT("rl_used_after"), Event.RLUsedAfter);
	return Object;
}
}

FWBEquipExecutionResult WBEquipExecution::ExecuteWandEquipFromHand(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBEquipExecutionRequest& Request)
{
	if (!FWBGameStateData::IsValidPlayerId(Request.PlayerId)
		|| State.GetPlayerById(Request.PlayerId) == nullptr)
	{
		return MakeResult(EWBEquipExecutionResultCode::InvalidPlayer, Request);
	}

	FString ZoneStateReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(State.GetCardZoneState(), ZoneStateReason))
	{
		return MakeResult(EWBEquipExecutionResultCode::ZoneStateInvalid, Request, ZoneStateReason);
	}

	const FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), Request.PlayerId);
	if (PlayerZones == nullptr)
	{
		return MakeResult(EWBEquipExecutionResultCode::PlayerZonesMissing, Request);
	}

	if (Request.SourceInstanceId.IsEmpty())
	{
		return MakeResult(EWBEquipExecutionResultCode::SourceCardMissing, Request);
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
				? EWBEquipExecutionResultCode::SourceCardNotInHand
				: EWBEquipExecutionResultCode::SourceCardMissing,
			Request);
	}

	const FWBZoneCardEntry SourceEntry = PlayerZones->Hand[SourceHandIndex];
	if (SourceEntry.Card.CardId != Request.SourceCardId)
	{
		return MakeResult(EWBEquipExecutionResultCode::SourceCardIdMismatch, Request);
	}

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, Request.SourceCardId);
	if (!Lookup.bFound)
	{
		return MakeResult(EWBEquipExecutionResultCode::CardDefinitionNotFound, Request);
	}

	if (Lookup.Definition.Kind != EWBCardDefinitionKind::Wand)
	{
		return MakeResult(EWBEquipExecutionResultCode::SourceCardNotWand, Request);
	}

	const int32 RR = Lookup.Definition.WandStats.RR;
	if (RR < 0)
	{
		FWBEquipExecutionResult Result = MakeResult(EWBEquipExecutionResultCode::InvalidRR, Request);
		Result.RR = RR;
		return Result;
	}

	const FWBUnitState* TargetUnit = State.GetUnitById(Request.TargetUnitId);
	if (TargetUnit == nullptr || !TargetUnit->IsUnitOnBoard())
	{
		return MakeResult(EWBEquipExecutionResultCode::TargetUnitNotFound, Request);
	}

	if (TargetUnit->OwnerId != Request.PlayerId)
	{
		return MakeResult(EWBEquipExecutionResultCode::TargetUnitNotOwned, Request);
	}

	FWBResonanceLoadSummary LoadSummary;
	if (!WBResonanceLoad::CanPayRR(State, Request.TargetUnitId, RR, LoadSummary))
	{
		FWBEquipExecutionResult Result = MakeResult(
			LoadSummary.Reason == TEXT("invalid_rr")
				? EWBEquipExecutionResultCode::InvalidRR
				: EWBEquipExecutionResultCode::InsufficientRL,
			Request);
		Result.RR = RR;
		Result.RLUsedBefore = LoadSummary.RLUsed;
		Result.RLUsedAfter = LoadSummary.RLUsed;
		return Result;
	}

	FWBPlayerCardZoneState* MutablePlayerZones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), Request.PlayerId);
	FWBUnitState* MutableTargetUnit = State.GetMutableUnitById(Request.TargetUnitId);
	if (MutablePlayerZones == nullptr || SourceHandIndex < 0 || SourceHandIndex >= MutablePlayerZones->Hand.Num())
	{
		return MakeResult(EWBEquipExecutionResultCode::PlayerZonesMissing, Request);
	}
	if (MutableTargetUnit == nullptr || !MutableTargetUnit->IsUnitOnBoard())
	{
		return MakeResult(EWBEquipExecutionResultCode::TargetUnitNotFound, Request);
	}

	const int32 RLUsedBefore = MutableTargetUnit->RLUsed;
	const int32 RLUsedAfter = RLUsedBefore + RR;
	const FString SlotId = BuildNextWandSlotId(State.GetCardZoneState(), Request.TargetUnitId);

	FWBEquippedCardEntry EquippedEntry;
	EquippedEntry.Card = SourceEntry.Card;
	EquippedEntry.Card.OwnerPlayerId = Request.PlayerId;
	EquippedEntry.EquippedToUnitId = Request.TargetUnitId;
	EquippedEntry.SlotId = SlotId;

	MutablePlayerZones->Hand.RemoveAt(SourceHandIndex, 1, EAllowShrinking::No);
	SortHandAndNormalizeIndexes(*MutablePlayerZones);
	MutableTargetUnit->RLUsed = RLUsedAfter;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(EquippedEntry);
	WBCardZoneState::SortOrderedZonesDeterministically(State.GetMutableCardZoneStateForTest());

	FWBEquipExecutionTraceEvent TraceEvent;
	TraceEvent.EventType = TEXT("equip_wand");
	TraceEvent.PlayerId = Request.PlayerId;
	TraceEvent.SourceInstanceId = Request.SourceInstanceId;
	TraceEvent.SourceCardId = Request.SourceCardId;
	TraceEvent.EquippedToUnitId = Request.TargetUnitId;
	TraceEvent.SlotId = SlotId;
	TraceEvent.RR = RR;
	TraceEvent.RLUsedBefore = RLUsedBefore;
	TraceEvent.RLUsedAfter = RLUsedAfter;

	FWBEquipExecutionResult Result = MakeResult(EWBEquipExecutionResultCode::Success, Request);
	Result.SlotId = SlotId;
	Result.RR = RR;
	Result.RLUsedBefore = RLUsedBefore;
	Result.RLUsedAfter = RLUsedAfter;
	Result.TraceEvents.Add(TraceEvent);
	return Result;
}

bool WBEquipExecution::EquipExecutionResultToJsonStringForTest(
	const FWBEquipExecutionResult& Result,
	FString& OutJson)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("ok"), Result.bOk);
	Object->SetStringField(TEXT("reason"), Result.Reason);
	Object->SetNumberField(TEXT("player_id"), Result.PlayerId);
	Object->SetStringField(TEXT("source_instance_id"), Result.SourceInstanceId);
	Object->SetStringField(TEXT("source_card_id"), Result.SourceCardId);
	Object->SetNumberField(TEXT("equipped_to_unit_id"), Result.EquippedToUnitId);
	Object->SetStringField(TEXT("slot_id"), Result.SlotId);
	Object->SetNumberField(TEXT("rr"), Result.RR);
	Object->SetNumberField(TEXT("rl_used_before"), Result.RLUsedBefore);
	Object->SetNumberField(TEXT("rl_used_after"), Result.RLUsedAfter);

	TArray<TSharedPtr<FJsonValue>> TraceEvents;
	TraceEvents.Reserve(Result.TraceEvents.Num());
	for (const FWBEquipExecutionTraceEvent& TraceEvent : Result.TraceEvents)
	{
		TraceEvents.Add(MakeShared<FJsonValueObject>(TraceEventToJsonObject(TraceEvent)));
	}
	Object->SetArrayField(TEXT("trace_events"), TraceEvents);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object, Writer);
}

FString WBEquipExecution::ResultCodeToString(const EWBEquipExecutionResultCode Code)
{
	switch (Code)
	{
	case EWBEquipExecutionResultCode::Success:
		return TEXT("success");
	case EWBEquipExecutionResultCode::InvalidPlayer:
		return TEXT("invalid_player");
	case EWBEquipExecutionResultCode::PlayerZonesMissing:
		return TEXT("player_zones_missing");
	case EWBEquipExecutionResultCode::SourceCardMissing:
		return TEXT("source_card_missing");
	case EWBEquipExecutionResultCode::SourceCardNotInHand:
		return TEXT("source_card_not_in_hand");
	case EWBEquipExecutionResultCode::SourceCardIdMismatch:
		return TEXT("source_card_id_mismatch");
	case EWBEquipExecutionResultCode::CardDefinitionNotFound:
		return TEXT("card_definition_not_found");
	case EWBEquipExecutionResultCode::SourceCardNotWand:
		return TEXT("source_card_not_wand");
	case EWBEquipExecutionResultCode::InvalidRR:
		return TEXT("invalid_rr");
	case EWBEquipExecutionResultCode::TargetUnitNotFound:
		return TEXT("target_unit_not_found");
	case EWBEquipExecutionResultCode::TargetUnitNotOwned:
		return TEXT("target_unit_not_owned");
	case EWBEquipExecutionResultCode::InsufficientRL:
		return TEXT("insufficient_rl");
	case EWBEquipExecutionResultCode::ZoneStateInvalid:
		return TEXT("zone_state_invalid");
	case EWBEquipExecutionResultCode::UnsupportedEquipOperation:
	default:
		return TEXT("unsupported_equip_operation");
	}
}
