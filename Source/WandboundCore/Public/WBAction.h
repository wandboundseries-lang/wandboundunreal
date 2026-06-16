#pragma once

#include "CoreMinimal.h"
#include "WBTypes.h"

enum class EWBActionType : uint8
{
	None,
	Move,
	Pass,
	EndTurn
};

struct WANDBOUNDCORE_API FWBAction
{
	EWBActionType Type = EWBActionType::None;
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	FWBTile FromTile;
	FWBTile ToTile;
};
