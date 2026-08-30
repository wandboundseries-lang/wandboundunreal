#include "WBPostDestructionTrigger.h"

#include "WBCardZoneState.h"
#include "WBDeckSummon.h"
#include "WBUnitStatDelta.h"

namespace
{
constexpr int32 PostDestructionBoardSize = 9;
constexpr int32 PostDestructionMaxOwnedUnitsIncludingHero = 4;

FWBPostDestructionTriggerResult MakePostDestructionFailure(const FString& Reason)
{
	FWBPostDestructionTriggerResult Result;
	Result.Reason = Reason;
	return Result;
}

FWBTraceEvent MakeTriggerTrace(
	const FName Kind,
	const FWBUnitDestructionSnapshot& Event,
	const FString& TriggerId,
	const FString& Reason = FString())
{
	FWBTraceEvent Trace;
	Trace.Kind = Kind;
	Trace.ActionId = Event.EventId + TEXT(":") + TriggerId;
	Trace.PlayerId = Event.ControllerPlayerId;
	Trace.SourceUnitId = Event.DestroyedUnitId;
	Trace.FromTile = Event.LastTile;
	Trace.ResolutionOrder = Event.ResolutionOrder;
	Trace.DamageCause = FName(*FString::FromInt(
		static_cast<int32>(Event.Cause)));
	Trace.Reason = Reason;
	Trace.bOk = true;
	return Trace;
}

TArray<FWBAfterUnitDestroyedTriggerDefinition> SortedSelfTriggers(
	const FWBCardDefinition& Definition)
{
	TArray<FWBAfterUnitDestroyedTriggerDefinition> Triggers;
	for (const FWBAfterUnitDestroyedTriggerDefinition& Trigger :
		Definition.AfterUnitDestroyedTriggers)
	{
		if (Trigger.SourceScope
			== EWBAfterUnitDestroyedSourceScope::DestroyedSelf)
		{
			Triggers.Add(Trigger);
		}
	}
	Triggers.Sort([](
		const FWBAfterUnitDestroyedTriggerDefinition& A,
		const FWBAfterUnitDestroyedTriggerDefinition& B)
	{
		return A.TriggerId < B.TriggerId;
	});
	return Triggers;
}

struct FResolvedObserverTrigger
{
	FWBPostDestructionObserverSourceSnapshot Source;
	FWBAfterUnitDestroyedTriggerDefinition Trigger;
};

TArray<FResolvedObserverTrigger> BuildObserverTriggers(
	const FWBUnitDestructionSnapshot& Event,
	const FWBCardDefinitionRepository& Repository)
{
	TArray<FResolvedObserverTrigger> Resolved;
	for (const FWBPostDestructionObserverSourceSnapshot& Source :
		Event.ObserverSources)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Source.SourceCardId);
		if (!Lookup.bFound)
		{
			continue;
		}
		for (const FWBAfterUnitDestroyedTriggerDefinition& Trigger :
			Lookup.Definition.AfterUnitDestroyedTriggers)
		{
			if (Trigger.SourceScope
				!= EWBAfterUnitDestroyedSourceScope::
					ControlledFactionUnitDestroyed)
			{
				continue;
			}
			FResolvedObserverTrigger Entry;
			Entry.Source = Source;
			Entry.Trigger = Trigger;
			Resolved.Add(MoveTemp(Entry));
		}
	}
	Resolved.Sort([](
		const FResolvedObserverTrigger& A,
		const FResolvedObserverTrigger& B)
	{
		if (A.Source.ControllerPlayerId != B.Source.ControllerPlayerId)
		{
			return A.Source.ControllerPlayerId < B.Source.ControllerPlayerId;
		}
		if (A.Source.SourceUnitId != B.Source.SourceUnitId)
		{
			return A.Source.SourceUnitId < B.Source.SourceUnitId;
		}
		return A.Trigger.TriggerId < B.Trigger.TriggerId;
	});
	return Resolved;
}

