#include "WBEventSnapshot.h"

#include "WBGameStateData.h"

bool FWBEventIdentitySnapshot::IsValid() const
{
	return Kind != EWBEventKind::Unknown
		&& !EventId.IsEmpty()
		&& TurnNumber >= 0;
}

bool FWBUnitParticipantSnapshot::IsCaptured() const
{
	return UnitId != INDEX_NONE && !CardId.IsEmpty();
}

int32 FWBEventSourceSnapshot::GetCasterUnitId() const
{
	return WBIsActivation(ActivationProvenance) && SourceUnitId >= 0
		? SourceUnitId
		: INDEX_NONE;
}

FWBUnitParticipantSnapshot FWBEventSourceSnapshot::AsParticipant() const
{
	FWBUnitParticipantSnapshot Result;
	Result.UnitId = SourceUnitId;
	Result.CardId = SourceCardId;
	Result.OwnerPlayerId = OwnerPlayerId;
	Result.ControllerPlayerId = ControllerPlayerId;
	Result.Tile = SourceTile;
	Result.bWasHero = bWasHero;
	return Result;
}

FWBEventIdentitySnapshot WBEventSnapshot::MakeIdentity(
	const EWBEventKind Kind,
	const FString& EventId,
	const int32 TurnNumber,
	const FString& SourceActionId,
	const FString& ContinuationId,
	const EWBDeclarationProvenance ActionDeclaration,
	const EWBDeclarationProvenance TargetDeclaration)
{
	FWBEventIdentitySnapshot Result;
	Result.Kind = Kind;
	Result.EventId = EventId;
	Result.TurnNumber = TurnNumber;
	Result.SourceActionId = SourceActionId;
	Result.ContinuationId = ContinuationId;
	Result.ActionDeclaration = ActionDeclaration;
	Result.TargetDeclaration = TargetDeclaration;
	return Result;
}

FWBUnitParticipantSnapshot WBEventSnapshot::CaptureUnitParticipant(
	const FWBGameStateData& State,
	const FWBUnitState& Unit)
{
	FWBUnitParticipantSnapshot Result;
	Result.UnitId = Unit.UnitId;
	Result.CardId = Unit.CardId;
	Result.OwnerPlayerId = Unit.GetOwnerPlayerIdForRules();
	Result.ControllerPlayerId = Unit.GetControllerPlayerIdForRules();
	Result.Tile = FWBTile(Unit.X, Unit.Y);
	const FWBPlayerStateData* Owner = State.GetPlayerById(Result.OwnerPlayerId);
	Result.bWasHero = Owner != nullptr && Owner->HeroUnitId == Unit.UnitId;
	return Result;
}

FWBEventSourceSnapshot WBEventSnapshot::CaptureUnitSource(
	const FWBGameStateData& State,
	const FWBUnitState& Unit,
	const EWBActivationProvenance ActivationProvenance)
{
	const FWBUnitParticipantSnapshot Participant =
		CaptureUnitParticipant(State, Unit);
	FWBEventSourceSnapshot Result;
	Result.SourceUnitId = Participant.UnitId;
	Result.SourceCardId = Participant.CardId;
	Result.OwnerPlayerId = Participant.OwnerPlayerId;
	Result.ControllerPlayerId = Participant.ControllerPlayerId;
	Result.SourceTile = Participant.Tile;
	Result.bWasHero = Participant.bWasHero;
	Result.ActivationProvenance = ActivationProvenance;
	return Result;
}

FWBEventSourceSnapshot WBEventSnapshot::FromStatusSource(
	const FWBStatusSourceProvenance& Source)
{
	FWBEventSourceSnapshot Result;
	Result.SourceUnitId = Source.SourceUnitId;
	Result.SourceCardId = Source.SourceCardId;
	Result.SourceCardInstanceId = Source.SourceCardInstanceId;
	Result.OwnerPlayerId = Source.SourceOwnerPlayerId;
	Result.ControllerPlayerId = Source.SourcePlayerId;
	// Status provenance does not retain whether an activation was unit-cast.
	// Keep the adapter resolution-only so it never fabricates a Caster.
	Result.ActivationProvenance = EWBActivationProvenance::ResolutionOnly;
	return Result;
}
