#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBBoardSummaryBridge.h"
#include "WBGameStateData.h"
#include "WBRuntimeTurnResolutionAdapter.h"

class UWBRuntimeVisualControllerComponent;

struct WANDBOUNDRUNTIME_API FWBSelectedActionVisualHarnessOptions
{
	bool bRefreshVisualsOnSuccess = true;
	bool bRefreshVisualsOnFailure = false;
};

struct WANDBOUNDRUNTIME_API FWBSelectedActionVisualHarnessResult
{
	FWBRuntimeSelectedActionResult RuntimeResult;
	FWBBoardViewRefreshResult VisualRefreshResult;
};

class WANDBOUNDRUNTIME_API WBSelectedActionVisualHarness
{
public:
	static FWBSelectedActionVisualHarnessResult ApplySelectedActionAndRefreshVisuals(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& RuntimeContext,
		UWBRuntimeVisualControllerComponent* VisualController,
		const FWBSelectedActionVisualHarnessOptions& Options = FWBSelectedActionVisualHarnessOptions());
};
