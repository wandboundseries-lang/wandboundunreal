#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBRuntimeTurnResolutionContext
{
	bool bResolveEndTurnAsFullTransition = true;
	IWBMPRollSource* MPRollSource = nullptr;
};

class WANDBOUNDCORE_API WBRuntimeTurnResolutionAdapter
{
public:
	static FWBApplyActionResult ApplyRuntimeSelectedAction(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& Context);
};
