#include "WBCharacterSummon.h"

#include "WBCardLifecycle.h"
#include "WBCardZoneState.h"
#include "WBCharacterConstruction.h"
#include "WBTurnOneRestrictions.h"

namespace
{
constexpr int32 CharacterSummonBoardSize = 9;
constexpr int32 CharacterSummonMaxControlledUnitsIncludingHero = 4;

FWBCharacterSummonResult MakeCharacterSummonFailure(const FString& Reason)
{
	FWBCharacterSummonResult Result;
	Result.Reason = Reason;
	return Result;
}

bool IsCharacterSummonTileInBounds(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.X < CharacterSummonBoardSize
		&& Tile.Y >= 0 && Tile.Y < CharacterSummonBoardSize;
}

const TArray<FWBZoneCardEntry>* GetCharacterSummonSourceEntries(
	const FWBPlayerCardZoneState& Zones,
	const EWBCardZone SourceZone)
{
	if (SourceZone == EWBCardZone::Deck) return &Zones.Deck;
	if (SourceZone == EWBCardZone::Hand) return &Zones.Hand;
	return nullptr;
}

const FWBUnitState* FindCharacterSummonControlledHero(
	const FWBGameStateData& State,
	const int32 ControllerPlayerId)
{
	const FWBPlayerStateData* Player = State.GetPlayerById(ControllerPlayerId);
	if (Player == nullptr) return nullptr;
	const FWBUnitState* Hero = State.GetUnitById(Player->HeroUnitId);
	return Hero != nullptr
		&& Hero->IsUnitOnBoard()
		&& Hero->GetControllerPlayerIdForRules() == ControllerPlayerId
		? Hero : nullptr;
}

bool AreCharacterSummonTilesOrthogonallyAdjacent(
	const FWBTile& A,
	const FWBTile& B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) == 1;
}
}

