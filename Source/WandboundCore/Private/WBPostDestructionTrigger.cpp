#include "WBPostDestructionTrigger.h"

#include "WBCardZoneState.h"
#include "WBDeckSummon.h"
#include "WBMandatoryDeckChoice.h"
#include "WBPrivateCardChoice.h"
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
				Repository, Source.SourceSnapshot.SourceCardId);
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
		if (A.Source.SourceSnapshot.ControllerPlayerId
			!= B.Source.SourceSnapshot.ControllerPlayerId)
		{
			return A.Source.SourceSnapshot.ControllerPlayerId
				< B.Source.SourceSnapshot.ControllerPlayerId;
		}
		if (A.Source.SourceSnapshot.SourceUnitId
			!= B.Source.SourceSnapshot.SourceUnitId)
		{
			return A.Source.SourceSnapshot.SourceUnitId
				< B.Source.SourceSnapshot.SourceUnitId;
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
		Observer.Source.SourceSnapshot.SourceUnitId,
		*Observer.Trigger.TriggerId);
	Trace.PlayerId = Observer.Source.SourceSnapshot.ControllerPlayerId;
	Trace.SourceUnitId = Observer.Source.SourceSnapshot.SourceUnitId;
	Trace.TargetUnitId = Observer.Source.SourceSnapshot.SourceUnitId;
	Trace.PreviousTargetUnitId = Event.DestroyedUnitId;
	Trace.FromTile = Event.LastTile;
	Trace.ResolutionOrder = Event.ResolutionOrder;
	Trace.DamageCause = FName(*FString::FromInt(
		static_cast<int32>(Event.Cause)));
	Trace.Reason = Reason;
	Trace.bOk = true;
	return Trace;
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
		Choice.Descriptor.ChoosingPlayerId,
		*Choice.Descriptor.ChoiceId,
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
			if (Observer.Source.SourceSnapshot.ControllerPlayerId
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
				Observer.Source.SourceSnapshot.SourceUnitId);
			if (LiveSource == nullptr
				|| !LiveSource->IsUnitOnBoard()
				|| LiveSource->bDefeated
				|| LiveSource->GetControllerPlayerIdForRules()
					!= Observer.Source.SourceSnapshot.ControllerPlayerId
				|| LiveSource->CardId
					!= Observer.Source.SourceSnapshot.SourceCardId)
			{
				Result.TraceEvents.Add(MakeObserverTrace(
					FName(TEXT("post_destruction_observer_skipped")),
					EventSnapshot,
					Observer,
					TEXT("observer_source_unavailable")));
				continue;
			}

			FWBUnitStatDeltaRequest Delta;
			Delta.SourceUnitId = Observer.Source.SourceSnapshot.SourceUnitId;
			Delta.TargetUnitId = Observer.Source.SourceSnapshot.SourceUnitId;
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
		FWBPrivateCardChoiceDescriptor Descriptor;
		Descriptor.ChoiceId = Event.EventId + TEXT(":") + Trigger.TriggerId;
		Descriptor.ChoosingPlayerId = Event.ControllerPlayerId;
		Descriptor.SourceZone = EWBCardZone::Deck;
		Descriptor.Timing = EWBPrivateCardChoiceTiming::ResolutionContinuation;
		Descriptor.Requirement = EWBPrivateCardChoiceRequirement::Mandatory;
		Descriptor.TargetDeclaration = EWBDeclarationProvenance::PlayerDeclared;
		Descriptor.ContinuationKind =
			EWBPrivateCardChoiceContinuationKind::PostDestructionTrigger;
		Descriptor.Filter.RequiredKind = EWBCardDefinitionKind::Character;
		Descriptor.Filter.RequiredFaction = Trigger.RequiredFaction;
		Descriptor.ResumePriorityPlayerId = ResumePriorityPlayerId;
		Descriptor.ResumeMatchPhase = ResumeMatchPhase;
		const FWBPrivateCardChoiceCandidateResult Eligible =
			WBPrivateCardChoice::FreezeCandidates(State, Repository, Descriptor);
		if (!Eligible.bOk)
		{
			return MakePostDestructionFailure(Eligible.Reason);
		}
		if (!CanAttemptSummon(State, Event, NoSummonReason)
			|| Eligible.Candidates.IsEmpty())
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

		FWBPendingPrivateCardChoiceState Choice;
		Choice.bActive = true;
		Choice.Descriptor = MoveTemp(Descriptor);
		Choice.PostDestruction.DestructionEventId = Event.EventId;
		Choice.PostDestruction.TriggerId = Trigger.TriggerId;
		Choice.PostDestruction.DestinationTile = Event.LastTile;
		Choice.PostDestruction.SourceSnapshot = Event;
		Choice.PostDestruction.bApplyCSNInheritance =
			Trigger.bApplyCSNInheritance;
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
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId)
{
	return WBMandatoryDeckChoice::EnumerateLegalActionIds(
		State, Repository, ViewerPlayerId);
}

