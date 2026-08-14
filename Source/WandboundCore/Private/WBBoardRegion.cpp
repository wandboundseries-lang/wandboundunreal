#include "WBBoardRegion.h"

#include "WBGameStateData.h"

namespace
{
constexpr int32 BoardRegionBoardSize = 9;
constexpr int32 NeutralRow = BoardRegionBoardSize / 2;

bool IsInBounds(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.X < BoardRegionBoardSize
		&& Tile.Y >= 0 && Tile.Y < BoardRegionBoardSize;
}
}

EWBPlayerRelativeBoardRegion WBBoardRegion::GetBoardRegionForPlayer(
	const int32 PlayerId,
	const FWBTile& Tile)
{
	if (!FWBGameStateData::IsValidPlayerId(PlayerId) || !IsInBounds(Tile))
	{
		return EWBPlayerRelativeBoardRegion::Invalid;
	}
	if (Tile.Y == NeutralRow)
	{
		return EWBPlayerRelativeBoardRegion::NeutralRow;
	}

	const bool bPlayerZeroOwnHalf = Tile.Y > NeutralRow;
	const bool bOwnHalf = PlayerId == 0
		? bPlayerZeroOwnHalf
		: !bPlayerZeroOwnHalf;
	return bOwnHalf
		? EWBPlayerRelativeBoardRegion::OwnHalf
		: EWBPlayerRelativeBoardRegion::OpponentHalf;
}

bool WBBoardRegion::IsOwnHalfForPlayer(
	const int32 PlayerId,
	const FWBTile& Tile)
{
	return GetBoardRegionForPlayer(PlayerId, Tile)
		== EWBPlayerRelativeBoardRegion::OwnHalf;
}

bool WBBoardRegion::IsNeutralRow(const FWBTile& Tile)
{
	return IsInBounds(Tile) && Tile.Y == NeutralRow;
}

bool WBBoardRegion::IsOpponentHalfForPlayer(
	const int32 PlayerId,
	const FWBTile& Tile)
{
	return GetBoardRegionForPlayer(PlayerId, Tile)
		== EWBPlayerRelativeBoardRegion::OpponentHalf;
}
