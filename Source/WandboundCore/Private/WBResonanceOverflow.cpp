#include "WBResonanceOverflow.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"

namespace
{
FWBResonanceOverflowPlan MakePlan(
	const EWBResonanceOverflowResultCode Code,
	const int32 UnitId,
	const FString& Reason = FString())
{
	FWBResonanceOverflowPlan Plan;
	Plan.bOk = Code == EWBResonanceOverflowResultCode::Success;
	Plan.Code = Code;
	Plan.Reason = Reason.IsEmpty()
		? WBResonanceOverflow::ResultCodeToString(Code)
		: Reason;
	Plan.UnitId = UnitId;
	return Plan;
}

FWBResonanceOverflowResult MakeResult(
	const EWBResonanceOverflowResultCode Code,
	const FWBResonanceOverflowPlan& Plan,
	const FString& Reason = FString())
{
	FWBResonanceOverflowResult Result;
	Result.bOk = Code == EWBResonanceOverflowResultCode::Success;
	Result.Code = Code;
	Result.Reason = Reason.IsEmpty()
		? WBResonanceOverflow::ResultCodeToString(Code)
		: Reason;
	Result.Plan = Plan;
	return Result;
}

int32 SlotSortRank(const FString& SlotId)
{
	if (SlotId == TEXT("wand"))
	{
		return 0;
	}

	const FString Prefix = TEXT("wand_");
	if (SlotId.StartsWith(Prefix))
	{
		const FString Suffix = SlotId.RightChop(Prefix.Len());
		if (Suffix.IsNumeric())
		{
			return FCString::Atoi(*Suffix);
		}
	}

	return 1000000;
}

bool SortByOverflowRemovalOrder(
	const FWBResonanceOverflowRemoval& A,
	const FWBResonanceOverflowRemoval& B)
{
	const int32 SlotRankA = SlotSortRank(A.SlotId);
	const int32 SlotRankB = SlotSortRank(B.SlotId);
	if (SlotRankA != SlotRankB)
	{
		return SlotRankA < SlotRankB;
	}

	if (A.SlotId != B.SlotId)
	{
		return A.SlotId < B.SlotId;
	}

	if (A.EquipOrder != B.EquipOrder)
	{
		return A.EquipOrder < B.EquipOrder;
	}

	return A.SourceInstanceId < B.SourceInstanceId;
}

TSharedRef<FJsonObject> TraceEventToJsonObject(const FWBResonanceOverflowTraceEvent& Event)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("event_type"), Event.EventType);
	Object->SetNumberField(TEXT("player_id"), Event.PlayerId);
	Object->SetNumberField(TEXT("unit_id"), Event.UnitId);
	Object->SetNumberField(TEXT("rl_total"), Event.RLTotal);
	Object->SetNumberField(TEXT("overflow_amount"), Event.OverflowAmount);
	Object->SetStringField(TEXT("source_instance_id"), Event.SourceInstanceId);
	Object->SetStringField(TEXT("source_card_id"), Event.SourceCardId);
	Object->SetStringField(TEXT("slot_id"), Event.SlotId);
	Object->SetNumberField(TEXT("equip_order"), Event.EquipOrder);
	Object->SetNumberField(TEXT("rr"), Event.RR);
	Object->SetNumberField(TEXT("rl_used_before"), Event.RLUsedBefore);
	Object->SetNumberField(TEXT("rl_used_after"), Event.RLUsedAfter);
	return Object;
}
}

