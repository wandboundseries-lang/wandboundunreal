#include "WBProductionSummonEquipDataProvider.h"

#include "WBCardZoneObservation.h"
#include "WBGameStateData.h"
#include "WBResonanceRecalculation.h"

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
	const FWBCardDefinitionRepository& Repository,
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

		const FWBResonanceRecalculationResult RLResult =
			WBResonanceRecalculation::CalculateUnit(State, Unit->UnitId, Repository);
		if (!RLResult.bSucceeded)
		{
			AddDiagnostic(Data, TEXT("rl_recalculation_failed"), Card.CardId, Card.InstanceId, Unit->UnitId);
			continue;
		}

		if (!RLResult.bIsOverflowing && RR <= RLResult.AvailableRL)
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

FName PublicCardTypeName(const EWBCardDefinitionKind Kind)
{
	switch (Kind)
	{
	case EWBCardDefinitionKind::Character: return FName(TEXT("Character"));
	case EWBCardDefinitionKind::Wand: return FName(TEXT("Wand"));
	case EWBCardDefinitionKind::Action: return FName(TEXT("Action"));
	case EWBCardDefinitionKind::Trap: return FName(TEXT("Trap"));
	case EWBCardDefinitionKind::NPC: return FName(TEXT("NPC"));
	default: return FName(TEXT("Card"));
	}
}

FString PublicTargetPrompt(
	const EWBCardEffectTargetRequirement Requirement)
{
	switch (Requirement)
	{
	case EWBCardEffectTargetRequirement::Unit: return TEXT("Choose a unit");
	case EWBCardEffectTargetRequirement::Tile: return TEXT("Choose a tile");
	case EWBCardEffectTargetRequirement::WallEdge: return TEXT("Choose a wall");
	default: return FString();
	}
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
			AppendEquipOptionForWand(Data, State, Repository, Card, Lookup.Definition, ViewerPlayerId);
			break;

		default:
			AddDiagnostic(Data, TEXT("unsupported_card_kind"), Card.CardId, Card.InstanceId);
			break;
		}
	}

	return Data;
}

bool FWBProductionSummonEquipDataProvider::GetPublicDefinitionData(
	const FWBCardDefinitionRepository& Repository,
	const FString& DefinitionId,
	FWBProductionPublicDefinitionData& OutData) const
{
	OutData = FWBProductionPublicDefinitionData();
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, DefinitionId);
	if (!Lookup.bFound)
	{
		return false;
	}
	OutData.DefinitionId = Lookup.Definition.CardId;
	OutData.DisplayName = Lookup.Definition.PublicName;
	OutData.CardType = PublicCardTypeName(Lookup.Definition.Kind);
	OutData.PublicCategory = Lookup.Definition.PublicCategory;
	OutData.PublicFactions = Lookup.Definition.PublicFactions;
	OutData.PublicTags = Lookup.Definition.PublicTags;
	OutData.PublicRulesText = Lookup.Definition.PublicRulesText;
	OutData.RR = Lookup.Definition.WandStats.RR;
	return true;
}

bool FWBProductionSummonEquipDataProvider::GetPublicEffectData(
	const FWBCardDefinitionRepository& Repository,
	const FString& DefinitionId,
	const FString& EffectId,
	FWBProductionPublicEffectData& OutData) const
{
	OutData = FWBProductionPublicEffectData();
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, DefinitionId);
	if (!Lookup.bFound)
	{
		return false;
	}
	const FWBCardEffectDefinition* Effect =
		Lookup.Definition.ActivatedEffects.FindByPredicate(
			[&EffectId](const FWBCardEffectDefinition& Candidate)
			{
				return Candidate.EffectId == EffectId;
			});
	if (Effect == nullptr)
	{
		return false;
	}
	OutData.PublicLabel = Effect->PublicLabel;
	OutData.PublicTargetPrompt =
		PublicTargetPrompt(Effect->TargetRequirement);
	return true;
}
