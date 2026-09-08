#include "WBCardZoneTransitionTrigger.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"

namespace
{
bool IsSupportedSourceScope(
	const EWBCardZoneTransitionTriggerSourceScope Scope)
{
	return Scope == EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf
		|| Scope == EWBCardZoneTransitionTriggerSourceScope::
			ResidentDiscardObserver;
}

FWBTraceEvent MakeRedactedTriggerTrace(
	const FName Kind,
	const FWBCardZoneTransitionTriggerInstance& Trigger,
	const int32 CardCount)
{
	FWBTraceEvent Trace;
	Trace.Kind = Kind;
	Trace.PlayerId = Trigger.SourceSnapshot.OwnerPlayerId;
	Trace.TurnNumber =
		Trigger.TransitionSnapshot.EventIdentity.TurnNumber;
	Trace.CardCount = CardCount;
	Trace.ResolutionOrder = Trigger.CollectionOrder;
	Trace.SourceUnitId = INDEX_NONE;
	Trace.bOk = true;
	return Trace;
}

FString BuildStableTriggerId(
	const FWBCardZoneTransitionSnapshot& Transition,
	const FWBCardZoneTransitionTriggerSourceSnapshot& Source,
	const int32 CollectionOrder)
{
	return FString::Printf(
		TEXT("card_zone_trigger:%s:s%d:i%s:t%s:o%d"),
		*Transition.EventIdentity.EventId,
		static_cast<int32>(Source.SourceScope),
		*Source.SourceCardInstanceId,
		*Source.TriggerId,
		CollectionOrder);
}

void SortDefinitions(
	TArray<FWBCardZoneTransitionTriggerDefinition>& Definitions)
{
	Definitions.Sort([](
		const FWBCardZoneTransitionTriggerDefinition& A,
		const FWBCardZoneTransitionTriggerDefinition& B)
	{
		return A.TriggerId < B.TriggerId;
	});
}

bool AddMatchingDefinitions(
	const FWBCardDefinition& CardDefinition,
	const FWBCardZoneTransitionSnapshot& Transition,
	const FWBCardZoneTransitionTriggerSourceSnapshot& SourceBase,
	TArray<FWBCardZoneTransitionTriggerInstance>& OutTriggers,
	FString& OutReason)
{
	TArray<FWBCardZoneTransitionTriggerDefinition> Definitions =
		CardDefinition.CardZoneTransitionTriggers;
	SortDefinitions(Definitions);
	for (const FWBCardZoneTransitionTriggerDefinition& Definition : Definitions)
	{
		if (Definition.SourceScope != SourceBase.SourceScope
			|| !WBCardZoneTransitionTrigger::MatchesFilter(
				Definition.Filter, Transition))
		{
			continue;
		}
		if (!Definition.bMandatory)
		{
			OutReason =
				TEXT("optional_card_zone_transition_trigger_unsupported");
			return false;
		}
		if (Definition.TriggerId.IsEmpty() || Definition.DrawCount < 0)
		{
			OutReason = TEXT("card_zone_transition_trigger_invalid");
			return false;
		}

		FWBCardZoneTransitionTriggerInstance Trigger;
		Trigger.TransitionSnapshot = Transition;
		Trigger.SourceSnapshot = SourceBase;
		Trigger.SourceSnapshot.TriggerId = Definition.TriggerId;
		Trigger.Definition = Definition;
		Trigger.CollectionOrder = OutTriggers.Num();
		Trigger.StableTriggerId = BuildStableTriggerId(
			Transition, Trigger.SourceSnapshot, Trigger.CollectionOrder);
		OutTriggers.Add(MoveTemp(Trigger));
	}
	return true;
}

bool CollectForTransition(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBCardZoneTransitionSnapshot& Transition,
	TArray<FWBCardZoneTransitionTriggerInstance>& OutTriggers,
	FString& OutReason)
{
	OutTriggers.Reset();
	if (!Transition.IsValid())
	{
		OutReason = TEXT("card_zone_transition_trigger_snapshot_invalid");
		return false;
	}

	const FWBCardDefinitionRepositoryLookupResult MovedLookup =
		WBCardDefinitionRepository::FindCardById(
			Repository, Transition.CardId);
	if (MovedLookup.bFound)
	{
		FWBCardZoneTransitionTriggerSourceSnapshot SelfSource;
		SelfSource.SourceCardInstanceId = Transition.CardInstanceId;
		SelfSource.SourceCardId = Transition.CardId;
		SelfSource.OwnerPlayerId = Transition.OwnerPlayerId;
		SelfSource.SourceScope =
			EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf;
		if (!AddMatchingDefinitions(
			MovedLookup.Definition,
			Transition,
			SelfSource,
			OutTriggers,
			OutReason))
		{
			return false;
		}
	}

	const FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindPlayerZones(
			State.GetCardZoneState(), Transition.OwnerPlayerId);
	if (PlayerZones == nullptr)
	{
		OutReason = TEXT("card_zone_transition_trigger_owner_zones_missing");
		return false;
	}

	TArray<FWBZoneCardEntry> Discard = PlayerZones->Discard;
	Discard.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
	{
		if (A.ZoneIndex != B.ZoneIndex)
		{
			return A.ZoneIndex < B.ZoneIndex;
		}
		return A.Card.InstanceId < B.Card.InstanceId;
	});
	for (const FWBZoneCardEntry& Entry : Discard)
	{
		if (Entry.Card.OwnerPlayerId != Transition.OwnerPlayerId
			|| Entry.Zone != EWBCardZone::Discard)
		{
			continue;
		}
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Entry.Card.CardId);
		if (!Lookup.bFound)
		{
			continue;
		}

