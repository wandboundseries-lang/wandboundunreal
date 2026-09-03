#include "WBMandatoryDeckChoice.h"

#include "WBCardZoneState.h"
#include "WBDeckSummon.h"
#include "WBPostDestructionTrigger.h"

namespace
{
FWBMandatoryDeckChoiceResult MakeMandatoryDeckChoiceFailure(
	const FString& Reason)
{
	FWBMandatoryDeckChoiceResult Result;
	Result.Reason = Reason;
	return Result;
}

bool IsEligibleEntry(
	const FWBZoneCardEntry& Entry,
	const FWBPendingMandatoryDeckChoiceState& Choice,
	const FWBCardDefinitionRepository& Repository)
{
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(
			Repository, Entry.Card.CardId);
	return Lookup.bFound
		&& Lookup.Definition.Kind == EWBCardDefinitionKind::Character
		&& (Choice.RequiredFaction.IsEmpty()
			|| Lookup.Definition.PublicFactions.Contains(
				Choice.RequiredFaction));
}

void NormalizeMandatoryDeckChoiceZone(TArray<FWBZoneCardEntry>& Entries)
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].ZoneIndex = Index;
	}
}

bool ReleaseHeldWands(
	FWBGameStateData& State,
	const FWBPendingMandatoryDeckChoiceState& Choice,
	FString& OutReason)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), Choice.ControllerPlayerId);
	if (Zones == nullptr)
	{
		OutReason = TEXT("player_zones_missing");
		return false;
	}
	for (const FWBEquippedCardEntry& Wand :
		Choice.ActivatedEffectSourceSnapshot.EquippedWands)
	{
		FWBZoneCardEntry Existing;
		if (WBCardZoneState::FindCardByInstanceId(
			State.GetCardZoneState(), Wand.Card.InstanceId, Existing))
		{
			OutReason = TEXT("mandatory_choice_held_wand_already_zoned");
			return false;
		}
		FWBZoneCardEntry Discarded;
		Discarded.Card = Wand.Card;
		Discarded.Zone = EWBCardZone::Discard;
		Discarded.ZoneIndex = Zones->Discard.Num();
		Zones->Discard.Add(MoveTemp(Discarded));
	}
	NormalizeMandatoryDeckChoiceZone(Zones->Discard);
	WBCardZoneState::SortOrderedZonesDeterministically(
		State.GetMutableCardZoneStateForTest());
	OutReason.Reset();
	return true;
}

FWBTraceEvent MakeEffectSummonTrace(
	const FName Kind,
	const FWBPendingMandatoryDeckChoiceState& Choice,
	const FString& Reason = FString())
{
	FWBTraceEvent Trace;
	Trace.Kind = Kind;
	Trace.ActionId = Choice.SourceActionId;
	Trace.PlayerId = Choice.ControllerPlayerId;
	Trace.SourceUnitId = Choice.ActivatedEffectSourceSnapshot.SourceUnitId;
	Trace.FromTile = Choice.ActivatedEffectSourceSnapshot.SourceTile;
	Trace.ToTile = Choice.DestinationTile;
	Trace.PendingEffectFrameId = Choice.SourceEffectFrameId;
	Trace.Reason = Reason;
	Trace.bOk = Reason.IsEmpty();
	return Trace;
}

FWBTraceEvent MakePrivateDeclaredTargetTrace(
	const FWBPendingMandatoryDeckChoiceState& Choice,
	const FString& CardInstanceId)
{
	FWBTraceEvent Trace = MakeEffectSummonTrace(
		FName(TEXT("mandatory_deck_target_declared")), Choice);
	Trace.CardInstanceId = CardInstanceId;
	Trace.bDeclaredTarget = true;
	return Trace;
}
}

FString WBMandatoryDeckChoice::BuildActionId(
	const FWBPendingMandatoryDeckChoiceState& Choice,
	const FString& CardInstanceId)
{
	return FString::Printf(
		TEXT("mandatory_deck_choice:p%d:c%s:i%s"),
		Choice.ControllerPlayerId,
		*Choice.ChoiceId,
		*CardInstanceId);
}

TArray<FString> WBMandatoryDeckChoice::EnumerateLegalActionIds(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	TArray<FString> ActionIds;
	if (!State.HasPendingMandatoryDeckChoice()) return ActionIds;
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
		if (Entry != nullptr && IsEligibleEntry(*Entry, Choice, Repository))
		{
			ActionIds.Add(BuildActionId(Choice, InstanceId));
		}
	}
	return ActionIds;
}

