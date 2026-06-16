#include "WBGameStateData.h"

#include "WBRules.h"

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
