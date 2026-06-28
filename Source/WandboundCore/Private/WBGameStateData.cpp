#include "WBGameStateData.h"

#include "WBRules.h"

namespace
{
void AddUniqueStatusName(TArray<FName>& Names, const TCHAR* Name)
{
	Names.AddUnique(FName(Name));
}

TArray<FName> GetStatusNameAliases(const FName StatusId)
{
	TArray<FName> Names;
	if (!StatusId.IsNone())
	{
		Names.AddUnique(StatusId);
	}

	const FString LowerStatusId = StatusId.ToString().ToLower();
	if (LowerStatusId == TEXT("root") || LowerStatusId == TEXT("rooted"))
	{
		AddUniqueStatusName(Names, TEXT("Rooted"));
		AddUniqueStatusName(Names, TEXT("root"));
	}
	else if (LowerStatusId == TEXT("stun") || LowerStatusId == TEXT("stunned"))
	{
		AddUniqueStatusName(Names, TEXT("Stunned"));
		AddUniqueStatusName(Names, TEXT("stun"));
	}
	else if (LowerStatusId == TEXT("poison"))
	{
		AddUniqueStatusName(Names, TEXT("Poison"));
		AddUniqueStatusName(Names, TEXT("poison"));
	}
	else if (LowerStatusId == TEXT("burn"))
	{
		AddUniqueStatusName(Names, TEXT("Burn"));
		AddUniqueStatusName(Names, TEXT("burn"));
	}
	else if (LowerStatusId == TEXT("frozen"))
	{
		AddUniqueStatusName(Names, TEXT("Frozen"));
		AddUniqueStatusName(Names, TEXT("frozen"));
	}

	return Names;
}

bool TryFindActiveStatusKey(const FWBUnitState& Unit, const FName StatusId, FName& OutStatusKey)
{
	for (const FName& Alias : GetStatusNameAliases(StatusId))
	{
		if (Unit.Statuses.Contains(Alias))
		{
			OutStatusKey = Alias;
			return true;
		}
	}

	OutStatusKey = NAME_None;
	return false;
}

FWBPlayerStateData* FindOrAddTestPlayerState(FWBGameStateData& State, const int32 PlayerId, const int32 InitialRemainingMP)
{
	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		return nullptr;
	}

	FWBPlayerStateData* ExistingPlayer = State.GetMutablePlayerById(PlayerId);
	if (ExistingPlayer != nullptr)
	{
		ExistingPlayer->RemainingMP += FMath::Max(InitialRemainingMP, 0);
		return ExistingPlayer;
	}

	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = FMath::Max(InitialRemainingMP, 0);
	State.Players.Add(Player);
	return &State.Players.Last();
}
}

bool FWBUnitState::HasStatus(const FName StatusId) const
{
	for (const FName& Alias : GetStatusNameAliases(StatusId))
	{
		if (Statuses.Contains(Alias))
		{
			return true;
		}
	}

	return false;
}

bool FWBUnitState::IsUnitOnBoard() const
{
	return !bDefeated && !bRemovedFromBoard && X >= 0 && Y >= 0;
}

int32 FWBUnitState::GetMaxArmor() const
{
	return FMath::Max(MaxArmor, 0);
}

int32 FWBUnitState::GetCurrentArmor() const
{
	return FMath::Clamp(CurrentArmor, 0, GetMaxArmor());
}

void FWBUnitState::SetArmorForTest(const int32 InCurrentArmor, const int32 InMaxArmor)
{
	MaxArmor = FMath::Max(InMaxArmor, 0);
	CurrentArmor = FMath::Clamp(InCurrentArmor, 0, MaxArmor);
}

void FWBUnitState::MarkUnitDefeated()
{
	bDefeated = true;
}

void FWBUnitState::RemoveUnitFromBoard()
{
	bRemovedFromBoard = true;
	X = -1;
	Y = -1;
}

