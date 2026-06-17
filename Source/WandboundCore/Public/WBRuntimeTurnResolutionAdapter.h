#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBRuntimeTurnResolutionContext
{
	bool bResolveEndTurnAsFullTransition = true;
	IWBMPRollSource* MPRollSource = nullptr;
};

struct WANDBOUNDCORE_API FWBRuntimeSelectedActionResult
{
	FWBApplyActionResult ApplyResult;
	FName SelectedActionType;
	FString SelectedActionId;
	bool bConsumedMPRoll = false;
	int32 ConsumedMPRoll = 0;
	FWBPublicTurnSummary FinalPublicTurnSummary;
};

class WANDBOUNDCORE_API WBRuntimeTurnResolutionAdapter
{
public:
	static FWBApplyActionResult ApplyRuntimeSelectedAction(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& Context);

	static FWBRuntimeSelectedActionResult ApplyRuntimeSelectedActionWithResult(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& Context);
};
