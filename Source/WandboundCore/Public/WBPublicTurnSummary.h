#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBPublicPlayerTurnSummary
{
	int32 PlayerId = -1;
	int32 RemainingMP = 0;
	int32 LastMPRoll = 0;
	int32 WallsLeft = 0;
	int32 WallRemovalsLeft = 0;
};

struct WANDBOUNDCORE_API FWBPublicTurnSummary
{
	int32 CurrentPlayerId = -1;
	int32 PriorityPlayerId = -1;
	int32 TurnNumber = 0;
	FName Phase;
	bool bGameOver = false;
	TArray<FWBPublicPlayerTurnSummary> Players;
};

class WANDBOUNDCORE_API WBPublicTurnSummary
{
public:
	static FWBPublicTurnSummary Build(const FWBGameStateData& State);
};