void FWBUnitState::AddStatus(const FName StatusId, const int32 TurnsRemaining)
{
	if (StatusId.IsNone())
	{
		return;
	}

	Statuses.Add(StatusId);
	if (TurnsRemaining > 0)
	{
		StatusTurnsRemaining.Add(StatusId, TurnsRemaining);
	}
	else
	{
		for (const FName& Alias : GetStatusNameAliases(StatusId))
		{
			StatusTurnsRemaining.Remove(Alias);
		}
	}
}

void FWBUnitState::RemoveStatus(const FName StatusId)
{
	for (const FName& Alias : GetStatusNameAliases(StatusId))
	{
		Statuses.Remove(Alias);
		StatusTurnsRemaining.Remove(Alias);
	}
}

int32 FWBUnitState::GetStatusTurnsRemaining(const FName StatusId) const
{
	for (const FName& Alias : GetStatusNameAliases(StatusId))
	{
		if (const int32* TurnsRemaining = StatusTurnsRemaining.Find(Alias))
		{
			return *TurnsRemaining;
		}
	}

	return 0;
}

void FWBUnitState::SetStatusTurnsRemaining(const FName StatusId, const int32 TurnsRemaining)
{
	FName ActiveStatusKey;
	if (!TryFindActiveStatusKey(*this, StatusId, ActiveStatusKey))
	{
		return;
	}

	for (const FName& Alias : GetStatusNameAliases(StatusId))
	{
		StatusTurnsRemaining.Remove(Alias);
	}

	if (TurnsRemaining > 0)
	{
		StatusTurnsRemaining.Add(ActiveStatusKey, TurnsRemaining);
	}
}

TArray<FName> FWBUnitState::GetSortedStatusIdsForTrace() const
{
	TArray<FName> SortedStatusIds = Statuses.Array();
	SortedStatusIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});

	return SortedStatusIds;
}

bool FWBGameStateData::IsValidPlayerId(const int32 PlayerId)
{
	return PlayerId == 0 || PlayerId == 1;
}

int32 FWBGameStateData::TileToIndex(const FWBTile& Tile)
{
	constexpr int32 BoardWidth = 9;
	return Tile.Y * BoardWidth + Tile.X;
}

int32 FWBGameStateData::GetCurrentPlayerId() const
{
	return CurrentPlayer;
}

int32 FWBGameStateData::GetActionPriorityPlayerId() const
{
	return PriorityPlayer;
}

const FWBPlayerStateData* FWBGameStateData::GetPlayerById(const int32 PlayerId) const
{
	for (const FWBPlayerStateData& Player : Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}

	return nullptr;
}

FWBPlayerStateData* FWBGameStateData::GetMutablePlayerById(const int32 PlayerId)
{
	for (FWBPlayerStateData& Player : Players)
	{
		if (Player.PlayerId == PlayerId)
		{
			return &Player;
		}
	}

	return nullptr;
}

const FWBPlayerStateData* FWBGameStateData::GetCurrentPlayer() const
{
	return GetPlayerById(CurrentPlayer);
}

FWBPlayerStateData* FWBGameStateData::GetMutableCurrentPlayer()
{
	return GetMutablePlayerById(CurrentPlayer);
}

const FWBCardZoneState& FWBGameStateData::GetCardZoneState() const
{
	return CardZoneState;
}

FWBCardZoneState& FWBGameStateData::GetMutableCardZoneStateForTest()
{
	return CardZoneState;
}

void FWBGameStateData::ClearCardZoneStateForTest()
{
	CardZoneState = FWBCardZoneState();
}

TArray<const FWBUnitState*> FWBGameStateData::GetUnitsForPlayer(const int32 PlayerId) const
{
	TArray<const FWBUnitState*> OwnedUnits;
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.OwnerId == PlayerId && Unit.IsUnitOnBoard())
		{
			OwnedUnits.Add(&Unit);
		}
	}

	return OwnedUnits;
}

