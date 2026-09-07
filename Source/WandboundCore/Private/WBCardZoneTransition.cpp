#include "WBCardZoneTransition.h"

#include "WBCardZoneState.h"
#include "WBGameStateData.h"

bool FWBCardZoneTransitionSnapshot::IsValid() const
{
	return EventIdentity.IsValid()
		&& !CardInstanceId.IsEmpty()
		&& !CardId.IsEmpty()
		&& FWBGameStateData::IsValidPlayerId(OwnerPlayerId)
		&& WBCardZoneState::IsOrderedZone(SourceZone)
		&& WBCardZoneState::IsOrderedZone(DestinationZone)
		&& SourceZone != DestinationZone
		&& SourceZoneCountAfter >= 0
		&& DestinationZoneCountAfter > 0
		&& ResolutionOrder >= 0;
}

bool WBCardZoneTransition::BuildCommittedSnapshot(
	const FWBGameStateData& State,
	const FWBCardZoneMutationResult& Mutation,
	const FWBCardZoneTransitionContext& Context,
	FWBCardZoneTransitionSnapshot& OutSnapshot,
	FString& OutReason)
{
	OutSnapshot = FWBCardZoneTransitionSnapshot();
	if (!Mutation.bOk
		|| Mutation.Code != EWBCardZoneMutationResultCode::Success)
	{
		OutReason = TEXT("card_zone_transition_mutation_not_successful");
		return false;
	}
	if (!WBCardZoneState::IsOrderedZone(Mutation.SourceZone)
		|| !WBCardZoneState::IsOrderedZone(Mutation.DestinationZone)
		|| Mutation.SourceZone == Mutation.DestinationZone)
	{
		OutReason = TEXT("card_zone_transition_zone_unsupported");
		return false;
	}
	if (Mutation.Card.InstanceId.IsEmpty()
		|| Mutation.Card.CardId.IsEmpty()
		|| Mutation.Card.OwnerPlayerId != Mutation.PlayerId
		|| !FWBGameStateData::IsValidPlayerId(Mutation.PlayerId)
		|| Context.ResolutionOrder < 0)
	{
		OutReason = TEXT("card_zone_transition_identity_invalid");
		return false;
	}

	OutSnapshot.CardInstanceId = Mutation.Card.InstanceId;
	OutSnapshot.CardId = Mutation.Card.CardId;
	OutSnapshot.OwnerPlayerId = Mutation.Card.OwnerPlayerId;
	OutSnapshot.SourceZone = Mutation.SourceZone;
	OutSnapshot.DestinationZone = Mutation.DestinationZone;
	OutSnapshot.Cause = Context.Cause;
	OutSnapshot.SourceZoneCountAfter = Mutation.SourceZoneCountAfter;
	OutSnapshot.DestinationZoneCountAfter =
		Mutation.DestinationZoneCountAfter;
	OutSnapshot.ResolutionOrder = Context.ResolutionOrder;
	const FString EventId = FString::Printf(
		TEXT("card_zone_transition:t%d:p%d:o%d:%s>%s:i%s"),
		State.TurnNumber,
		Mutation.PlayerId,
		Context.ResolutionOrder,
		*WBCardZoneState::ZoneToString(Mutation.SourceZone),
		*WBCardZoneState::ZoneToString(Mutation.DestinationZone),
		*Mutation.Card.InstanceId);
	OutSnapshot.EventIdentity = WBEventSnapshot::MakeIdentity(
		EWBEventKind::CardZoneTransition,
		EventId,
		State.TurnNumber,
		Context.SourceActionId,
		Context.ContinuationId,
		Context.ActionDeclaration,
		Context.TargetDeclaration);
	if (!OutSnapshot.IsValid())
	{
		OutSnapshot = FWBCardZoneTransitionSnapshot();
		OutReason = TEXT("card_zone_transition_snapshot_invalid");
		return false;
	}

	OutReason.Reset();
	return true;
}

FWBTraceEvent WBCardZoneTransition::MakeRedactedTrace(
	const FWBCardZoneTransitionSnapshot& Snapshot)
{
	FWBTraceEvent Trace;
	Trace.Kind = FName(TEXT("card_zone_transition"));
	Trace.PlayerId = Snapshot.OwnerPlayerId;
	Trace.TurnNumber = Snapshot.EventIdentity.TurnNumber;
	Trace.CardCount = 1;
	Trace.SourceCardZone = FName(
		*WBCardZoneState::ZoneToString(Snapshot.SourceZone));
	Trace.DestinationCardZone = FName(
		*WBCardZoneState::ZoneToString(Snapshot.DestinationZone));
	Trace.CardZoneTransitionCause = CauseToName(Snapshot.Cause);
	Trace.ResolutionOrder = Snapshot.ResolutionOrder;
	Trace.bOk = Snapshot.IsValid();
	return Trace;
}

void WBCardZoneTransition::AppendRedactedTraces(
	const TArray<FWBCardZoneTransitionSnapshot>& Snapshots,
	TArray<FWBTraceEvent>& OutTraceEvents)
{
	for (const FWBCardZoneTransitionSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.IsValid())
		{
			OutTraceEvents.Add(MakeRedactedTrace(Snapshot));
		}
	}
}

FName WBCardZoneTransition::CauseToName(
	const EWBCardZoneTransitionCause Cause)
{
	switch (Cause)
	{
	case EWBCardZoneTransitionCause::Draw:
		return FName(TEXT("draw"));
	case EWBCardZoneTransitionCause::Effect:
		return FName(TEXT("effect"));
	case EWBCardZoneTransitionCause::Cost:
		return FName(TEXT("cost"));
	case EWBCardZoneTransitionCause::Rule:
		return FName(TEXT("rule"));
	case EWBCardZoneTransitionCause::Setup:
		return FName(TEXT("setup"));
	case EWBCardZoneTransitionCause::Unknown:
	default:
		return FName(TEXT("unknown"));
	}
}