FWBPostDestructionTriggerResult WBPostDestructionTrigger::SubmitChoice(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& ActionId)
{
	const FWBMandatoryDeckChoiceResult Generic =
		WBMandatoryDeckChoice::Submit(State, Repository, ActionId);
	FWBPostDestructionTriggerResult Result;
	Result.bOk = Generic.bOk;
	Result.Reason = Generic.Reason;
	Result.bPendingChoice = Generic.bPendingChoice;
	Result.bSummoned = Generic.bSummoned;
	Result.TraceEvents = Generic.TraceEvents;
	return Result;
}

FWBPostDestructionTriggerResult WBPostDestructionTrigger::ResolveSelectedChoice(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& SelectedCardInstanceId,
	const FString& ActionId)
{
	if (!State.HasPendingMandatoryDeckChoice()
		|| State.PendingUnitDestructionEvents.IsEmpty())
	{
		return MakePostDestructionFailure(TEXT("mandatory_deck_choice_missing"));
	}
	const FWBPendingPrivateCardChoiceState Choice = State.PendingMandatoryDeckChoice;
	const FWBUnitDestructionSnapshot Event =
		State.PendingUnitDestructionEvents[0];
	if (Choice.Descriptor.ContinuationKind
			!= EWBPrivateCardChoiceContinuationKind::PostDestructionTrigger
		|| Event.EventId != Choice.PostDestruction.DestructionEventId)
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
				return Candidate.TriggerId == Choice.PostDestruction.TriggerId;
			});
	if (Trigger == nullptr)
	{
		return MakePostDestructionFailure(TEXT("post_destruction_trigger_missing"));
	}

	FWBDeckSummonRequest Request;
	Request.PlayerId = Choice.Descriptor.ChoosingPlayerId;
	Request.SelectedCardInstanceId = SelectedCardInstanceId;
	Request.RequiredFaction = Trigger->RequiredFaction;
	Request.TargetTile = Event.LastTile;
	Request.InheritanceSource.SourceSnapshot =
		Event.DestroyedUnitSnapshot;
	Request.InheritanceSource.SourceUnitId = Event.DestroyedUnitId;
	Request.InheritanceSource.SourceCurrentRL = Event.CurrentRLSnapshot;
	Request.InheritanceSource.EquippedWands = Event.EquippedWands;
	Request.TransactionId = Choice.Descriptor.ChoiceId;
	const FWBDeckSummonResult Summon = WBDeckSummon::SummonExactCharacterToTile(
		State, Repository, Request);

	FWBPostDestructionTriggerResult Result;
	Result.bOk = true;
	Result.bSummoned = Summon.bOk;
	FWBTraceEvent DeclaredTarget;
	DeclaredTarget.Kind = FName(TEXT("mandatory_deck_target_declared"));
	DeclaredTarget.ActionId = ActionId;
	DeclaredTarget.PlayerId = Choice.Descriptor.ChoosingPlayerId;
	DeclaredTarget.SourceUnitId = Event.DestroyedUnitId;
	DeclaredTarget.CardInstanceId = SelectedCardInstanceId;
	DeclaredTarget.bDeclaredTarget = WBIsPlayerDeclared(
		Choice.Descriptor.TargetDeclaration);
	DeclaredTarget.bOk = true;
	Result.TraceEvents.Add(MoveTemp(DeclaredTarget));
	Result.TraceEvents.Append(Summon.TraceEvents);
	Result.TraceEvents.Add(MakeTriggerTrace(
		Summon.bOk
			? FName(TEXT("post_destruction_trigger_resolved"))
			: FName(TEXT("post_destruction_trigger_failed")),
		Event,
		Choice.PostDestruction.TriggerId,
		Summon.bOk ? FString() : Summon.Reason));
	State.ClearPendingMandatoryDeckChoice();

	const FWBPostDestructionTriggerResult Continued =
		AdvanceToDecisionOrComplete(
			State,
			Repository,
			Choice.Descriptor.ResumePriorityPlayerId,
			Choice.Descriptor.ResumeMatchPhase);
	if (!Continued.bOk)
	{
		return Continued;
	}
	Result.bPendingChoice = Continued.bPendingChoice;
	Result.TraceEvents.Append(Continued.TraceEvents);
	return Result;
}