TArray<FWBUnitState*> FWBGameStateData::GetMutableUnitsForPlayer(const int32 PlayerId)
{
	TArray<FWBUnitState*> OwnedUnits;
	for (FWBUnitState& Unit : Units)
	{
		if (Unit.OwnerId == PlayerId && Unit.IsUnitOnBoard())
		{
			OwnedUnits.Add(&Unit);
		}
	}

	return OwnedUnits;
}

bool FWBGameStateData::IsNormalTurnPhase() const
{
	return Phase == EWBGamePhase::NormalTurn;
}

bool FWBGameStateData::IsResponsePhase() const
{
	return Phase == EWBGamePhase::Response;
}

void FWBGameStateData::AdvanceTurnBasic()
{
	const int32 PreviousPlayer = CurrentPlayer;
	CurrentPlayer = PreviousPlayer == 0 ? 1 : 0;
	PriorityPlayer = CurrentPlayer;
	Phase = EWBGamePhase::NormalTurn;
	TurnNumber += 1;
	// TODO: MP dice rolls, card draw, and start-of-turn triggers are intentionally outside this deterministic baseline.
}

bool FWBGameStateData::ResetActionResourcesForPlayer(const int32 PlayerId, FString& OutReason)
{
	FWBPlayerStateData* Player = GetMutablePlayerById(PlayerId);
	if (Player == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	Player->WallsLeft = 1;
	Player->WallRemovalsLeft = TurnNumber >= 30 ? 1 : 0;

	for (FWBUnitState* Unit : GetMutableUnitsForPlayer(PlayerId))
	{
		if (Unit != nullptr)
		{
			Unit->AttacksLeft = FMath::Max(Unit->MaxAttacksPerTurn, 0);
		}
	}

	OutReason.Reset();
	return true;
}

bool FWBGameStateData::ApplyTurnStartResourceSetupForPlayer(
	const int32 PlayerId,
	const int32 ExplicitMPRoll,
	FString& OutReason)
{
	if (!IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}

	if (ExplicitMPRoll < 1 || ExplicitMPRoll > 6)
	{
		OutReason = TEXT("invalid_mp_roll");
		return false;
	}

	FWBPlayerStateData* Player = GetMutablePlayerById(PlayerId);
	if (Player == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	CurrentPlayer = PlayerId;
	PriorityPlayer = PlayerId;
	Phase = EWBGamePhase::NormalTurn;
	Player->LastMPRoll = ExplicitMPRoll;
	Player->RemainingMP = ExplicitMPRoll;
	ClearActivationUsageKeysForPlayer(PlayerId);

	if (!ResetActionResourcesForPlayer(PlayerId, OutReason))
	{
		return false;
	}

	OutReason.Reset();
	return true;
}

bool FWBGameStateData::HasActivationUsageKeyThisTurn(const int32 PlayerId, const FString& Key) const
{
	if (!IsValidPlayerId(PlayerId) || Key.IsEmpty())
	{
		return false;
	}

	const TSet<FString>* Keys = ActivationUsageKeysThisTurn.Find(PlayerId);
	return Keys != nullptr && Keys->Contains(Key);
}

void FWBGameStateData::MarkActivationUsageKeyForTest(const int32 PlayerId, const FString& Key)
{
	if (!IsValidPlayerId(PlayerId) || Key.IsEmpty())
	{
		return;
	}

	TSet<FString>& Keys = ActivationUsageKeysThisTurn.FindOrAdd(PlayerId);
	Keys.Add(Key);
}

void FWBGameStateData::ClearActivationUsageKeysForPlayer(const int32 PlayerId)
{
	if (!IsValidPlayerId(PlayerId))
	{
		return;
	}

	ActivationUsageKeysThisTurn.Remove(PlayerId);
}

bool FWBGameStateData::HasPendingAttack() const
{
	return PendingAttack.bActive;
}

void FWBGameStateData::ClearPendingAttack()
{
	PendingAttack = FWBPendingAttackState();
}

void FWBGameStateData::SetPendingAttackForTest(const FWBPendingAttackState& InPendingAttack)
{
	PendingAttack = InPendingAttack;
}

const FWBUnitState* FWBGameStateData::GetUnitById(const int32 UnitId) const
{
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.UnitId == UnitId)
		{
			return &Unit;
		}
	}

	return nullptr;
}

