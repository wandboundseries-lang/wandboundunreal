#include "WBCSNInheritanceTrigger.h"

#include "WBCardLifecycle.h"
#include "WBCharacterPassiveEligibility.h"

namespace
{
FString BuildStableTriggerId(
	const FWBAfterCSNInheritanceTriggerDefinition& Trigger,
	const FWBCSNInheritanceEventContext& Context)
{
	return FString::Printf(
		TEXT("after_csn_inheritance:%s:p%d:u%d:%s"),
		*Context.TransactionId,
		Context.InheritingPlayerId,
		Context.InheritingUnitId,
		*Trigger.TriggerId);
}

FWBTraceEvent MakeTrace(
	const FName Kind,
	const FString& StableTriggerId,
	const FWBCSNInheritanceEventContext& Context,
	const int32 CardCount)
{
	FWBTraceEvent Event;
	Event.Kind = Kind;
	Event.ActionId = StableTriggerId;
	Event.PlayerId = Context.InheritingPlayerId;
	Event.SourceUnitId = Context.SourceUnitId;
	Event.TargetUnitId = Context.InheritingUnitId;
	Event.CardCount = CardCount;
	Event.InheritedRL = Context.SourceCurrentRL;
	Event.AttackContinuationId = Context.TransactionId;
	Event.bOk = true;
	return Event;
}
}

FWBCSNInheritanceTriggerResult
WBCSNInheritanceTrigger::ResolveAfterSuccessfulInheritance(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBCSNInheritanceEventContext& Context)
{
	FWBCSNInheritanceTriggerResult Result;
	if (!FWBGameStateData::IsValidPlayerId(Context.InheritingPlayerId)
		|| Context.InheritingUnitId < 0
		|| Context.SourceCurrentRL < 0
		|| Context.InheritedWandCount < 0
		|| Context.TransactionId.IsEmpty())
	{
		Result.Reason = TEXT("csn_inheritance_trigger_context_invalid");
		return Result;
	}

	const FWBUnitState* Unit = State.GetUnitById(Context.InheritingUnitId);
	if (Unit == nullptr
		|| Unit->OwnerId != Context.InheritingPlayerId
		|| !Unit->IsUnitOnBoard()
		|| Unit->bDefeated)
	{
		Result.Reason = TEXT("csn_inheritance_trigger_source_invalid");
		return Result;
	}

	if (State.bGameOver
		|| !WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(*Unit))
	{
		Result.bOk = true;
		return Result;
	}

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, Unit->CardId);
	if (!Lookup.bFound)
	{
		Result.Reason = TEXT("csn_inheritance_trigger_definition_missing");
		return Result;
	}

	TArray<FWBAfterCSNInheritanceTriggerDefinition> Triggers =
		Lookup.Definition.AfterCSNInheritanceTriggers;
	Triggers.Sort([](
		const FWBAfterCSNInheritanceTriggerDefinition& A,
		const FWBAfterCSNInheritanceTriggerDefinition& B)
	{
		return A.TriggerId < B.TriggerId;
	});
	FWBGameStateData WorkingState = State;

	for (const FWBAfterCSNInheritanceTriggerDefinition& Trigger : Triggers)
	{
		if (!Trigger.bMandatory)
		{
			Result.Reason =
				TEXT("optional_csn_inheritance_trigger_unsupported");
			return Result;
		}
		if (Trigger.TriggerId.IsEmpty() || Trigger.DrawCount <= 0)
		{
			Result.Reason = TEXT("csn_inheritance_trigger_effect_missing");
			return Result;
		}

		const FString StableTriggerId = BuildStableTriggerId(Trigger, Context);
		Result.TraceEvents.Add(MakeTrace(
			FName(TEXT("csn_inheritance_triggered")),
			StableTriggerId,
			Context,
			Trigger.DrawCount));

		const FWBCardLifecycleResult Draw = WBCardLifecycle::DrawCards(
			WorkingState, Context.InheritingPlayerId, Trigger.DrawCount);
		if (!Draw.bOk)
		{
			Result.Reason = Draw.Reason;
			return Result;
		}

		Result.TraceEvents.Add(MakeTrace(
			FName(TEXT("csn_inheritance_card_drawn")),
			StableTriggerId,
			Context,
			Trigger.DrawCount));
		Result.TraceEvents.Add(MakeTrace(
			FName(TEXT("csn_inheritance_trigger_resolved")),
			StableTriggerId,
			Context,
			Trigger.DrawCount));
		++Result.ResolvedTriggerCount;
		Result.DrawnCardCount += Trigger.DrawCount;
	}

	State = MoveTemp(WorkingState);
	Result.bOk = true;
	return Result;
}
