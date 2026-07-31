#pragma once

#include "CoreMinimal.h"
#include "WBTypes.h"

enum class EWBPlayerRelativeBoardRegion : uint8
{
	Invalid,
	OwnHalf,
	NeutralRow,
	OpponentHalf
};

class WANDBOUNDCORE_API WBBoardRegion
{
public:
	static EWBPlayerRelativeBoardRegion GetBoardRegionForPlayer(
		int32 PlayerId,
		const FWBTile& Tile);

	static bool IsOwnHalfForPlayer(int32 PlayerId, const FWBTile& Tile);
	static bool IsNeutralRow(const FWBTile& Tile);
	static bool IsOpponentHalfForPlayer(int32 PlayerId, const FWBTile& Tile);
};
