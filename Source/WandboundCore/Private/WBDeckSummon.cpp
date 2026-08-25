#include "WBDeckSummon.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBCSNInheritance.h"

namespace
{
constexpr int32 DeckSummonBoardSize = 9;
constexpr int32 DeckSummonMaxOwnedUnitsIncludingHero = 4;

FWBDeckSummonResult MakeDeckSummonFailure(const FString& Reason)
{
	FWBDeckSummonResult Result;
	Result.Reason = Reason;
	return Result;
}

int32 AllocateDeckSummonUnitId(const FWBGameStateData& State)
{
	int32 MaxId = INDEX_NONE;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.UnitId == MAX_int32) return INDEX_NONE;
		MaxId = FMath::Max(MaxId, Unit.UnitId);
	}
	return MaxId + 1;
}
}

FWBDeckSummonResult WBDeckSummon::SummonExactCharacterToTile(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBDeckSummonRequest& Request)
{
	if (State.bGameOver)
	{
		return MakeDeckSummonFailure(TEXT("game_over"));
	}
	if (!FWBGameStateData::IsValidPlayerId(Request.PlayerId)
		|| State.GetPlayerById(Request.PlayerId) == nullptr)
	{
		return MakeDeckSummonFailure(TEXT("invalid_player"));
	}
	if (Request.TargetTile.X < 0 || Request.TargetTile.X >= DeckSummonBoardSize
		|| Request.TargetTile.Y < 0 || Request.TargetTile.Y >= DeckSummonBoardSize)
	{
		return MakeDeckSummonFailure(TEXT("post_destruction_tile_out_of_bounds"));
	}
	if (State.IsTileOccupied(Request.TargetTile))
	{
		return MakeDeckSummonFailure(TEXT("post_destruction_tile_occupied"));
	}
	if (State.GetUnitsForPlayer(Request.PlayerId).Num()
		>= DeckSummonMaxOwnedUnitsIncludingHero)
	{
		return MakeDeckSummonFailure(TEXT("unit_cap_reached"));
	}

	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), Request.PlayerId);
	if (Zones == nullptr)
	{
		return MakeDeckSummonFailure(TEXT("player_zones_missing"));
	}
	const FWBZoneCardEntry* Selected = Zones->Deck.FindByPredicate(
		[&Request](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == Request.SelectedCardInstanceId;
		});
	if (Selected == nullptr)
	{
		return MakeDeckSummonFailure(TEXT("selected_deck_instance_unavailable"));
	}
	const FString SelectedCardId = Selected->Card.CardId;
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, SelectedCardId);
	if (!Lookup.bFound)
	{
		return MakeDeckSummonFailure(TEXT("card_definition_not_found"));
	}
	if (Lookup.Definition.Kind != EWBCardDefinitionKind::Character)
	{
		return MakeDeckSummonFailure(TEXT("selected_deck_card_not_character"));
	}
	if (!Request.RequiredFaction.IsEmpty()
		&& !Lookup.Definition.PublicFactions.Contains(Request.RequiredFaction))
	{
		return MakeDeckSummonFailure(TEXT("selected_deck_card_faction_mismatch"));
	}
	const FWBCardCharacterStatsDefinition& Stats =
		Lookup.Definition.CharacterStats;
	if (Stats.HP <= 0 || Stats.ATK < 0 || Stats.AR < 0 || Stats.RL < 0)
	{
		return MakeDeckSummonFailure(TEXT("invalid_character_stats"));
	}
	const int32 NewUnitId = AllocateDeckSummonUnitId(State);
	if (NewUnitId < 0)
	{
		return MakeDeckSummonFailure(TEXT("unit_id_allocation_failed"));
	}

	FWBGameStateData WorkingState = State;
	const FWBCardLifecycleResult Removed =
		WBCardLifecycle::RemoveExactCardFromDeck(
			WorkingState,
			Request.PlayerId,
			Request.SelectedCardInstanceId);
	if (!Removed.bOk || Removed.CardId != SelectedCardId)
	{
		return MakeDeckSummonFailure(Removed.Reason.IsEmpty()
			? FString(TEXT("selected_deck_instance_unavailable"))
			: Removed.Reason);
	}

	FWBUnitState Unit;
	Unit.UnitId = NewUnitId;
	Unit.OwnerId = Request.PlayerId;
	Unit.CardId = SelectedCardId;
	Unit.X = Request.TargetTile.X;
	Unit.Y = Request.TargetTile.Y;
	Unit.HP = Stats.HP;
	Unit.MaxHP = Stats.HP;
	Unit.ATK = Stats.ATK;
	Unit.AR = Stats.AR;
	Unit.BaseRL = Stats.RL;
	Unit.CurrentRL = Stats.RL;
	Unit.RLTotal = Stats.RL;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 0;
	Unit.MaxAttacksPerTurn = 1;
	WorkingState.Units.Add(Unit);

	FWBCSNInheritanceMutationRequest Inheritance;
	Inheritance.ControllerPlayerId = Request.PlayerId;
	Inheritance.SourceUnitId = Request.InheritanceSource.SourceUnitId;
	Inheritance.TargetUnitId = NewUnitId;
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
	Result.CreatedUnitId = NewUnitId;
	Result.SummonedCardId = SelectedCardId;
	FWBTraceEvent Summoned;
	Summoned.Kind = Request.SummonTraceKind.IsNone()
		? FName(TEXT("effect_summon_completed"))
		: Request.SummonTraceKind;
	Summoned.PlayerId = Request.PlayerId;
	Summoned.SourceUnitId = Request.InheritanceSource.SourceUnitId;
	Summoned.TargetUnitId = NewUnitId;
	Summoned.CardId = SelectedCardId;
	Summoned.ToTile = Request.TargetTile;
	Summoned.ActionId = Request.TransactionId;
	Summoned.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Summoned));
	Result.TraceEvents.Append(Inherited.TraceEvents);
	State = MoveTemp(WorkingState);
	return Result;
}
