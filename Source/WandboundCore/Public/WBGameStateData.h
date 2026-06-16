#pragma once

#include "CoreMinimal.h"
#include "WBTypes.h"

struct WANDBOUNDCORE_API FWBUnitState
{
	int32 UnitId = -1;
	int32 OwnerId = -1;
	FString CardId;
	int32 X = -1;
	int32 Y = -1;
	int32 HP = 1;
	int32 MaxHP = 1;
	int32 ATK = 1;
	int32 AR = 1;
	int32 RLTotal = 0;
	int32 RLUsed = 0;
	int32 AttacksLeft = 0;
	int32 MPRemaining = 0;
	TSet<FName> Statuses;
	TSet<FName> Passives;
};

struct WANDBOUNDCORE_API FWBPlayerStateData
{
	int32 PlayerId = -1;
	int32 HeroUnitId = -1;
	int32 WallsLeft = 0;
	TArray<FString> Deck;
	TArray<FString> Hand;
	TArray<FString> Discard;
};

struct WANDBOUNDCORE_API FWBGameStateData
{
	int32 CurrentPlayer = 0;
	int32 PriorityPlayer = 0;
	int32 TurnNumber = 1;
	TArray<FWBUnitState> Units;
	TArray<FWBWallEdge> Walls;
	TArray<FWBPlayerStateData> Players;

	const FWBUnitState* GetUnitById(int32 UnitId) const;
	FWBUnitState* GetMutableUnitById(int32 UnitId);
	int32 UnitIdAt(const FWBTile& Tile) const;
	bool IsTileOccupied(const FWBTile& Tile) const;
	bool AddUnitForTest(const FWBUnitState& Unit);
	void AddWallForTest(const FWBWallEdge& Edge);
	bool RemoveWallForTest(const FWBWallEdge& Edge);
};
