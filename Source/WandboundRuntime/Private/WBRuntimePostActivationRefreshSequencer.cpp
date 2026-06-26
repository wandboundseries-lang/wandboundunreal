#include "WBRuntimePostActivationRefreshSequencer.h"

#include "WBRuntimeDecisionPointOwnerComponent.h"

namespace
{
FWBRuntimePostActivationRefreshResult MakePostActivationRefreshFailure(const FString& Reason)
{
	FWBRuntimePostActivationRefreshResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

bool ShouldRefreshNormalDecisionPoint(
	const bool bActivationExecutionOk,
	const FWBRuntimePostActivationRefreshOptions& Options)
{
	return bActivationExecutionOk
		? Options.bRefreshNormalDecisionPointOnSuccess
		: Options.bRefreshNormalDecisionPointOnFailure;
}

bool ShouldRefreshActivationPresentation(
	const bool bActivationExecutionOk,
	const FWBRuntimePostActivationRefreshOptions& Options)
{
	return bActivationExecutionOk
		? Options.bRefreshActivationPresentationOnSuccess
		: Options.bRefreshActivationPresentationOnFailure;
}

FString ReasonForFailedActivationExecution(const FWBRuntimeActivationExecutionResult& ActivationResult)
{
	return ActivationResult.Reason.IsEmpty()
		? FString(TEXT("activation_execution_failed"))
		: ActivationResult.Reason;
}
}

FWBRuntimePostActivationRefreshResult
WBRuntimePostActivationRefreshSequencer::ExecuteActivationAndRefreshFromExternalData(
	FWBGameStateData& State,
	const FString& SelectedActivationActionId,
	UWBRuntimeDecisionPointOwnerComponent* Owner,
	const FWBRuntimePostActivationRefreshInput& PostActionRefreshInput,
	const FWBRuntimePostActivationRefreshOptions& Options)
{
	if (Owner == nullptr)
	{
		return MakePostActivationRefreshFailure(TEXT("decision_point_owner_missing"));
	}

	FWBRuntimePostActivationRefreshResult Result;
	Result.ActivationExecutionResult = Owner->ExecuteActivationActionId(
		State,
		SelectedActivationActionId);
	Result.bActivationExecutionAttempted = Result.ActivationExecutionResult.bExecutionAttempted;
	Result.bActivationExecutionOk = Result.ActivationExecutionResult.bOk;

	const bool bShouldRefreshNormalDecisionPoint =
		ShouldRefreshNormalDecisionPoint(Result.bActivationExecutionOk, Options);
	const bool bShouldRefreshActivationPresentation =
		ShouldRefreshActivationPresentation(Result.bActivationExecutionOk, Options);

	if (bShouldRefreshNormalDecisionPoint)
	{
		Result.bPostActionRefreshAttempted = true;
		Result.NormalDecisionRefreshResult = Owner->RefreshDecisionPointFromExternalData(
			PostActionRefreshInput.PostActionPrecomputedLegalActions,
			PostActionRefreshInput.PostActionPublicBoardSummary);
		Result.bNormalDecisionPointRefreshed = Result.NormalDecisionRefreshResult.bOk;
	}

	if (bShouldRefreshActivationPresentation)
	{
		Result.bPostActionRefreshAttempted = true;
		Result.ActivationPresentationRefreshResult =
			Owner->RefreshActivationPresentationFromExternalData(
				PostActionRefreshInput.PostActionActivationActionSet,
				PostActionRefreshInput.PostActionPublicBoardSummary);
		Result.bActivationPresentationRefreshed = Result.ActivationPresentationRefreshResult.bOk;
	}

	if (!Result.bActivationExecutionOk)
	{
		Result.bOk = false;
		Result.Reason = ReasonForFailedActivationExecution(Result.ActivationExecutionResult);
		return Result;
	}

	if (bShouldRefreshNormalDecisionPoint && !Result.NormalDecisionRefreshResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = TEXT("normal_decision_refresh_failed");
		return Result;
	}

	if (bShouldRefreshActivationPresentation && !Result.ActivationPresentationRefreshResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = TEXT("activation_presentation_refresh_failed");
		return Result;
	}

	Result.bOk = true;
	Result.Reason = Result.ActivationExecutionResult.Reason;
	return Result;
}