FWBTraceEvent MakeObserverTrace(
	const FName Kind,
	const FWBUnitDestructionSnapshot& Event,
	const FResolvedObserverTrigger& Observer,
	const FString& Reason = FString())
{
	FWBTraceEvent Trace;
	Trace.Kind = Kind;
	Trace.ActionId = FString::Printf(
		TEXT("%s:observer:u%d:%s"),
		*Event.EventId,
		Observer.Source.SourceUnitId,
		*Observer.Trigger.TriggerId);
	Trace.PlayerId = Observer.Source.ControllerPlayerId;
	Trace.SourceUnitId = Observer.Source.SourceUnitId;
	Trace.TargetUnitId = Observer.Source.SourceUnitId;
	Trace.PreviousTargetUnitId = Event.DestroyedUnitId;
	Trace.FromTile = Event.LastTile;
	Trace.ResolutionOrder = Event.ResolutionOrder;
	Trace.DamageCause = FName(*FString::FromInt(
		static_cast<int32>(Event.Cause)));
	Trace.Reason = Reason;
	Trace.bOk = true;
	return Trace;
}

TArray<FWBZoneCardEntry> EligibleDeckEntries(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 PlayerId,
	const FString& RequiredFaction)
{
	TArray<FWBZoneCardEntry> Eligible;
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), PlayerId);
	if (Zones == nullptr) return Eligible;
	for (const FWBZoneCardEntry& Entry : Zones->Deck)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Entry.Card.CardId);
		if (Lookup.bFound
			&& Lookup.Definition.Kind == EWBCardDefinitionKind::Character
			&& (RequiredFaction.IsEmpty()
				|| Lookup.Definition.PublicFactions.Contains(RequiredFaction)))
		{
			Eligible.Add(Entry);
		}
	}
	Eligible.Sort([](const FWBZoneCardEntry& A, const FWBZoneCardEntry& B)
	{
		if (A.ZoneIndex != B.ZoneIndex) return A.ZoneIndex < B.ZoneIndex;
		return A.Card.InstanceId < B.Card.InstanceId;
	});
	return Eligible;
}

bool CanAttemptSummon(
	const FWBGameStateData& State,
	const FWBUnitDestructionSnapshot& Event,
	FString& OutReason)
{
	if (Event.LastTile.X < 0 || Event.LastTile.X >= PostDestructionBoardSize
		|| Event.LastTile.Y < 0 || Event.LastTile.Y >= PostDestructionBoardSize)
	{
		OutReason = TEXT("post_destruction_tile_out_of_bounds");
		return false;
	}
	if (State.IsTileOccupied(Event.LastTile))
	{
		OutReason = TEXT("post_destruction_tile_occupied");
		return false;
	}
	if (State.GetUnitsForPlayer(Event.ControllerPlayerId).Num()
		>= PostDestructionMaxOwnedUnitsIncludingHero)
	{
		OutReason = TEXT("unit_cap_reached");
		return false;
	}
	OutReason.Reset();
	return true;
}
}

FString WBPostDestructionTrigger::BuildChoiceActionId(
	const FWBPendingMandatoryDeckChoiceState& Choice,
	const FString& CardInstanceId)
{
	return FString::Printf(
		TEXT("mandatory_deck_choice:p%d:c%s:i%s"),
		Choice.ControllerPlayerId,
		*Choice.ChoiceId,
		*CardInstanceId);
}

