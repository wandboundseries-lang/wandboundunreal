#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeActionSelectionBridge.h"
#include "WBRuntimeInteractionRefreshAdapter.h"
#include "WBRuntimeLegalActionPresentation.h"

class UWBRuntimeControllerFacadeComponent;
class UWBRuntimeDecisionPointCoordinatorComponent;
struct FWBRuntimeTurnResolutionContext;

struct FWBDecisionLoopTestHarnessStepResult
{
	bool bRefreshOk = false;
	bool bExecutionOk = false;
	FString SelectedActionId;
	FWBRuntimeInteractionRefreshResult RefreshResult;
	FWBRuntimeActionSelectionExecutionResult ExecutionResult;
};

// Test-only helper. This harness simulates an external rules/runtime owner for automation tests.
// Do not include this from WandboundRuntime or production code.
class FWBDecisionLoopTestHarness
{
public:
	static TArray<FWBAction> GenerateExternalLegalActionsForTest(
		const FWBGameStateData& State,
		int32 PlayerId);

	static FWBPublicBoardSummary BuildExternalPublicSummaryForTest(
		const FWBGameStateData& State);

	static bool FindFirstPresentedActionIdOfType(
		const FWBRuntimeLegalActionPresentationSnapshot& Snapshot,
		EWBRuntimeActionPresentationType Type,
		FString& OutActionId);

	static FWBDecisionLoopTestHarnessStepResult RefreshAndExecuteFirstActionOfType(
		FWBGameStateData& State,
		int32 PlayerId,
		EWBRuntimeActionPresentationType Type,
		FWBRuntimeTurnResolutionContext& RuntimeContext,
		UWBRuntimeDecisionPointCoordinatorComponent* Coordinator,
		UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade);
};
