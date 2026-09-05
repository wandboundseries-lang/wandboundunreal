#include "WBDeckSummon.h"

#include "WBCardZoneState.h"
#include "WBCharacterSummon.h"
#include "WBCSNInheritance.h"

namespace
{
FWBDeckSummonResult MakeDeckSummonFailure(const FString& Reason)
{
	FWBDeckSummonResult Result;
	Result.Reason = Reason;
	return Result;
}

FString MapDeckSummonReason(const FString& Reason)
{
	if (Reason == TEXT("target_tile_out_of_bounds"))
		return TEXT("post_destruction_tile_out_of_bounds");
	if (Reason == TEXT("target_tile_occupied"))
		return TEXT("post_destruction_tile_occupied");
	if (Reason == TEXT("source_card_missing")
		|| Reason == TEXT("source_card_not_in_deck")
		|| Reason == TEXT("card_instance_missing")
		|| Reason == TEXT("card_not_in_expected_zone"))
		return TEXT("selected_deck_instance_unavailable");
	if (Reason == TEXT("source_card_not_character"))
		return TEXT("selected_deck_card_not_character");
	if (Reason == TEXT("source_card_faction_mismatch"))
		return TEXT("selected_deck_card_faction_mismatch");
	return Reason;
}
}

FWBDeckSummonResult WBDeckSummon::SummonExactCharacterToTile(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBDeckSummonRequest& Request)
{
	FWBGameStateData WorkingState = State;
	FWBCharacterSummonRequest SummonRequest;
	SummonRequest.OwnerPlayerId = Request.PlayerId;
	SummonRequest.ControllerPlayerId = Request.PlayerId;
	SummonRequest.SourceZone = EWBCardZone::Deck;
	SummonRequest.CardInstanceId = Request.SelectedCardInstanceId;
	SummonRequest.RequiredFaction = Request.RequiredFaction;
	SummonRequest.TargetTile = Request.TargetTile;
	SummonRequest.ConditionPolicy =
		EWBCharacterSummonConditionPolicy::IgnoreSummoningConditions;
	SummonRequest.TraceKind = Request.SummonTraceKind;
	SummonRequest.TransactionId = Request.TransactionId;
	SummonRequest.SourceUnitId = Request.InheritanceSource.SourceUnitId;
	const FWBCharacterSummonResult Summoned =
		WBCharacterSummon::SummonExactCharacter(
			WorkingState, Repository, SummonRequest);
	if (!Summoned.bOk)
	{
		return MakeDeckSummonFailure(MapDeckSummonReason(Summoned.Reason));
	}

	FWBCSNInheritanceMutationRequest Inheritance;
	Inheritance.SourceSnapshot = Request.InheritanceSource.SourceSnapshot;
	Inheritance.ControllerPlayerId = Request.PlayerId;
	Inheritance.SourceUnitId = Request.InheritanceSource.SourceUnitId;
	Inheritance.TargetUnitId = Summoned.CreatedUnitId;
	Inheritance.SourceCurrentRL = Request.InheritanceSource.SourceCurrentRL;
	Inheritance.EquippedWandSnapshot =
		Request.InheritanceSource.EquippedWands;
	Inheritance.ExpectedWandLocation = Request.InheritanceWandLocation;
	Inheritance.TransactionId = Request.TransactionId;
	const FWBCSNInheritanceMutationResult Inherited =
		WBCSNInheritance::Apply(WorkingState, Repository, Inheritance);
	if (!Inherited.bOk)
	{
		return MakeDeckSummonFailure(Inherited.Reason);
	}

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		WorkingState.GetCardZoneState(), ZoneReason))
	{
		return MakeDeckSummonFailure(ZoneReason.IsEmpty()
			? FString(TEXT("invalid_zone_state")) : ZoneReason);
	}

	FWBDeckSummonResult Result;
	Result.bOk = true;
	Result.CreatedUnitId = Summoned.CreatedUnitId;
	Result.SummonedCardId = Summoned.CardId;
	Result.TraceEvents = Summoned.TraceEvents;
	Result.TraceEvents.Append(Inherited.TraceEvents);
	State = MoveTemp(WorkingState);
	return Result;
}