		FWBCardZoneTransitionTriggerSourceSnapshot ObserverSource;
		ObserverSource.SourceCardInstanceId = Entry.Card.InstanceId;
		ObserverSource.SourceCardId = Entry.Card.CardId;
		ObserverSource.OwnerPlayerId = Entry.Card.OwnerPlayerId;
		ObserverSource.ResidenceZoneAtCollection = EWBCardZone::Discard;
		ObserverSource.ZoneIndexAtCollection = Entry.ZoneIndex;
		ObserverSource.SourceScope =
			EWBCardZoneTransitionTriggerSourceScope::
				ResidentDiscardObserver;
		if (!AddMatchingDefinitions(
			Lookup.Definition,
			Transition,
			ObserverSource,
			OutTriggers,
			OutReason))
		{
			return false;
		}
	}
	return true;
}
}

bool FWBCardZoneTransitionTriggerSourceSnapshot::IsValid() const
{
	return !SourceCardInstanceId.IsEmpty()
		&& !SourceCardId.IsEmpty()
		&& FWBGameStateData::IsValidPlayerId(OwnerPlayerId)
		&& IsSupportedSourceScope(SourceScope)
		&& !TriggerId.IsEmpty()
		&& EligibilityPolicy ==
			EWBTriggerEligibilityPolicy::SnapshotAtCollection
		&& (SourceScope != EWBCardZoneTransitionTriggerSourceScope::
				ResidentDiscardObserver
			|| (ResidenceZoneAtCollection == EWBCardZone::Discard
				&& ZoneIndexAtCollection >= 0));
}

bool WBCardZoneTransitionTrigger::MatchesFilter(
	const FWBCardZoneTransitionTriggerFilter& Filter,
	const FWBCardZoneTransitionSnapshot& Snapshot)
{
	return Snapshot.IsValid()
		&& (!Filter.bRequireSourceZone
			|| Snapshot.SourceZone == Filter.RequiredSourceZone)
		&& (!Filter.bRequireDestinationZone
			|| Snapshot.DestinationZone ==
				Filter.RequiredDestinationZone)
		&& (!Filter.bRequireCause
			|| Snapshot.Cause == Filter.RequiredCause);
}

