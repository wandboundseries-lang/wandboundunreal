#include "WBBoardSummaryBridge.h"

#include "WBBoardViewActor.h"

FWBPublicBoardSummary WBBoardSummaryBridge::BuildPublicSummaryFromState(const FWBGameStateData& State)
{
	return WBPublicBoardSummary::Build(State);
}

FWBBoardViewRefreshResult WBBoardSummaryBridge::RenderStateToBoardView(
	const FWBGameStateData& State,
	AWBBoardViewActor* BoardViewActor)
{
	return RenderSummaryToBoardView(BuildPublicSummaryFromState(State), BoardViewActor);
}

FWBBoardViewRefreshResult WBBoardSummaryBridge::RenderSummaryToBoardView(
	const FWBPublicBoardSummary& Summary,
	AWBBoardViewActor* BoardViewActor)
{
	FWBBoardViewRefreshResult Result;
	if (BoardViewActor == nullptr)
	{
		return Result;
	}

	BoardViewActor->RenderPublicBoardSummary(Summary);

	Result.bRendered = true;
	Result.TileCount = BoardViewActor->GetRenderedTileCount();
	Result.UnitCount = BoardViewActor->GetRenderedUnitCount();
	Result.WallCount = BoardViewActor->GetRenderedWallCount();
	Result.TerrainCount = BoardViewActor->GetRenderedTerrainCount();
	return Result;
}
