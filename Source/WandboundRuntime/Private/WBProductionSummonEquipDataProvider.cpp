#include "WBProductionSummonEquipDataProvider.h"

#include "WBCardZoneObservation.h"
#include "WBGameStateData.h"
#include "WBResonanceLoad.h"

namespace
{
constexpr int32 BoardSize = 9;
constexpr int32 MaxOwnedUnitsIncludingHero = 4;

void AddDiagnostic(
	FWBProductionSummonEquipDecisionData& Data,
	const TCHAR* Code,
	const FString& CardId = FString(),
	const FString& InstanceId = FString(),
	const int32 UnitId = -1)
{
	FWBProductionSummonEquipProviderDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.CardId = CardId;
	Diagnostic.InstanceId = InstanceId;
	Diagnostic.UnitId = UnitId;
	Data.Diagnostics.Add(Diagnostic);
}

bool IsTileInBounds(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.X < BoardSize && Tile.Y >= 0 && Tile.Y < BoardSize;
}

const FWBUnitState* FindHeroUnit(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
	if (Player == nullptr || Player->HeroUnitId < 0)
	{
		return nullptr;
	}

	const FWBUnitState* Hero = State.GetUnitById(Player->HeroUnitId);
	if (Hero == nullptr
		|| Hero->OwnerId != PlayerId
		|| Hero->bDefeated
		|| !Hero->IsUnitOnBoard())
	{
		return nullptr;
	}

	return Hero;
}

TArray<FWBTile> BuildSummonTilesAdjacentToHero(
	const FWBGameStateData& State,
	const FWBUnitState& Hero)
{
	const FWBTile HeroTile(Hero.X, Hero.Y);
	TArray<FWBTile> Tiles;
	Tiles.Add(FWBTile(HeroTile.X + 1, HeroTile.Y));
	Tiles.Add(FWBTile(HeroTile.X - 1, HeroTile.Y));
	Tiles.Add(FWBTile(HeroTile.X, HeroTile.Y + 1));
	Tiles.Add(FWBTile(HeroTile.X, HeroTile.Y - 1));

	Tiles.RemoveAll([&State](const FWBTile& Tile)
	{
		return !IsTileInBounds(Tile) || State.IsTileOccupied(Tile);
	});

	Tiles.Sort([](const FWBTile& A, const FWBTile& B)
	{
		if (A.Y != B.Y)
		{
			return A.Y < B.Y;
		}
		return A.X < B.X;
	});
	return Tiles;
}

int32 CountOwnedBoardUnits(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	return State.GetUnitsForPlayer(PlayerId).Num();
}

TArray<const FWBUnitState*> GetSortedOwnedBoardUnits(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	TArray<const FWBUnitState*> Units = State.GetUnitsForPlayer(PlayerId);
	Units.Sort([](const FWBUnitState& A, const FWBUnitState& B)
	{
		if (A.OwnerId != B.OwnerId)
		{
			return A.OwnerId < B.OwnerId;
		}
		if (A.Y != B.Y)
		{
			return A.Y < B.Y;
		}
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		return A.UnitId < B.UnitId;
	});
	return Units;
}

void AppendSummonOptionForCharacter(
	FWBProductionSummonEquipDecisionData& Data,
	const FWBGameStateData& State,
	const FWBObservedCardRef& Card,
	const FWBCardDefinition& Definition,
	const int32 ViewerPlayerId)
{
	const FWBUnitState* Hero = FindHeroUnit(State, ViewerPlayerId);
	if (Hero == nullptr)
	{
		AddDiagnostic(Data, TEXT("hero_not_found"), Card.CardId, Card.InstanceId);
		return;
	}

	if (CountOwnedBoardUnits(State, ViewerPlayerId) >= MaxOwnedUnitsIncludingHero)
	{
		AddDiagnostic(Data, TEXT("unit_cap_reached"), Card.CardId, Card.InstanceId);
		return;
	}

	TArray<FWBTile> LegalTiles = BuildSummonTilesAdjacentToHero(State, *Hero);
	if (LegalTiles.IsEmpty())
	{
		AddDiagnostic(Data, TEXT("no_summon_tiles_available"), Card.CardId, Card.InstanceId, Hero->UnitId);
		return;
	}

	FWBProductionSummonOption Option;
	Option.SourceInstanceId = Card.InstanceId;
	Option.SourceCardId = Card.CardId;
	Option.PublicName = Definition.PublicName;
	Option.OwnerPlayerId = ViewerPlayerId;
	Option.LegalTiles = MoveTemp(LegalTiles);
	Option.CharacterStats = Definition.CharacterStats;
	Data.SummonOptions.Add(Option);
}

void AppendEquipOptionForWand(
	FWBProductionSummonEquipDecisionData& Data,
	const FWBGameStateData& State,
	const FWBObservedCardRef& Card,
	const FWBCardDefinition& Definition,
	const int32 ViewerPlayerId)
{
	const int32 RR = Definition.WandStats.RR;
	if (RR < 0)
	{
		AddDiagnostic(Data, TEXT("invalid_rr"), Card.CardId, Card.InstanceId);
		return;
	}

	FWBProductionEquipOption Option;
	Option.SourceInstanceId = Card.InstanceId;
	Option.SourceCardId = Card.CardId;
	Option.PublicName = Definition.PublicName;
	Option.OwnerPlayerId = ViewerPlayerId;
	Option.RR = RR;

	for (const FWBUnitState* Unit : GetSortedOwnedBoardUnits(State, ViewerPlayerId))
	{
		if (Unit == nullptr)
		{
			continue;
		}

		FWBResonanceLoadSummary LoadSummary;
		if (WBResonanceLoad::CanPayRR(State, Unit->UnitId, RR, LoadSummary))
		{
			Option.EligibleUnitIds.Add(Unit->UnitId);
		}
	}

	if (Option.EligibleUnitIds.IsEmpty())
	{
		AddDiagnostic(Data, TEXT("no_eligible_equip_unit"), Card.CardId, Card.InstanceId);
		return;
	}

	Data.EquipOptions.Add(Option);
}
}

FWBProductionSummonEquipDecisionData FWBProductionSummonEquipDataProvider::BuildDecisionData(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const int32 ViewerPlayerId) const
{
	FWBProductionSummonEquipDecisionData Data;
	Data.ViewerPlayerId = ViewerPlayerId;

	if (!FWBGameStateData::IsValidPlayerId(ViewerPlayerId)
		|| State.GetPlayerById(ViewerPlayerId) == nullptr)
	{
		AddDiagnostic(Data, TEXT("invalid_viewer_player"));
		return Data;
	}

	const FWBCardZonePlayerObservation Observation =
		WBCardZoneObservation::BuildObservationForPlayer(State, ViewerPlayerId);

	for (const FWBObservedCardRef& Card : Observation.OwnHand.Cards)
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Card.CardId);
		if (!Lookup.bFound)
		{
			AddDiagnostic(Data, TEXT("card_definition_not_found"), Card.CardId, Card.InstanceId);
			continue;
		}

		switch (Lookup.Definition.Kind)
		{
		case EWBCardDefinitionKind::Character:
			AppendSummonOptionForCharacter(Data, State, Card, Lookup.Definition, ViewerPlayerId);
			break;

		case EWBCardDefinitionKind::Wand:
			AppendEquipOptionForWand(Data, State, Card, Lookup.Definition, ViewerPlayerId);
			break;

		default:
			AddDiagnostic(Data, TEXT("unsupported_card_kind"), Card.CardId, Card.InstanceId);
			break;
		}
	}

	return Data;
}
