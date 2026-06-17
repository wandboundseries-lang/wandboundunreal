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
	static int32 OrthogonalDistance(const FWBTile& A, const FWBTile& B);
	static bool AreTilesOrthogonallyAligned(const FWBTile& A, const FWBTile& B);
	static bool HasOrthogonalLineOfSight(const FWBGameStateData& State, const FWBTile& From, const FWBTile& To, FString& OutReason);
	static FWBMoveQueryResult QueryMove(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult CanDeclareAttack(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult QueryEndTurn(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult QueryPass(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult QueryPassResponse(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult CanEndTurn(const FWBGameStateData& State, const FWBAction& Action);
	static FWBActionQueryResult CanPassResponse(const FWBGameStateData& State, const FWBAction& Action);
	static bool CanApplyTurnStartResourceSetup(const FWBGameStateData& State, int32 PlayerId, int32 ExplicitMPRoll, FString& OutReason);
	static bool CanApplyStartOfTurnStatusTicks(const FWBGameStateData& State, int32 PlayerId, FString& OutReason);
	static bool CanApplyEndOfTurnStatusTicks(const FWBGameStateData& State, int32 PlayerId, FString& OutReason);
	static bool CanApplyDeterministicTurnTransition(const FWBGameStateData& State, int32 EndingPlayerId, int32 NextPlayerExplicitMPRoll, FString& OutReason);
	static TArray<FWBAction> GenerateLegalMoveActions(const FWBGameStateData& State, int32 PlayerId, int32 UnitId);
	static void GenerateLegalActions(const FWBGameStateData& State, int32 PlayerId, TArray<FWBAction>& OutActions);
	static TArray<FWBAction> GenerateLegalActions(const FWBGameStateData& State);
	static TArray<FWBAction> GenerateLegalActionsForPlayer(const FWBGameStateData& State, int32 PlayerId);
	static bool CanMoveUnit(const FWBGameStateData& State, const FWBAction& Action, FString& OutReason);
};