FWBCardZoneTransitionTriggerResult
WBCardZoneTransitionTrigger::ResolveCommittedTransitions(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const TArray<FWBCardZoneTransitionSnapshot>& Transitions,
	const int32 ResolutionGuard)
{
	FWBCardZoneTransitionTriggerResult Result;
	if (ResolutionGuard <= 0)
	{
		Result.Reason = TEXT("card_zone_transition_trigger_guard_invalid");
		return Result;
	}
	if (State.bGameOver || Transitions.IsEmpty())
	{
		Result.bOk = true;
		return Result;
	}
	const FWBCardDefinitionRepositoryValidationResult RepositoryValidation =
		WBCardDefinitionRepository::ValidateRepository(Repository);
	if (!RepositoryValidation.bOk)
	{
		Result.Reason = RepositoryValidation.Reason;
		return Result;
	}

	FWBGameStateData WorkingState = State;
	auto FailClosed = [&Result]()
	{
		Result.ProcessedTransitionCount = 0;
		Result.ResolvedTriggerCount = 0;
		Result.DrawnCardCount = 0;
		Result.CollectedTriggers.Reset();
		Result.NestedTransitionEvents.Reset();
		Result.TraceEvents.Reset();
		return Result;
	};
	TArray<FWBCardZoneTransitionSnapshot> Queue = Transitions;
	Queue.StableSort([](
		const FWBCardZoneTransitionSnapshot& A,
		const FWBCardZoneTransitionSnapshot& B)
	{
		return A.ResolutionOrder < B.ResolutionOrder;
	});
	int32 QueueIndex = 0;
	int32 GuardCount = 0;
	while (QueueIndex < Queue.Num())
	{
		if (WorkingState.bGameOver)
		{
			break;
		}
		if (++GuardCount > ResolutionGuard)
		{
			Result.Reason =
				TEXT("card_zone_transition_trigger_guard_exceeded");
			return FailClosed();
		}

		const FWBCardZoneTransitionSnapshot Transition = Queue[QueueIndex++];
		++Result.ProcessedTransitionCount;
		TArray<FWBCardZoneTransitionTriggerInstance> Triggers;
		if (!CollectForTransition(
			WorkingState, Repository, Transition, Triggers, Result.Reason))
		{
			return FailClosed();
		}

		for (FWBCardZoneTransitionTriggerInstance& Trigger : Triggers)
		{
			if (++GuardCount > ResolutionGuard)
			{
				Result.Reason =
					TEXT("card_zone_transition_trigger_guard_exceeded");
				return FailClosed();
			}
			if (!Trigger.SourceSnapshot.IsValid())
			{
				Result.Reason =
					TEXT("card_zone_transition_trigger_source_invalid");
				return FailClosed();
			}

			Result.CollectedTriggers.Add(Trigger);
			Result.TraceEvents.Add(MakeRedactedTriggerTrace(
				FName(TEXT("card_zone_transition_triggered")),
				Trigger,
				Trigger.Definition.DrawCount));
			for (int32 DrawIndex = 0;
				DrawIndex < Trigger.Definition.DrawCount;
				++DrawIndex)
			{
				FWBCardZoneTransitionContext Context;
				Context.Cause = EWBCardZoneTransitionCause::Effect;
				Context.SourceActionId = Trigger.StableTriggerId;
				Context.ContinuationId =
					Trigger.TransitionSnapshot.EventIdentity.EventId;
				Context.ResolutionOrder = DrawIndex;
				const FWBCardLifecycleResult Draw =
					WBCardLifecycle::DrawOneCard(
						WorkingState,
						Trigger.SourceSnapshot.OwnerPlayerId,
						Context);
				if (!Draw.bOk || Draw.TransitionEvents.Num() != 1)
				{
					Result.Reason = Draw.bOk
						? TEXT("card_zone_transition_trigger_draw_event_missing")
						: Draw.Reason;
					return FailClosed();
				}
				const FWBCardZoneTransitionSnapshot& Nested =
					Draw.TransitionEvents[0];
				Result.NestedTransitionEvents.Add(Nested);
				Queue.Add(Nested);
				WBCardZoneTransition::AppendRedactedTraces(
					Draw.TransitionEvents, Result.TraceEvents);
				Result.TraceEvents.Add(MakeRedactedTriggerTrace(
					FName(TEXT("card_zone_transition_trigger_card_drawn")),
					Trigger,
					1));
				++Result.DrawnCardCount;
			}
			Result.TraceEvents.Add(MakeRedactedTriggerTrace(
				FName(TEXT("card_zone_transition_trigger_resolved")),
				Trigger,
				Trigger.Definition.DrawCount));
			++Result.ResolvedTriggerCount;
		}
	}

	State = MoveTemp(WorkingState);
	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}