FWBCharacterSummonResult WBCharacterSummon::SummonExactCharacter(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBCharacterSummonRequest& Request)
{
	if (State.bGameOver)
	{
		return MakeCharacterSummonFailure(TEXT("game_over"));
	}
	if (!FWBGameStateData::IsValidPlayerId(Request.OwnerPlayerId)
		|| !FWBGameStateData::IsValidPlayerId(Request.ControllerPlayerId)
		|| State.GetPlayerById(Request.OwnerPlayerId) == nullptr
		|| State.GetPlayerById(Request.ControllerPlayerId) == nullptr)
	{
		return MakeCharacterSummonFailure(TEXT("invalid_player"));
	}
	if (Request.SourceZone != EWBCardZone::Deck
		&& Request.SourceZone != EWBCardZone::Hand)
	{
		return MakeCharacterSummonFailure(TEXT("summon_source_zone_unsupported"));
	}
	if (Request.CardInstanceId.IsEmpty())
	{
		return MakeCharacterSummonFailure(TEXT("source_card_missing"));
	}

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		State.GetCardZoneState(), ZoneReason))
	{
		return MakeCharacterSummonFailure(ZoneReason.IsEmpty()
			? FString(TEXT("zone_state_invalid")) : ZoneReason);
	}
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), Request.OwnerPlayerId);
	if (Zones == nullptr)
	{
		return MakeCharacterSummonFailure(TEXT("player_zones_missing"));
	}
	const TArray<FWBZoneCardEntry>* Entries = GetCharacterSummonSourceEntries(
		*Zones, Request.SourceZone);
	const FWBZoneCardEntry* Selected = Entries != nullptr
		? Entries->FindByPredicate([&Request](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == Request.CardInstanceId;
		}) : nullptr;
	if (Selected == nullptr)
	{
		FWBZoneCardEntry Existing;
		return MakeCharacterSummonFailure(WBCardZoneState::FindCardByInstanceId(
			State.GetCardZoneState(), Request.CardInstanceId, Existing)
			? (Request.SourceZone == EWBCardZone::Deck
				? FString(TEXT("source_card_not_in_deck"))
				: FString(TEXT("source_card_not_in_hand")))
			: FString(TEXT("source_card_missing")));
	}
	if (Selected->Card.OwnerPlayerId != Request.OwnerPlayerId)
	{
		return MakeCharacterSummonFailure(TEXT("source_card_owner_mismatch"));
	}
	if (!Request.ExpectedCardId.IsEmpty()
		&& Selected->Card.CardId != Request.ExpectedCardId)
	{
		return MakeCharacterSummonFailure(TEXT("source_card_id_mismatch"));
	}

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(
			Repository, Selected->Card.CardId);
	if (!Lookup.bFound)
	{
		return MakeCharacterSummonFailure(TEXT("card_definition_not_found"));
	}
	if (Lookup.Definition.Kind != EWBCardDefinitionKind::Character)
	{
		return MakeCharacterSummonFailure(TEXT("source_card_not_character"));
	}
	if (!WBCharacterConstruction::IsValidCharacterDefinition(
		Lookup.Definition))
	{
		return MakeCharacterSummonFailure(TEXT("invalid_character_stats"));
	}
	if (!Request.RequiredFaction.IsEmpty()
		&& !Lookup.Definition.PublicFactions.Contains(
			Request.RequiredFaction))
	{
		return MakeCharacterSummonFailure(TEXT("source_card_faction_mismatch"));
	}
	if (!IsCharacterSummonTileInBounds(Request.TargetTile))
	{
		return MakeCharacterSummonFailure(TEXT("target_tile_out_of_bounds"));
	}
	if (State.IsTileOccupied(Request.TargetTile))
	{
		return MakeCharacterSummonFailure(TEXT("target_tile_occupied"));
	}
	if (State.GetUnitsControlledByPlayer(Request.ControllerPlayerId).Num()
		>= CharacterSummonMaxControlledUnitsIncludingHero)
	{
		return MakeCharacterSummonFailure(TEXT("unit_cap_reached"));
	}
	if (Request.ConditionPolicy == EWBCharacterSummonConditionPolicy::Normal)
	{
		const FWBUnitState* Hero = FindCharacterSummonControlledHero(
			State, Request.ControllerPlayerId);
		if (Hero == nullptr
			|| !IsCharacterSummonTileInBounds(FWBTile(Hero->X, Hero->Y)))
		{
			return MakeCharacterSummonFailure(TEXT("hero_not_found"));
		}
		if (!AreCharacterSummonTilesOrthogonallyAdjacent(
			FWBTile(Hero->X, Hero->Y), Request.TargetTile))
		{
			return MakeCharacterSummonFailure(
				TEXT("target_tile_not_adjacent_to_hero"));
		}
		const FWBTurnOneRestrictionQuery Placement =
			WBTurnOneRestrictions::QuerySummonPlacement(
				State, Request.ControllerPlayerId, Request.TargetTile);
		if (!Placement.bOk)
		{
			return MakeCharacterSummonFailure(Placement.Reason.IsEmpty()
				? FString(TEXT("target_tile_not_allowed"))
				: Placement.Reason);
		}
	}

	const int32 NewUnitId = WBCharacterConstruction::AllocateNextUnitId(State);
	if (NewUnitId < 0 || State.GetUnitById(NewUnitId) != nullptr)
	{
		return MakeCharacterSummonFailure(TEXT("unit_id_allocation_failed"));
	}
	FWBCharacterConstructionRequest Construction;
	Construction.UnitId = NewUnitId;
	Construction.OwnerPlayerId = Request.OwnerPlayerId;
	Construction.ControllerPlayerId = Request.ControllerPlayerId;
	Construction.CardId = Selected->Card.CardId;
	Construction.Tile = Request.TargetTile;
	const FWBCharacterConstructionResult Built =
		WBCharacterConstruction::BuildUnit(Lookup.Definition, Construction);
	if (!Built.bOk)
	{
		return MakeCharacterSummonFailure(Built.Reason);
	}

	FWBGameStateData WorkingState = State;
	const FWBCardLifecycleResult Removed = Request.SourceZone == EWBCardZone::Deck
		? WBCardLifecycle::RemoveExactCardFromDeck(
			WorkingState, Request.OwnerPlayerId, Request.CardInstanceId)
		: WBCardLifecycle::RemoveExactCardFromHand(
			WorkingState, Request.OwnerPlayerId, Request.CardInstanceId);
	if (!Removed.bOk || Removed.CardId != Selected->Card.CardId)
	{
		return MakeCharacterSummonFailure(Removed.Reason.IsEmpty()
			? FString(TEXT("source_card_unavailable")) : Removed.Reason);
	}
	WorkingState.Units.Add(Built.Unit);
	if (!WBCardZoneState::ValidateZoneStateForTest(
		WorkingState.GetCardZoneState(), ZoneReason))
	{
		return MakeCharacterSummonFailure(ZoneReason.IsEmpty()
			? FString(TEXT("zone_state_invalid")) : ZoneReason);
	}

	FWBCharacterSummonResult Result;
	Result.bOk = true;
	Result.CreatedUnitId = NewUnitId;
	Result.CardInstanceId = Request.CardInstanceId;
	Result.CardId = Selected->Card.CardId;
	FWBTraceEvent Summoned;
	Summoned.Kind = Request.TraceKind.IsNone()
		? FName(TEXT("effect_summon_completed")) : Request.TraceKind;
	Summoned.ActionId = Request.TransactionId;
	Summoned.PlayerId = Request.ControllerPlayerId;
	Summoned.SourceUnitId = Request.SourceUnitId;
	Summoned.TargetUnitId = NewUnitId;
	Summoned.CardId = Result.CardId;
	Summoned.CardInstanceId = Request.bIncludeSelectedInstanceInTrace
		? Request.CardInstanceId : FString();
	Summoned.ToTile = Request.TargetTile;
	Summoned.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Summoned));
	State = MoveTemp(WorkingState);
	return Result;
}
