#include "WBPostDestructionTrigger.h"

#include "WBCardZoneState.h"
#include "WBDeckSummon.h"

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

TArray<FWBAfterUnitDestroyedTriggerDefinition> SortedTriggers(
	const FWBCardDefinition& Definition)
{
	TArray<FWBAfterUnitDestroyedTriggerDefinition> Triggers =
		Definition.AfterUnitDestroyedTriggers;
	Triggers.Sort([](
		const FWBAfterUnitDestroyedTriggerDefinition& A,
		const FWBAfterUnitDestroyedTriggerDefinition& B)
	{
		return A.TriggerId < B.TriggerId;
	});
	return Triggers;
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
			SortedTriggers(Lookup.Definition);
		if (Event.NextTriggerIndex >= Triggers.Num())
		{
			State.PendingUnitDestructionEvents.RemoveAt(
				0, 1, EAllowShrinking::No);
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
		Choice.ChoiceId = Event.EventId + TEXT(":") + Trigger.TriggerId;
		Choice.DestructionEventId = Event.EventId;
		Choice.TriggerId = Trigger.TriggerId;
		Choice.ControllerPlayerId = Event.ControllerPlayerId;
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
		SortedTriggers(SourceLookup.Definition);
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
	Request.InheritanceSource = Event;
	Request.TransactionId = Choice.ChoiceId;
	const FWBDeckSummonResult Summon = WBDeckSummon::SummonExactCharacterToTile(
		State, Repository, Request);

	FWBPostDestructionTriggerResult Result;
	Result.bOk = true;
	Result.bSummoned = Summon.bOk;
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
