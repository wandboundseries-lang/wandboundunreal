#include "WBRuntimeActivationSessionTestHarness.h"

#include "WBCardActivationCandidateGenerator.h"
#include "WBCardActivationLegalActionGenerator.h"
#include "WBPublicBoardSummary.h"
#include "WBRules.h"
#include "WBRuntimeActivationExecutionBridge.h"

namespace
{
FWBRuntimeActivationDecisionSessionRefreshInput ToSessionRefreshInput(
	const FWBActivationSessionExternalDataForTest& ExternalData)
{
	FWBRuntimeActivationDecisionSessionRefreshInput Input;
	Input.NormalLegalActions = ExternalData.NormalLegalActions;
	Input.ActivationActionSet = ExternalData.ActivationActionSet;
	Input.PublicBoardSummary = ExternalData.PublicBoardSummary;
	return Input;
}
}

FWBActivationSessionExternalDataForTest FWBActivationSessionTestHarness::BuildExternalDataForTest(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const TArray<FWBCardActivationCandidateSource>& ActivationSources)
{
	FWBActivationSessionExternalDataForTest Result;

	// Test-only external rules owner simulation. Production runtime must not call this.
	Result.NormalLegalActions = WBRules::GenerateLegalActionsForPlayer(State, PlayerId);

	// Test-only external public summary provider simulation. Production runtime receives summaries from elsewhere.
	Result.PublicBoardSummary = WBPublicBoardSummary::Build(State);

	// Test-only external activation owner simulation. Production runtime must not generate candidates/actions.
	const FWBCardActivationCandidateGenerationResult CandidateResult =
		WBCardActivationCandidateGenerator::GenerateCandidates(State, ActivationSources);
	if (!CandidateResult.bOk)
	{
		return Result;
	}

	const FWBCardActivationLegalActionGenerationResult LegalActionResult =
		WBCardActivationLegalActionGenerator::GenerateFromCandidates(CandidateResult.Candidates);
	if (LegalActionResult.bOk)
	{
		Result.ActivationActionSet = LegalActionResult.ActionSet;
	}

	return Result;
}

bool FWBActivationSessionTestHarness::FindFirstActivationActionIdWithLabel(
	const FWBCardActivationLegalActionSet& ActionSet,
	const FString& LabelContains,
	FString& OutActivationActionId)
{
	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		if (LabelContains.IsEmpty() || Action.PublicLabel.Contains(LabelContains, ESearchCase::IgnoreCase))
		{
			OutActivationActionId = Action.ActivationActionId;
			return true;
		}
	}

	OutActivationActionId.Reset();
	return false;
}

FWBActivationSessionHarnessStepResult
FWBActivationSessionTestHarness::RefreshAndExecuteFirstActivationWithLabel(
	FWBGameStateData& State,
	const int32 PlayerId,
	const TArray<FWBCardActivationCandidateSource>& ActivationSources,
	const FString& LabelContains,
	UWBRuntimeActivationDecisionSessionFacadeComponent* SessionFacade)
{
	FWBActivationSessionHarnessStepResult StepResult;
	if (SessionFacade == nullptr)
	{
		return StepResult;
	}

	StepResult.PreActionExternalData = BuildExternalDataForTest(State, PlayerId, ActivationSources);
	StepResult.InitialRefreshResult = SessionFacade->RefreshSessionFromExternalData(
		ToSessionRefreshInput(StepResult.PreActionExternalData));
	StepResult.bRefreshOk = StepResult.InitialRefreshResult.bOk;
	if (!StepResult.bRefreshOk)
	{
		return StepResult;
	}

	if (!FindFirstActivationActionIdWithLabel(
		StepResult.PreActionExternalData.ActivationActionSet,
		LabelContains,
		StepResult.SelectedActivationActionId))
	{
		return StepResult;
	}

	// The facade requires caller-supplied post-action data at call time. In tests, previewing
	// the resolved handoff on a cloned state simulates the external rules owner rebuilding the
	// next decision snapshot while the real state still mutates through the runtime facade path.
	FWBGameStateData PredictedPostState = State;
	const FWBRuntimeActivationExecutionHandoffResult PredictedHandoff =
		SessionFacade->CreateActivationExecutionHandoff(StepResult.SelectedActivationActionId);
	const FWBRuntimeActivationExecutionResult PredictedExecution =
		WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
			PredictedPostState,
			PredictedHandoff);
	StepResult.PostActionExternalData = BuildExternalDataForTest(
		PredictedExecution.bOk ? PredictedPostState : State,
		PlayerId,
		ActivationSources);

	StepResult.ExecutionResult = SessionFacade->ExecuteActivationActionIdAndRefreshFromExternalData(
		State,
		StepResult.SelectedActivationActionId,
		ToSessionRefreshInput(StepResult.PostActionExternalData));
	StepResult.bExecutionOk = StepResult.ExecutionResult.bOk;
	return StepResult;
}