FWBResonanceOverflowPlan WBResonanceOverflow::BuildOverflowPlanForUnit(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 UnitId)
{
	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	if (Unit == nullptr || !Unit->IsUnitOnBoard())
	{
		return MakePlan(EWBResonanceOverflowResultCode::TargetUnitNotFound, UnitId);
	}

	FWBResonanceOverflowPlan Plan = MakePlan(EWBResonanceOverflowResultCode::Success, UnitId);
	Plan.PlayerId = Unit->OwnerId;
	Plan.RLTotal = Unit->RLTotal;
	Plan.RLUsedBefore = Unit->RLUsed;
	Plan.RLUsedAfter = Unit->RLUsed;

	if (!FWBGameStateData::IsValidPlayerId(Unit->OwnerId)
		|| State.GetPlayerById(Unit->OwnerId) == nullptr)
	{
		return MakePlan(EWBResonanceOverflowResultCode::InvalidPlayer, UnitId);
	}

	if (Unit->RLTotal < 0 || Unit->RLUsed < 0)
	{
		Plan.bOk = false;
		Plan.Code = EWBResonanceOverflowResultCode::InvalidRLState;
		Plan.Reason = ResultCodeToString(Plan.Code);
		return Plan;
	}

	FString ZoneStateReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(State.GetCardZoneState(), ZoneStateReason))
	{
		Plan.bOk = false;
		Plan.Code = EWBResonanceOverflowResultCode::ZoneStateInvalid;
		Plan.Reason = ZoneStateReason;
		return Plan;
	}

	if (Unit->RLUsed <= Unit->RLTotal)
	{
		return Plan;
	}

	Plan.bHasOverflow = true;
	Plan.OverflowAmount = Unit->RLUsed - Unit->RLTotal;

	TArray<FWBResonanceOverflowRemoval> Candidates;
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		if (Entry.EquippedToUnitId != UnitId)
		{
			continue;
		}

		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Entry.Card.CardId);
		if (!Lookup.bFound)
		{
			Plan.bOk = false;
			Plan.Code = EWBResonanceOverflowResultCode::CardDefinitionNotFound;
			Plan.Reason = ResultCodeToString(Plan.Code);
			return Plan;
		}

		if (Lookup.Definition.Kind != EWBCardDefinitionKind::Wand)
		{
			Plan.bOk = false;
			Plan.Code = EWBResonanceOverflowResultCode::EquippedCardNotWand;
			Plan.Reason = ResultCodeToString(Plan.Code);
			return Plan;
		}

		const int32 RR = Lookup.Definition.WandStats.RR;
		if (RR <= 0)
		{
			Plan.bOk = false;
			Plan.Code = EWBResonanceOverflowResultCode::InvalidRR;
			Plan.Reason = ResultCodeToString(Plan.Code);
			return Plan;
		}

		FWBResonanceOverflowRemoval Removal;
		Removal.PlayerId = Unit->OwnerId;
		Removal.UnitId = UnitId;
		Removal.SourceInstanceId = Entry.Card.InstanceId;
		Removal.SourceCardId = Entry.Card.CardId;
		Removal.SlotId = Entry.SlotId;
		Removal.EquipOrder = Entry.EquipOrder;
		Removal.RR = RR;
		Candidates.Add(Removal);
	}

	Candidates.Sort(SortByOverflowRemovalOrder);

	int32 RunningRLUsed = Unit->RLUsed;
	for (FWBResonanceOverflowRemoval& Candidate : Candidates)
	{
		Candidate.RLUsedBefore = RunningRLUsed;
		RunningRLUsed = FMath::Max(0, RunningRLUsed - Candidate.RR);
		Candidate.RLUsedAfter = RunningRLUsed;
		Plan.Removals.Add(Candidate);

		if (RunningRLUsed <= Unit->RLTotal)
		{
			Plan.RLUsedAfter = RunningRLUsed;
			return Plan;
		}
	}

	Plan.bOk = false;
	Plan.Code = EWBResonanceOverflowResultCode::InsufficientOverflowCandidates;
	Plan.Reason = ResultCodeToString(Plan.Code);
	Plan.RLUsedAfter = RunningRLUsed;
	return Plan;
}

FWBResonanceOverflowResult WBResonanceOverflow::ResolveOverflowForUnit(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 UnitId)
{
	const FWBResonanceOverflowPlan Plan = BuildOverflowPlanForUnit(State, Repository, UnitId);
	if (!Plan.bOk)
	{
		return MakeResult(Plan.Code, Plan, Plan.Reason);
	}

	FWBResonanceOverflowResult Result = MakeResult(EWBResonanceOverflowResultCode::Success, Plan);
	if (!Plan.bHasOverflow)
	{
		return Result;
	}

	Result.bResolvedOverflow = true;

	FWBResonanceOverflowTraceEvent BeginEvent;
	BeginEvent.EventType = TEXT("rl_overflow_begin");
	BeginEvent.PlayerId = Plan.PlayerId;
	BeginEvent.UnitId = Plan.UnitId;
	BeginEvent.RLTotal = Plan.RLTotal;
	BeginEvent.OverflowAmount = Plan.OverflowAmount;
	BeginEvent.RLUsedBefore = Plan.RLUsedBefore;
	BeginEvent.RLUsedAfter = Plan.RLUsedBefore;
	Result.TraceEvents.Add(BeginEvent);

	for (const FWBResonanceOverflowRemoval& Removal : Plan.Removals)
	{
		const FWBCardLifecycleResult LifecycleResult =
			WBCardLifecycle::MoveEquippedCardToDiscard(
				State,
				Removal.PlayerId,
				Removal.SourceInstanceId);
		if (!LifecycleResult.bOk)
		{
			return MakeResult(
				EWBResonanceOverflowResultCode::LifecycleMoveFailed,
				Plan,
				LifecycleResult.Reason);
		}

		FWBUnitState* MutableUnit = State.GetMutableUnitById(Removal.UnitId);
		if (MutableUnit == nullptr || !MutableUnit->IsUnitOnBoard())
		{
			return MakeResult(EWBResonanceOverflowResultCode::TargetUnitNotFound, Plan);
		}
		MutableUnit->RLUsed = Removal.RLUsedAfter;

		FWBResonanceOverflowTraceEvent RemoveEvent;
		RemoveEvent.EventType = TEXT("rl_overflow_remove_wand");
		RemoveEvent.PlayerId = Removal.PlayerId;
		RemoveEvent.UnitId = Removal.UnitId;
		RemoveEvent.RLTotal = Plan.RLTotal;
		RemoveEvent.OverflowAmount = Plan.OverflowAmount;
		RemoveEvent.SourceInstanceId = Removal.SourceInstanceId;
		RemoveEvent.SourceCardId = Removal.SourceCardId;
		RemoveEvent.SlotId = Removal.SlotId;
		RemoveEvent.EquipOrder = Removal.EquipOrder;
		RemoveEvent.RR = Removal.RR;
		RemoveEvent.RLUsedBefore = Removal.RLUsedBefore;
		RemoveEvent.RLUsedAfter = Removal.RLUsedAfter;
		Result.TraceEvents.Add(RemoveEvent);
	}

	FWBResonanceOverflowTraceEvent EndEvent;
	EndEvent.EventType = TEXT("rl_overflow_end");
	EndEvent.PlayerId = Plan.PlayerId;
	EndEvent.UnitId = Plan.UnitId;
	EndEvent.RLTotal = Plan.RLTotal;
	EndEvent.OverflowAmount = Plan.OverflowAmount;
	EndEvent.RLUsedBefore = Plan.RLUsedBefore;
	EndEvent.RLUsedAfter = Plan.RLUsedAfter;
	Result.TraceEvents.Add(EndEvent);
	return Result;
}

