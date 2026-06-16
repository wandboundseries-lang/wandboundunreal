#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBMoveQueryResult
{
	bool bOk = false;
	FString Reason;
	int32 CostMP = 0;

	static FWBMoveQueryResult Ok(int32 InCostMP = 1);
	static FWBMoveQueryResult Deny(const TCHAR* InReason);
};

struct WANDBOUNDCORE_API FWBActionQueryResult
{
	bool bOk = false;
	FString Reason;

	static FWBActionQueryResult Ok();
	static FWBActionQueryResult Deny(const TCHAR* InReason);
};

class WANDBOUNDCORE_API WBRules
{
public:
	static bool IsTileInBounds(const FWBTile& Tile);
	static bool AreOrthogonallyAdjacent(const FWBTile& A, const FWBTile& B);
	static bool IsValidWallEdge(const FWBWallEdge& Edge);
	static FWBWallEdge NormalizeWallEdge(const FWBWallEdge& Edge);
	static bool AreSameWallEdge(const FWBWallEdge& A, const FWBWallEdge& B);
	static bool HasWallBetween(const FWBGameStateData& State, const FWBTile& A, const FWBTile& B);
	static bool IsTileOccupied(const FWBGameStateData& State, const FWBTile& Tile);
	static const FWBUnitState* GetUnitById(const FWBGameStateData& State, int32 UnitId);
	static FWBMoveQueryResult QueryMove(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult QueryEndTurn(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult QueryPass(const FWBGameStateData& State, const FWBAction& Action);
	static TArray<FWBAction> GenerateLegalMoveActions(const FWBGameStateData& State, int32 PlayerId, int32 UnitId);
	static TArray<FWBAction> GenerateLegalActions(const FWBGameStateData& State);
	static TArray<FWBAction> GenerateLegalActionsForPlayer(const FWBGameStateData& State, int32 PlayerId);
	static bool CanMoveUnit(const FWBGameStateData& State, const FWBAction& Action, FString& OutReason);
};
