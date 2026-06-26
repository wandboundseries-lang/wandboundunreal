#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBCardActivationLegalAction.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeActivationExecutionBridge.h"
#include "WBRuntimeActivationPresentationRefreshAdapter.h"
#include "WBRuntimeInteractionRefreshAdapter.h"

class UWBRuntimeDecisionPointOwnerComponent;

struct WANDBOUNDRUNTIME_API FWBRuntimePostActivationRefreshOptions
{
	bool bRefreshNormalDecisionPointOnSuccess = true;
	bool bRefreshActivationPresentationOnSuccess = true;

	bool bRefreshNormalDecisionPointOnFailure = false;
	bool bRefreshActivationPresentationOnFailure = false;
};

struct WANDBOUNDRUNTIME_API FWBRuntimePostActivationRefreshInput
{
	TArray<FWBAction> PostActionPrecomputedLegalActions;
	FWBCardActivationLegalActionSet PostActionActivationActionSet;
	FWBPublicBoardSummary PostActionPublicBoardSummary;
};

struct WANDBOUNDRUNTIME_API FWBRuntimePostActivationRefreshResult
{
	bool bOk = false;
	FString Reason;

	bool bActivationExecutionAttempted = false;
	bool bActivationExecutionOk = false;

	bool bPostActionRefreshAttempted = false;
	bool bNormalDecisionPointRefreshed = false;
	bool bActivationPresentationRefreshed = false;

	FWBRuntimeActivationExecutionResult ActivationExecutionResult;
	FWBRuntimeInteractionRefreshResult NormalDecisionRefreshResult;
	FWBRuntimeActivationPresentationRefreshResult ActivationPresentationRefreshResult;
};

class WANDBOUNDRUNTIME_API WBRuntimePostActivationRefreshSequencer
{
public:
	static FWBRuntimePostActivationRefreshResult ExecuteActivationAndRefreshFromExternalData(
		FWBGameStateData& State,
		const FString& SelectedActivationActionId,
		UWBRuntimeDecisionPointOwnerComponent* Owner,
		const FWBRuntimePostActivationRefreshInput& PostActionRefreshInput,
		const FWBRuntimePostActivationRefreshOptions& Options = FWBRuntimePostActivationRefreshOptions());
};