FWBPostDestructionTriggerResult
WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ResumePriorityPlayerId,
	const int32 ResumeMatchPhase)
{
	FWBPostDestructionTriggerResult Result;
	if (State.HasPendingMandatoryDeckChoice())
	{
		Result.bOk = true;
		Result.bPendingChoice = true;
		return Result;
	}
	if (State.bGameOver)
	{
		State.PendingUnitDestructionEvents.Reset();
		State.ClearPendingMandatoryDeckChoice();
		Result.bOk = true;
		return Result;
	}

	int32 Guard = 0;
	while (!State.PendingUnitDestructionEvents.IsEmpty())
	{
		if (++Guard > 128)
		{
			return MakePostDestructionFailure(TEXT("post_destruction_trigger_guard_exceeded"));
		}
		FWBUnitDestructionSnapshot& Event =
			State.PendingUnitDestructionEvents[0];
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Event.DestroyedCardId);
		if (!Lookup.bFound)
		{
			return MakePostDestructionFailure(TEXT("destroyed_unit_definition_missing"));
		}
		const TArray<FWBAfterUnitDestroyedTriggerDefinition> Triggers =
			SortedSelfTriggers(Lookup.Definition);
		if (Event.NextTriggerIndex >= Triggers.Num())
		{
			const TArray<FResolvedObserverTrigger> Observers =
				BuildObserverTriggers(Event, Repository);
			if (Event.NextObserverTriggerIndex >= Observers.Num())
			{
				State.PendingUnitDestructionEvents.RemoveAt(
					0, 1, EAllowShrinking::No);
				continue;
			}

			const FResolvedObserverTrigger Observer =
				Observers[Event.NextObserverTriggerIndex++];
			const FWBUnitDestructionSnapshot EventSnapshot = Event;
			if (Observer.Source.ControllerPlayerId
				!= EventSnapshot.ControllerPlayerId
				|| Observer.Trigger.RequiredFaction.IsEmpty()
				|| !Lookup.Definition.PublicFactions.Contains(
					Observer.Trigger.RequiredFaction))
			{
				continue;
			}
			if (!Observer.Trigger.bMandatory
				|| Observer.Trigger.Operation
					!= EWBPostDestructionEffectOperation::
						ApplyPersistentStatDeltaToTriggerSource
				|| Observer.Trigger.Target
					!= EWBPostDestructionTarget::TriggerSource)
			{
				return MakePostDestructionFailure(
					TEXT("unsupported_post_destruction_observer_trigger"));
			}

			Result.TraceEvents.Add(MakeObserverTrace(
				FName(TEXT("post_destruction_observer_triggered")),
				EventSnapshot,
				Observer));
			const FWBUnitState* LiveSource = State.GetUnitById(
				Observer.Source.SourceUnitId);
			if (LiveSource == nullptr
				|| !LiveSource->IsUnitOnBoard()
				|| LiveSource->bDefeated
				|| LiveSource->GetControllerPlayerIdForRules()
					!= Observer.Source.ControllerPlayerId
				|| LiveSource->CardId != Observer.Source.SourceCardId)
			{
				Result.TraceEvents.Add(MakeObserverTrace(
					FName(TEXT("post_destruction_observer_skipped")),
					EventSnapshot,
					Observer,
					TEXT("observer_source_unavailable")));
				continue;
			}

			FWBUnitStatDeltaRequest Delta;
			Delta.SourceUnitId = Observer.Source.SourceUnitId;
			Delta.TargetUnitId = Observer.Source.SourceUnitId;
			Delta.ATKDelta = Observer.Trigger.StatDelta.ATKDelta;
			Delta.MaxHPDelta = Observer.Trigger.StatDelta.MaxHPDelta;
			Delta.CurrentHPDelta = Observer.Trigger.StatDelta.CurrentHPDelta;
			Delta.TransactionId = MakeObserverTrace(
				NAME_None, EventSnapshot, Observer).ActionId;
			const FWBUnitStatDeltaResult Applied =
				WBUnitStatDelta::ApplyPersistentDelta(State, Delta);
			if (!Applied.bOk)
			{
				Result.TraceEvents.Add(MakeObserverTrace(
					FName(TEXT("post_destruction_observer_failed")),
					EventSnapshot,
					Observer,
					Applied.Reason));
				continue;
			}
			Result.TraceEvents.Append(Applied.TraceEvents);
			Result.TraceEvents.Add(MakeObserverTrace(
				FName(TEXT("post_destruction_observer_resolved")),
				EventSnapshot,
				Observer));
			continue;
		}

		const FWBAfterUnitDestroyedTriggerDefinition Trigger =
			Triggers[Event.NextTriggerIndex++];
		if (!Event.bCharacterPassiveEligible)
		{
			Result.TraceEvents.Add(MakeTriggerTrace(
				FName(TEXT("post_destruction_trigger_suppressed")),
				Event,
				Trigger.TriggerId,
				TEXT("character_passive_suppressed")));
			continue;
		}
		if (!Trigger.bMandatory
			|| Trigger.SourceScope
				!= EWBAfterUnitDestroyedSourceScope::DestroyedSelf
			|| Trigger.Operation
				!= EWBPostDestructionEffectOperation::
					SummonCharacterFromDeckToDestroyedTile
			|| Trigger.SummonCount != 1
			|| !Trigger.bIgnoreSummoningConditions
			|| !Trigger.bApplyCSNInheritance)
		{
			return MakePostDestructionFailure(TEXT("unsupported_post_destruction_trigger"));
		}

		Result.TraceEvents.Add(MakeTriggerTrace(
			FName(TEXT("post_destruction_triggered")),
			Event,
			Trigger.TriggerId));
		FString NoSummonReason;
		const TArray<FWBZoneCardEntry> Eligible = EligibleDeckEntries(
			State,
			Repository,
			Event.ControllerPlayerId,
			Trigger.RequiredFaction);
		if (!CanAttemptSummon(State, Event, NoSummonReason)
			|| Eligible.IsEmpty())
		{
			if (NoSummonReason.IsEmpty())
			{
				NoSummonReason = TEXT("no_eligible_deck_character");
			}
			Result.TraceEvents.Add(MakeTriggerTrace(
				FName(TEXT("post_destruction_trigger_resolved")),
				Event,
				Trigger.TriggerId,
				NoSummonReason));
			continue;
		}

		FWBPendingMandatoryDeckChoiceState Choice;
		Choice.bActive = true;
		Choice.Origin = EWBMandatoryDeckChoiceOrigin::PostDestructionTrigger;
		Choice.ChoiceId = Event.EventId + TEXT(":") + Trigger.TriggerId;
		Choice.DestructionEventId = Event.EventId;
		Choice.TriggerId = Trigger.TriggerId;
		Choice.ControllerPlayerId = Event.ControllerPlayerId;
		Choice.RequiredFaction = Trigger.RequiredFaction;
		Choice.DestinationTile = Event.LastTile;
		Choice.SourceSnapshot = Event;
		Choice.bApplyCSNInheritance = Trigger.bApplyCSNInheritance;
		Choice.ResumePriorityPlayerId = ResumePriorityPlayerId;
		Choice.ResumeMatchPhase = ResumeMatchPhase;
		for (const FWBZoneCardEntry& Entry : Eligible)
		{
			Choice.EligibleCardInstanceIds.Add(Entry.Card.InstanceId);
		}
		State.PendingMandatoryDeckChoice = MoveTemp(Choice);
		Result.bOk = true;
		Result.bPendingChoice = true;
		return Result;
	}

	Result.bOk = true;
	return Result;
}

