#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBSelectedActionExecutionContext
{
	bool bUseFullTurnTransitionForEndTurn = false;
	int32 NextPlayerExplicitMPRoll = 0;
};

class WANDBOUNDCORE_API WBSelectedActionExecutor
{
public:
	static FWBApplyActionResult ApplySelectedAction(
		FWBGameStateData& State,
		const FWBAction& Action,
		const FWBSelectedActionExecutionContext& Context);
};
