#include "WBPublicBoardSummary.h"

#include "WBRules.h"

namespace
{
constexpr int32 PublicBoardWidth = 9;
constexpr int32 PublicBoardHeight = 9;

FString CanonicalPublicStatusIdString(const FName StatusId)
{
	const FString LowerStatusId = StatusId.GetPlainNameString().ToLower();
	if (LowerStatusId == TEXT("burn") || LowerStatusId.StartsWith(TEXT("burn_")))
	{
		return TEXT("Burn");
	}

	if (LowerStatusId == TEXT("poison") || LowerStatusId.StartsWith(TEXT("poison_")))
	{
		return TEXT("Poison");
	}

	if (LowerStatusId == TEXT("root")
		|| LowerStatusId.StartsWith(TEXT("root_"))
		|| LowerStatusId == TEXT("rooted")
		|| LowerStatusId.StartsWith(TEXT("rooted_")))
	{
		return TEXT("Rooted");
	}

	if (LowerStatusId == TEXT("stun")
		|| LowerStatusId.StartsWith(TEXT("stun_"))
		|| LowerStatusId == TEXT("stunned")
		|| LowerStatusId.StartsWith(TEXT("stunned_")))
	{
		return TEXT("Stunned");
	}

	if (LowerStatusId == TEXT("frozen") || LowerStatusId.StartsWith(TEXT("frozen_")))
	{
		return TEXT("Frozen");
	}

	return FString();
}

TArray<FWBPublicUnitStatusSummary> BuildPublicStatusSummaries(const FWBUnitState& Unit)
{
	TMap<FString, int32> TurnsByStatusId;
	for (const FName& RawStatusId : Unit.Statuses)
	{
		const FString PublicStatusId = CanonicalPublicStatusIdString(RawStatusId);
		if (PublicStatusId.IsEmpty())
		{
			continue;
		}

		const int32 TurnsRemaining = Unit.GetStatusTurnsRemaining(RawStatusId);
		if (int32* ExistingTurns = TurnsByStatusId.Find(PublicStatusId))
		{
			*ExistingTurns = FMath::Max(*ExistingTurns, TurnsRemaining);
		}
		else
		{
			TurnsByStatusId.Add(PublicStatusId, TurnsRemaining);
		}
	}

	TArray<FWBPublicUnitStatusSummary> Statuses;
	Statuses.Reserve(TurnsByStatusId.Num());
	for (const TPair<FString, int32>& Pair : TurnsByStatusId)
	{
		FWBPublicUnitStatusSummary Status;
		Status.StatusId = FName(*Pair.Key);
		Status.TurnsRemaining = Pair.Value;
		Statuses.Add(Status);
	}

	Statuses.Sort([](const FWBPublicUnitStatusSummary& A, const FWBPublicUnitStatusSummary& B)
	{
		const FString AId = A.StatusId.GetPlainNameString();
		const FString BId = B.StatusId.GetPlainNameString();
		if (AId != BId)
		{
			return AId < BId;
		}

		return A.TurnsRemaining < B.TurnsRemaining;
	});

	return Statuses;
}

FWBPublicUnitBoardSummary BuildPublicUnitSummary(const FWBUnitState& Unit)
{
	FWBPublicUnitBoardSummary Summary;
	Summary.UnitId = Unit.UnitId;
	Summary.OwnerId = Unit.OwnerId;
	Summary.CardId = Unit.CardId;
	Summary.X = Unit.X;
	Summary.Y = Unit.Y;
	Summary.HP = Unit.HP;
	Summary.MaxHP = Unit.MaxHP;
	Summary.CurrentArmor = Unit.GetCurrentArmor();
	Summary.MaxArmor = Unit.GetMaxArmor();
	Summary.ATK = Unit.ATK;
	Summary.AR = Unit.AR;
	Summary.RLTotal = Unit.RLTotal;
	Summary.RLUsed = Unit.RLUsed;
	Summary.AttacksLeft = Unit.AttacksLeft;
	Summary.Statuses = BuildPublicStatusSummaries(Unit);
	return Summary;
}

FString CanonicalPublicTerrainIdString(const FName TerrainId)
{
	const FString LowerTerrainId = TerrainId.GetPlainNameString().ToLower();
	if (LowerTerrainId == TEXT("normal") || LowerTerrainId == TEXT("none") || LowerTerrainId.IsEmpty())
	{
		return TEXT("Normal");
	}

	if (LowerTerrainId == TEXT("mud"))
	{
		return TEXT("mud");
	}

	if (LowerTerrainId == TEXT("lava"))
	{
		return TEXT("lava");
	}

	if (LowerTerrainId == TEXT("ice"))
	{
		return TEXT("ice");
	}

	if (LowerTerrainId == TEXT("water"))
	{
		return TEXT("water");
	}

	return TerrainId.GetPlainNameString();
}

FName CanonicalPublicTerrainId(const FName TerrainId)
{
	return FName(*CanonicalPublicTerrainIdString(TerrainId));
}

FName WallOrientationForEdge(const FWBWallEdge& Edge)
{
	if (Edge.A.X == Edge.B.X && Edge.A.Y != Edge.B.Y)
	{
		return FName(TEXT("vertical"));
	}

	if (Edge.A.Y == Edge.B.Y && Edge.A.X != Edge.B.X)
	{
		return FName(TEXT("horizontal"));
	}

	return NAME_None;
}

TArray<FWBPublicWallEdgeSummary> BuildPublicWallSummaries(const FWBGameStateData& State)
{
	TArray<FWBPublicWallEdgeSummary> Walls;
	Walls.Reserve(State.Walls.Num());
	for (const FWBWallEdge& Wall : State.Walls)
	{
		if (!WBRules::IsValidWallEdge(Wall))
		{
			continue;
		}

		const FWBWallEdge NormalizedWall = WBRules::NormalizeWallEdge(Wall);
		const FName Orientation = WallOrientationForEdge(NormalizedWall);
		if (Orientation.IsNone())
		{
			continue;
		}

		FWBPublicWallEdgeSummary Summary;
		Summary.AX = NormalizedWall.A.X;
		Summary.AY = NormalizedWall.A.Y;
		Summary.BX = NormalizedWall.B.X;
		Summary.BY = NormalizedWall.B.Y;
		Summary.Orientation = Orientation;
		Walls.Add(Summary);
	}

	Walls.Sort([](const FWBPublicWallEdgeSummary& A, const FWBPublicWallEdgeSummary& B)
	{
		if (A.AY != B.AY)
		{
			return A.AY < B.AY;
		}

		if (A.AX != B.AX)
		{
			return A.AX < B.AX;
		}

		if (A.BY != B.BY)
		{
			return A.BY < B.BY;
		}

		if (A.BX != B.BX)
		{
			return A.BX < B.BX;
		}

		return A.Orientation.GetPlainNameString() < B.Orientation.GetPlainNameString();
	});

	return Walls;
}

TArray<FWBPublicTerrainTileSummary> BuildPublicTerrainSummaries(const FWBGameStateData& State)
{
	const FString DefaultTerrainId = CanonicalPublicTerrainIdString(State.DefaultTerrainId);
	TArray<FWBPublicTerrainTileSummary> TerrainTiles;
	TerrainTiles.Reserve(State.TerrainByTileIndex.Num());

	for (const TPair<int32, FName>& TerrainPair : State.TerrainByTileIndex)
	{
		const int32 TileIndex = TerrainPair.Key;
		const FWBTile Tile(TileIndex % PublicBoardWidth, TileIndex / PublicBoardWidth);
		if (!WBRules::IsTileInBounds(Tile))
		{
			continue;
		}

		const FString TerrainId = CanonicalPublicTerrainIdString(TerrainPair.Value);
		if (TerrainId == DefaultTerrainId)
		{
			continue;
		}

		FWBPublicTerrainTileSummary Summary;
		Summary.X = Tile.X;
		Summary.Y = Tile.Y;
		Summary.TerrainId = FName(*TerrainId);
		TerrainTiles.Add(Summary);
	}

	TerrainTiles.Sort([](const FWBPublicTerrainTileSummary& A, const FWBPublicTerrainTileSummary& B)
	{
		if (A.Y != B.Y)
		{
			return A.Y < B.Y;
		}

		if (A.X != B.X)
		{
			return A.X < B.X;
		}

		return A.TerrainId.GetPlainNameString() < B.TerrainId.GetPlainNameString();
	});

	return TerrainTiles;
}
}

FWBPublicBoardSummary WBPublicBoardSummary::Build(const FWBGameStateData& State)
{
	FWBPublicBoardSummary Summary;
	Summary.BoardWidth = PublicBoardWidth;
	Summary.BoardHeight = PublicBoardHeight;
	Summary.DefaultTerrainId = CanonicalPublicTerrainId(State.DefaultTerrainId);
	Summary.Units.Reserve(State.Units.Num());
	for (const FWBUnitState& Unit : State.Units)
	{
		if (!Unit.IsUnitOnBoard())
		{
			continue;
		}

		Summary.Units.Add(BuildPublicUnitSummary(Unit));
	}

	Summary.Units.Sort([](const FWBPublicUnitBoardSummary& A, const FWBPublicUnitBoardSummary& B)
	{
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

	Summary.Walls = BuildPublicWallSummaries(State);
	Summary.TerrainTiles = BuildPublicTerrainSummaries(State);

	return Summary;
}
