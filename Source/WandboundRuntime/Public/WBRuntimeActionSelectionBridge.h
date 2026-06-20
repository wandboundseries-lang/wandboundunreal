#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBSelectedActionVisualHarness.h"
#include "WBRuntimeTurnResolutionAdapter.h"

class UWBRuntimeControllerFacadeComponent;

struct WANDBOUNDRUNTIME_API FWBResolvedRuntimeActionSelection
{
	bool bOk = false;
	FString Reason;
	FString SelectedActionId;
	FWBAction ResolvedAction;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActionSelectionExecutionResult
{
	FWBResolvedRuntimeActionSelection Selection;
	FWBSelectedActionVisualHarnessResult Execution;
};

class WANDBOUNDRUNTIME_API WBRuntimeActionSelectionBridge
{
public:
	static FWBResolvedRuntimeActionSelection ResolveSelectedActionId(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FString& SelectedActionId);

	static FWBRuntimeActionSelectionExecutionResult ExecuteSelectedActionId(
		FWBGameStateData& State,
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FString& SelectedActionId,
		FWBRuntimeTurnResolutionContext& RuntimeContext,
		UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade);
};