FWBUnitState* FWBGameStateData::GetMutableUnitById(const int32 UnitId)
{
	for (FWBUnitState& Unit : Units)
	{
		if (Unit.UnitId == UnitId)
		{
			return &Unit;
		}
	}

	return nullptr;
}

int32 FWBGameStateData::UnitIdAt(const FWBTile& Tile) const
{
	for (const FWBUnitState& Unit : Units)
	{
		if (Unit.IsUnitOnBoard() && FWBTile(Unit.X, Unit.Y) == Tile)
		{
			return Unit.UnitId;
		}
	}

	return -1;
}

bool FWBGameStateData::IsTileOccupied(const FWBTile& Tile) const
{
	return UnitIdAt(Tile) != -1;
}

bool FWBGameStateData::AddUnitForTest(const FWBUnitState& Unit)
{
	const FWBTile Tile(Unit.X, Unit.Y);
	if (Unit.UnitId < 0 || GetUnitById(Unit.UnitId) != nullptr)
	{
		return false;
	}

	if (!Unit.bRemovedFromBoard && !WBRules::IsTileInBounds(Tile))
	{
		return false;
	}

	if (Unit.IsUnitOnBoard() && IsTileOccupied(Tile))
	{
		return false;
	}

	FindOrAddTestPlayerState(*this, Unit.OwnerId, Unit.MPRemaining);
	FWBUnitState NormalizedUnit = Unit;
	NormalizedUnit.SetArmorForTest(Unit.CurrentArmor, Unit.MaxArmor);
	Units.Add(NormalizedUnit);
	return true;
}

void FWBGameStateData::AddWallForTest(const FWBWallEdge& Edge)
{
	if (!WBRules::IsValidWallEdge(Edge))
	{
		return;
	}

	const FWBWallEdge NormalizedEdge = WBRules::NormalizeWallEdge(Edge);
	for (const FWBWallEdge& ExistingEdge : Walls)
	{
		if (WBRules::AreSameWallEdge(ExistingEdge, NormalizedEdge))
		{
			return;
		}
	}

	Walls.Add(NormalizedEdge);
}

bool FWBGameStateData::RemoveWallForTest(const FWBWallEdge& Edge)
{
	for (int32 Index = 0; Index < Walls.Num(); ++Index)
	{
		if (WBRules::AreSameWallEdge(Walls[Index], Edge))
		{
			Walls.RemoveAt(Index);
			return true;
		}
	}

	return false;
}

FName FWBGameStateData::GetTerrainAt(const FWBTile& Tile) const
{
	if (!WBRules::IsTileInBounds(Tile))
	{
		return DefaultTerrainId;
	}

	const FName* TerrainId = TerrainByTileIndex.Find(TileToIndex(Tile));
	return TerrainId != nullptr ? *TerrainId : DefaultTerrainId;
}

void FWBGameStateData::SetTerrainForTest(const FWBTile& Tile, const FName TerrainId)
{
	if (!WBRules::IsTileInBounds(Tile) || TerrainId.IsNone())
	{
		return;
	}

	if (TerrainId.GetPlainNameString().Equals(DefaultTerrainId.GetPlainNameString(), ESearchCase::IgnoreCase))
	{
		ClearTerrainForTest(Tile);
		return;
	}

	TerrainByTileIndex.Add(TileToIndex(Tile), TerrainId);
}

void FWBGameStateData::ClearTerrainForTest(const FWBTile& Tile)
{
	if (!WBRules::IsTileInBounds(Tile))
	{
		return;
	}

	TerrainByTileIndex.Remove(TileToIndex(Tile));
}
