#include "WBGameStateData.h"

#include "WBRules.h"

bool FWBGameStateData::IsValidPlayerId(const int32 PlayerId)
{
	return PlayerId == 0 || PlayerId == 1;
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
		if (FWBTile(Unit.X, Unit.Y) == Tile)
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
	if (Unit.UnitId < 0 || !WBRules::IsTileInBounds(Tile) || IsTileOccupied(Tile) || GetUnitById(Unit.UnitId) != nullptr)
	{
		return false;
	}

	Units.Add(Unit);
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
