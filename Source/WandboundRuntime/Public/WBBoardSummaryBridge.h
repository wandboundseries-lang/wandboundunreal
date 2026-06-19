#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"

class AWBBoardViewActor;

struct WANDBOUNDRUNTIME_API FWBBoardViewRefreshResult
{
	bool bRendered = false;
	int32 TileCount = 0;
	int32 UnitCount = 0;
	int32 WallCount = 0;
	int32 TerrainCount = 0;
};

class WANDBOUNDRUNTIME_API WBBoardSummaryBridge
{
public:
	static FWBPublicBoardSummary BuildPublicSummaryFromState(const FWBGameStateData& State);

	static FWBBoardViewRefreshResult RenderStateToBoardView(
		const FWBGameStateData& State,
		AWBBoardViewActor* BoardViewActor);

	static FWBBoardViewRefreshResult RenderSummaryToBoardView(
		const FWBPublicBoardSummary& Summary,
		AWBBoardViewActor* BoardViewActor);
};