bool WBResonanceOverflow::OverflowResultToJsonStringForTest(
	const FWBResonanceOverflowResult& Result,
	FString& OutJson)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("ok"), Result.bOk);
	Object->SetBoolField(TEXT("resolved_overflow"), Result.bResolvedOverflow);
	Object->SetStringField(TEXT("reason"), Result.Reason);
	Object->SetNumberField(TEXT("player_id"), Result.Plan.PlayerId);
	Object->SetNumberField(TEXT("unit_id"), Result.Plan.UnitId);
	Object->SetNumberField(TEXT("rl_total"), Result.Plan.RLTotal);
	Object->SetNumberField(TEXT("rl_used_before"), Result.Plan.RLUsedBefore);
	Object->SetNumberField(TEXT("rl_used_after"), Result.Plan.RLUsedAfter);
	Object->SetNumberField(TEXT("overflow_amount"), Result.Plan.OverflowAmount);

	TArray<TSharedPtr<FJsonValue>> TraceEvents;
	TraceEvents.Reserve(Result.TraceEvents.Num());
	for (const FWBResonanceOverflowTraceEvent& TraceEvent : Result.TraceEvents)
	{
		TraceEvents.Add(MakeShared<FJsonValueObject>(TraceEventToJsonObject(TraceEvent)));
	}
	Object->SetArrayField(TEXT("trace_events"), TraceEvents);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Object, Writer);
}

FString WBResonanceOverflow::ResultCodeToString(const EWBResonanceOverflowResultCode Code)
{
	switch (Code)
	{
	case EWBResonanceOverflowResultCode::Success:
		return TEXT("success");
	case EWBResonanceOverflowResultCode::InvalidPlayer:
		return TEXT("invalid_player");
	case EWBResonanceOverflowResultCode::TargetUnitNotFound:
		return TEXT("target_unit_not_found");
	case EWBResonanceOverflowResultCode::TargetUnitNotOwned:
		return TEXT("target_unit_not_owned");
	case EWBResonanceOverflowResultCode::InvalidRLState:
		return TEXT("invalid_rl_state");
	case EWBResonanceOverflowResultCode::ZoneStateInvalid:
		return TEXT("zone_state_invalid");
	case EWBResonanceOverflowResultCode::CardDefinitionNotFound:
		return TEXT("card_definition_not_found");
	case EWBResonanceOverflowResultCode::EquippedCardNotWand:
		return TEXT("equipped_card_not_wand");
	case EWBResonanceOverflowResultCode::InvalidRR:
		return TEXT("invalid_rr");
	case EWBResonanceOverflowResultCode::InsufficientOverflowCandidates:
		return TEXT("insufficient_overflow_candidates");
	case EWBResonanceOverflowResultCode::LifecycleMoveFailed:
		return TEXT("lifecycle_move_failed");
	case EWBResonanceOverflowResultCode::UnsupportedOverflowOperation:
	default:
		return TEXT("unsupported_overflow_operation");
	}
}