FWBMandatoryDeckChoiceResult WBMandatoryDeckChoice::Submit(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& ActionId)
{
	if (!State.HasPendingMandatoryDeckChoice())
	{
		return MakeMandatoryDeckChoiceFailure(
			TEXT("mandatory_deck_choice_missing"));
	}
	if (State.PendingMandatoryDeckChoice.Origin
		== EWBMandatoryDeckChoiceOrigin::PostDestructionTrigger)
	{
		const FWBPostDestructionTriggerResult Legacy =
			WBPostDestructionTrigger::SubmitChoice(
				State, Repository, ActionId);
		FWBMandatoryDeckChoiceResult Result;
		Result.bOk = Legacy.bOk;
		Result.Reason = Legacy.Reason;
		Result.bPendingChoice = Legacy.bPendingChoice;
		Result.bSummoned = Legacy.bSummoned;
		Result.TraceEvents = Legacy.TraceEvents;
		return Result;
	}
	if (State.PendingMandatoryDeckChoice.Origin
		!= EWBMandatoryDeckChoiceOrigin::ActivatedEffectContinuation)
	{
		return MakeMandatoryDeckChoiceFailure(
			TEXT("mandatory_deck_choice_origin_unsupported"));
	}

	const FWBPendingMandatoryDeckChoiceState Choice =
		State.PendingMandatoryDeckChoice;
	const FString* SelectedInstance =
		Choice.EligibleCardInstanceIds.FindByPredicate(
			[&Choice, &ActionId](const FString& InstanceId)
			{
				return BuildActionId(Choice, InstanceId) == ActionId;
			});
	if (SelectedInstance == nullptr)
	{
		return MakeMandatoryDeckChoiceFailure(
			TEXT("mandatory_deck_choice_illegal"));
	}
	const FWBPlayerCardZoneState* PlayerZones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), Choice.ControllerPlayerId);
	const FWBZoneCardEntry* Selected = PlayerZones != nullptr
		? PlayerZones->Deck.FindByPredicate(
			[SelectedInstance](const FWBZoneCardEntry& Entry)
			{
				return Entry.Card.InstanceId == *SelectedInstance;
			})
		: nullptr;
	if (Selected == nullptr)
	{
		return MakeMandatoryDeckChoiceFailure(
			TEXT("selected_deck_instance_unavailable"));
	}
	if (!IsEligibleEntry(*Selected, Choice, Repository))
	{
		return MakeMandatoryDeckChoiceFailure(
			TEXT("selected_deck_instance_ineligible"));
	}

	FWBDeckSummonRequest Request;
	Request.PlayerId = Choice.ControllerPlayerId;
	Request.SelectedCardInstanceId = *SelectedInstance;
	Request.RequiredFaction = Choice.RequiredFaction;
	Request.TargetTile = Choice.DestinationTile;
	Request.InheritanceSource.SourceSnapshot =
		Choice.ActivatedEffectSourceSnapshot.SourceSnapshot.AsParticipant();
	Request.InheritanceSource.SourceUnitId =
		Choice.ActivatedEffectSourceSnapshot.SourceUnitId;
	Request.InheritanceSource.SourceCurrentRL =
		Choice.ActivatedEffectSourceSnapshot.CurrentRLSnapshot;
	Request.InheritanceSource.EquippedWands =
		Choice.ActivatedEffectSourceSnapshot.EquippedWands;
	Request.InheritanceWandLocation =
		EWBCSNInheritanceWandLocation::DetachedSourceSnapshot;
	Request.SummonTraceKind = FName(TEXT("effect_summon_completed"));
	Request.TransactionId = Choice.ChoiceId;
	const FWBDeckSummonResult Summon =
		WBDeckSummon::SummonExactCharacterToTile(
			State, Repository, Request);

	FWBMandatoryDeckChoiceResult Result;
	Result.bOk = true;
	Result.bSummoned = Summon.bOk;
	Result.TraceEvents.Add(MakePrivateDeclaredTargetTrace(
		Choice, *SelectedInstance));
	Result.TraceEvents.Append(Summon.TraceEvents);
	if (!Summon.bOk)
	{
		FString ReleaseReason;
		if (!ReleaseHeldWands(State, Choice, ReleaseReason))
		{
			return MakeMandatoryDeckChoiceFailure(ReleaseReason);
		}
		Result.TraceEvents.Add(MakeEffectSummonTrace(
			FName(TEXT("effect_summon_failed")), Choice, Summon.Reason));
	}
	State.ClearPendingMandatoryDeckChoice();
	return Result;
}
