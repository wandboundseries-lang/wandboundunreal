#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBCardActivationCandidate.h"
#include "WBCardActivationLegalAction.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeActivationDecisionSessionFacadeComponent.h"

struct FWBActivationSessionExternalDataForTest
{
	TArray<FWBAction> NormalLegalActions;
	FWBCardActivationLegalActionSet ActivationActionSet;
	FWBPublicBoardSummary PublicBoardSummary;
};

struct FWBActivationSessionHarnessStepResult
{
	bool bRefreshOk = false;
	bool bExecutionOk = false;

	FString SelectedActivationActionId;

	FWBRuntimeActivationDecisionSessionRefreshResult InitialRefreshResult;
	FWBRuntimeActivationDecisionSessionExecutionResult ExecutionResult;

	FWBActivationSessionExternalDataForTest PreActionExternalData;
	FWBActivationSessionExternalDataForTest PostActionExternalData;
};

// Test-only helper. This simulates an external rules owner for automation tests.
// Do not include this from WandboundRuntime or production code.
class FWBActivationSessionTestHarness
{
public:
	static FWBActivationSessionExternalDataForTest BuildExternalDataForTest(
		const FWBGameStateData& State,
		int32 PlayerId,
		const TArray<FWBCardActivationCandidateSource>& ActivationSources);

	static bool FindFirstActivationActionIdWithLabel(
		const FWBCardActivationLegalActionSet& ActionSet,
		const FString& LabelContains,
		FString& OutActivationActionId);

	static FWBActivationSessionHarnessStepResult RefreshAndExecuteFirstActivationWithLabel(
		FWBGameStateData& State,
		int32 PlayerId,
		const TArray<FWBCardActivationCandidateSource>& ActivationSources,
		const FString& LabelContains,
		UWBRuntimeActivationDecisionSessionFacadeComponent* SessionFacade);
};
