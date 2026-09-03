#include "WBMandatoryDeckChoice.h"

#include "WBCardZoneState.h"
#include "WBDeckSummon.h"
#include "WBPostDestructionTrigger.h"
#include "WBPrivateCardChoice.h"

namespace
{
FWBMandatoryDeckChoiceResult Fail(const FString& Reason)
{
	FWBMandatoryDeckChoiceResult Result;
	Result.Reason = Reason;
	return Result;
}

void NormalizeZone(TArray<FWBZoneCardEntry>& Entries)
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].ZoneIndex = Index;
	}
}

bool ReleaseHeldWands(
	FWBGameStateData& State,
	const FWBPendingPrivateCardChoiceState& Choice,
	FString& OutReason)
{
	const FWBActivatedEffectSourceSnapshot& Snapshot =
		Choice.ActivatedEffect.ActivatedEffectSourceSnapshot;
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), Choice.Descriptor.ChoosingPlayerId);
	if (Zones == nullptr)
	{
		OutReason = TEXT("player_zones_missing");
		return false;
	}
	for (const FWBEquippedCardEntry& Wand : Snapshot.EquippedWands)
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
	NormalizeZone(Zones->Discard);
	WBCardZoneState::SortOrderedZonesDeterministically(
		State.GetMutableCardZoneStateForTest());
	OutReason.Reset();
	return true;
}

FWBTraceEvent MakeEffectTrace(
	const FName Kind,
	const FWBPendingPrivateCardChoiceState& Choice,
	const FString& Reason = FString())
{
	const FWBPrivateCardChoiceDescriptor& Descriptor = Choice.Descriptor;
	const FWBActivatedEffectSourceSnapshot& Snapshot =
		Choice.ActivatedEffect.ActivatedEffectSourceSnapshot;
	FWBTraceEvent Trace;
	Trace.Kind = Kind;
	Trace.ActionId = Descriptor.SourceActionId;
	Trace.PlayerId = Descriptor.ChoosingPlayerId;
	Trace.SourceUnitId = Snapshot.SourceUnitId;
	Trace.FromTile = Snapshot.SourceTile;
	Trace.ToTile = Choice.ActivatedEffect.DestinationTile;
	Trace.PendingEffectFrameId = Descriptor.SourceEffectFrameId;
	Trace.Reason = Reason;
	Trace.bOk = Reason.IsEmpty();
	return Trace;
}

FWBTraceEvent MakePrivateDeclaredTargetTrace(
	const FWBPendingPrivateCardChoiceState& Choice,
	const FString& CardInstanceId)
{
	FWBTraceEvent Trace = MakeEffectTrace(
		FName(TEXT("mandatory_deck_target_declared")), Choice);
	Trace.CardInstanceId = CardInstanceId;
	Trace.bDeclaredTarget = WBIsPlayerDeclared(
		Choice.Descriptor.TargetDeclaration);
	return Trace;
}

FWBMandatoryDeckChoiceResult ResolveActivatedEffect(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBPendingPrivateCardChoiceState& Choice,
	const FString& SelectedInstance)
{
	const FWBActivatedEffectSourceSnapshot& Snapshot =
		Choice.ActivatedEffect.ActivatedEffectSourceSnapshot;
	FWBDeckSummonRequest Request;
	Request.PlayerId = Choice.Descriptor.ChoosingPlayerId;
	Request.SelectedCardInstanceId = SelectedInstance;
	Request.RequiredFaction = Choice.Descriptor.Filter.RequiredFaction;
	Request.TargetTile = Choice.ActivatedEffect.DestinationTile;
	Request.InheritanceSource.SourceSnapshot = Snapshot.SourceSnapshot.AsParticipant();
	Request.InheritanceSource.SourceUnitId = Snapshot.SourceUnitId;
	Request.InheritanceSource.SourceCurrentRL = Snapshot.CurrentRLSnapshot;
	Request.InheritanceSource.EquippedWands = Snapshot.EquippedWands;
	Request.InheritanceWandLocation =
		EWBCSNInheritanceWandLocation::DetachedSourceSnapshot;
	Request.SummonTraceKind = FName(TEXT("effect_summon_completed"));
	Request.TransactionId = Choice.Descriptor.ChoiceId;
	const FWBDeckSummonResult Summon =
		WBDeckSummon::SummonExactCharacterToTile(State, Repository, Request);

	FWBMandatoryDeckChoiceResult Result;
	Result.bOk = true;
	Result.bSummoned = Summon.bOk;
	Result.TraceEvents.Add(MakePrivateDeclaredTargetTrace(Choice, SelectedInstance));
	Result.TraceEvents.Append(Summon.TraceEvents);
	if (!Summon.bOk)
	{
		FString ReleaseReason;
		if (!ReleaseHeldWands(State, Choice, ReleaseReason))
		{
			return Fail(ReleaseReason);
		}
		Result.TraceEvents.Add(MakeEffectTrace(
			FName(TEXT("effect_summon_failed")), Choice, Summon.Reason));
	}
	State.ClearPendingMandatoryDeckChoice();
	return Result;
}
}