TArray<FString> WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	TArray<FString> ActionIds;
	if (!State.HasPendingMandatoryDeckChoice()
		|| State.PendingUnitDestructionEvents.IsEmpty())
	{
		return ActionIds;
	}
	const FWBPendingMandatoryDeckChoiceState& Choice =
		State.PendingMandatoryDeckChoice;
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), Choice.ControllerPlayerId);
	if (Zones == nullptr) return ActionIds;
	for (const FString& InstanceId : Choice.EligibleCardInstanceIds)
	{
		const FWBZoneCardEntry* Entry = Zones->Deck.FindByPredicate(
			[&InstanceId](const FWBZoneCardEntry& Candidate)
			{
				return Candidate.Card.InstanceId == InstanceId;
			});
		if (Entry != nullptr
			&& WBCardDefinitionRepository::FindCardById(
				Repository, Entry->Card.CardId).bFound)
		{
			ActionIds.Add(BuildChoiceActionId(Choice, InstanceId));
		}
	}
	return ActionIds;
}

FWBPostDestructionTriggerResult WBPostDestructionTrigger::SubmitChoice(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& ActionId)
{
	if (!State.HasPendingMandatoryDeckChoice()
		|| State.PendingUnitDestructionEvents.IsEmpty())
	{
		return MakePostDestructionFailure(TEXT("mandatory_deck_choice_missing"));
	}
	const FWBPendingMandatoryDeckChoiceState Choice =
		State.PendingMandatoryDeckChoice;
	const FString* SelectedInstance = Choice.EligibleCardInstanceIds.FindByPredicate(
		[&Choice, &ActionId](const FString& InstanceId)
		{
			return BuildChoiceActionId(Choice, InstanceId) == ActionId;
		});
	if (SelectedInstance == nullptr)
	{
		return MakePostDestructionFailure(TEXT("mandatory_deck_choice_illegal"));
	}
	const FWBUnitDestructionSnapshot Event =
		State.PendingUnitDestructionEvents[0];
	if (Event.EventId != Choice.DestructionEventId)
	{
		return MakePostDestructionFailure(TEXT("mandatory_deck_choice_stale"));
	}
	const FWBCardDefinitionRepositoryLookupResult SourceLookup =
		WBCardDefinitionRepository::FindCardById(
			Repository, Event.DestroyedCardId);
	if (!SourceLookup.bFound)
	{
		return MakePostDestructionFailure(TEXT("destroyed_unit_definition_missing"));
	}
	const TArray<FWBAfterUnitDestroyedTriggerDefinition> Triggers =
		SortedSelfTriggers(SourceLookup.Definition);
	const FWBAfterUnitDestroyedTriggerDefinition* Trigger =
		Triggers.FindByPredicate(
			[&Choice](const FWBAfterUnitDestroyedTriggerDefinition& Candidate)
			{
				return Candidate.TriggerId == Choice.TriggerId;
			});
	if (Trigger == nullptr)
	{
		return MakePostDestructionFailure(TEXT("post_destruction_trigger_missing"));
	}

	FWBDeckSummonRequest Request;
	Request.PlayerId = Choice.ControllerPlayerId;
	Request.SelectedCardInstanceId = *SelectedInstance;
	Request.RequiredFaction = Trigger->RequiredFaction;
	Request.TargetTile = Event.LastTile;
	Request.InheritanceSource.SourceUnitId = Event.DestroyedUnitId;
	Request.InheritanceSource.SourceCurrentRL = Event.CurrentRLSnapshot;
	Request.InheritanceSource.EquippedWands = Event.EquippedWands;
	Request.TransactionId = Choice.ChoiceId;
	const FWBDeckSummonResult Summon = WBDeckSummon::SummonExactCharacterToTile(
		State, Repository, Request);

	FWBPostDestructionTriggerResult Result;
	Result.bOk = true;
	Result.bSummoned = Summon.bOk;
	FWBTraceEvent DeclaredTarget;
	DeclaredTarget.Kind = FName(TEXT("mandatory_deck_target_declared"));
	DeclaredTarget.ActionId = ActionId;
	DeclaredTarget.PlayerId = Choice.ControllerPlayerId;
	DeclaredTarget.SourceUnitId = Event.DestroyedUnitId;
	DeclaredTarget.CardInstanceId = *SelectedInstance;
	DeclaredTarget.bDeclaredTarget = true;
	DeclaredTarget.bOk = true;
	Result.TraceEvents.Add(MoveTemp(DeclaredTarget));
	Result.TraceEvents.Append(Summon.TraceEvents);
	Result.TraceEvents.Add(MakeTriggerTrace(
		Summon.bOk
			? FName(TEXT("post_destruction_trigger_resolved"))
			: FName(TEXT("post_destruction_trigger_failed")),
		Event,
		Choice.TriggerId,
		Summon.bOk ? FString() : Summon.Reason));
	State.ClearPendingMandatoryDeckChoice();

	const FWBPostDestructionTriggerResult Continued =
		AdvanceToDecisionOrComplete(
			State,
			Repository,
			Choice.ResumePriorityPlayerId,
			Choice.ResumeMatchPhase);
	if (!Continued.bOk)
	{
		return Continued;
	}
	Result.bPendingChoice = Continued.bPendingChoice;
	Result.TraceEvents.Append(Continued.TraceEvents);
	return Result;
}