FString WBMandatoryDeckChoice::BuildActionId(
	const FWBPendingMandatoryDeckChoiceState& Choice,
	const FString& CardInstanceId)
{
	return FString::Printf(
		TEXT("mandatory_deck_choice:p%d:c%s:i%s"),
		Choice.Descriptor.ChoosingPlayerId,
		*Choice.Descriptor.ChoiceId,
		*CardInstanceId);
}

TArray<FString> WBMandatoryDeckChoice::EnumerateLegalActionIds(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId)
{
	TArray<FString> ActionIds;
	if (!State.HasPendingMandatoryDeckChoice()) return ActionIds;
	const FWBPendingPrivateCardChoiceState& Choice =
		State.PendingMandatoryDeckChoice;
	if (ViewerPlayerId != Choice.Descriptor.ChoosingPlayerId
		|| State.bGameOver)
	{
		return ActionIds;
	}
	for (const FString& InstanceId : Choice.Descriptor.FrozenCandidateInstanceIds)
	{
		const FWBPrivateCardChoiceSelectionResult Validation =
			WBPrivateCardChoice::ValidateSelection(
				State, Repository, Choice.Descriptor, InstanceId, true);
		if (Validation.bOk)
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
		return Fail(TEXT("mandatory_deck_choice_missing"));
	}
	const FWBPendingPrivateCardChoiceState Choice = State.PendingMandatoryDeckChoice;
	const FString* SelectedInstance =
		Choice.Descriptor.FrozenCandidateInstanceIds.FindByPredicate(
			[&Choice, &ActionId](const FString& InstanceId)
			{
				return BuildActionId(Choice, InstanceId) == ActionId;
			});
	if (SelectedInstance == nullptr)
	{
		return Fail(TEXT("mandatory_deck_choice_illegal"));
	}
	const FWBPrivateCardChoiceSelectionResult Validation =
		WBPrivateCardChoice::ValidateSelection(
			State, Repository, Choice.Descriptor, *SelectedInstance, true);
	if (!Validation.bOk) return Fail(Validation.Reason);

	FWBGameStateData WorkingState = State;
	FWBMandatoryDeckChoiceResult Result;
	switch (Choice.Descriptor.ContinuationKind)
	{
	case EWBPrivateCardChoiceContinuationKind::PostDestructionTrigger:
	{
		const FWBPostDestructionTriggerResult Continued =
			WBPostDestructionTrigger::ResolveSelectedChoice(
				WorkingState, Repository, *SelectedInstance, ActionId);
		Result.bOk = Continued.bOk;
		Result.Reason = Continued.Reason;
		Result.bPendingChoice = Continued.bPendingChoice;
		Result.bSummoned = Continued.bSummoned;
		Result.TraceEvents = Continued.TraceEvents;
		break;
	}
	case EWBPrivateCardChoiceContinuationKind::ActivatedEffectContinuation:
		Result = ResolveActivatedEffect(
			WorkingState, Repository, Choice, *SelectedInstance);
		break;
	default:
		return Fail(TEXT("mandatory_deck_choice_origin_unsupported"));
	}
	if (!Result.bOk) return Result;
	State = MoveTemp(WorkingState);
	return Result;
}
