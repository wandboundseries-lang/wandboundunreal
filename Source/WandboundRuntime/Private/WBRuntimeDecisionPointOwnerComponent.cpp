#include "WBRuntimeDecisionPointOwnerComponent.h"

#include "WBRuntimeControllerFacadeComponent.h"

namespace
{
FWBRuntimeInteractionRefreshResult MakeOwnerRefreshFailure(const FString& Reason)
{
	FWBRuntimeInteractionRefreshResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

FWBRuntimeActivationPresentationRefreshResult MakeOwnerActivationRefreshFailure(const FString& Reason)
{
	FWBRuntimeActivationPresentationRefreshResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

FWBRuntimeDecisionPointStatus MakeOwnerFailedStatus(const FString& Reason)
{
	FWBRuntimeDecisionPointStatus Status;
	Status.LastRefreshReason = Reason;
	return Status;
}

FWBRuntimeActionSelectionExecutionResult MakeOwnerSelectionFailure(
	const FString& SelectedActionId,
	const FString& Reason)
{
	FWBRuntimeActionSelectionExecutionResult Result;
	Result.Selection.bOk = false;
	Result.Selection.SelectedActionId = SelectedActionId;
	Result.Selection.Reason = Reason;
	return Result;
}

bool DidExecutionSucceed(const FWBRuntimeActionSelectionExecutionResult& Result)
{
	return Result.Selection.bOk
		&& Result.Execution.RuntimeResult.ApplyResult.bOk;
}

void AddSelectedActionStatus(
	FWBRuntimeDecisionPointStatus& Status,
	const FString& SelectedActionId,
	const FWBRuntimeActionSelectionExecutionResult& Result)
{
	Status.bHasLastSelectedAction = true;
	Status.LastSelectedActionId = SelectedActionId;
	Status.bLastSelectedActionResolved = Result.Selection.bOk;
	Status.LastSelectedActionReason = Result.Selection.bOk
		? Result.Execution.RuntimeResult.ApplyResult.Reason
		: Result.Selection.Reason;
}
}

UWBRuntimeDecisionPointOwnerComponent::UWBRuntimeDecisionPointOwnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeDecisionPointOwnerComponent::SetDecisionPointCoordinator(
	UWBRuntimeDecisionPointCoordinatorComponent* InCoordinator)
{
	DecisionPointCoordinator = InCoordinator;
}

UWBRuntimeDecisionPointCoordinatorComponent*
UWBRuntimeDecisionPointOwnerComponent::GetDecisionPointCoordinator() const
{
	return DecisionPointCoordinator.Get();
}

void UWBRuntimeDecisionPointOwnerComponent::SetRuntimeControllerFacade(
	UWBRuntimeControllerFacadeComponent* InFacade)
{
	RuntimeControllerFacade = InFacade;
}

UWBRuntimeControllerFacadeComponent*
UWBRuntimeDecisionPointOwnerComponent::GetRuntimeControllerFacade() const
{
	return RuntimeControllerFacade.Get();
}

FWBRuntimeInteractionRefreshResult
UWBRuntimeDecisionPointOwnerComponent::RefreshDecisionPointFromExternalData(
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	if (DecisionPointCoordinator == nullptr)
	{
		LastRefreshResult = MakeOwnerRefreshFailure(TEXT("decision_point_coordinator_missing"));
		bHasCurrentDecisionPoint = false;
		CurrentStatus = MakeOwnerFailedStatus(LastRefreshResult.Reason);
		return LastRefreshResult;
	}

	LastRefreshResult = DecisionPointCoordinator->RefreshDecisionPoint(
		PrecomputedLegalActions,
		PublicBoardSummary);
	bHasCurrentDecisionPoint = DecisionPointCoordinator->HasCurrentDecisionPoint();
	CurrentStatus = DecisionPointCoordinator->GetCurrentStatus();
	return LastRefreshResult;
}

FWBRuntimeActivationPresentationRefreshResult
UWBRuntimeDecisionPointOwnerComponent::RefreshActivationPresentationFromExternalData(
	const FWBCardActivationLegalActionSet& ActivationActionSet,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	if (DecisionPointCoordinator == nullptr)
	{
		return MakeOwnerActivationRefreshFailure(TEXT("decision_point_coordinator_missing"));
	}

	const FWBRuntimeActivationPresentationRefreshResult Result =
		DecisionPointCoordinator->RefreshActivationPresentationFromExternalData(
		ActivationActionSet,
		PublicBoardSummary);
	CurrentStatus = DecisionPointCoordinator->GetCurrentStatus();
	bHasCurrentDecisionPoint = DecisionPointCoordinator->HasCurrentDecisionPoint();
	return Result;
}

FWBRuntimeActionSelectionExecutionResult
UWBRuntimeDecisionPointOwnerComponent::ExecuteSelectedActionId(
	FWBGameStateData& State,
	const FString& SelectedActionId,
	FWBRuntimeTurnResolutionContext& RuntimeContext)
{
	if (DecisionPointCoordinator == nullptr)
	{
		LastExecutionResult = MakeOwnerSelectionFailure(
			SelectedActionId,
			TEXT("decision_point_coordinator_missing"));
		bHasLastExecutionResult = true;
		AddSelectedActionStatus(CurrentStatus, SelectedActionId, LastExecutionResult);
		return LastExecutionResult;
	}

	if (RuntimeControllerFacade == nullptr)
	{
		LastExecutionResult = MakeOwnerSelectionFailure(
			SelectedActionId,
			TEXT("runtime_controller_facade_missing"));
		bHasLastExecutionResult = true;
		bHasCurrentDecisionPoint = DecisionPointCoordinator->HasCurrentDecisionPoint();
		CurrentStatus = DecisionPointCoordinator->GetCurrentStatus();
		AddSelectedActionStatus(CurrentStatus, SelectedActionId, LastExecutionResult);
		return LastExecutionResult;
	}

	if (!bHasCurrentDecisionPoint || !DecisionPointCoordinator->HasCurrentDecisionPoint())
	{
		LastExecutionResult = MakeOwnerSelectionFailure(
			SelectedActionId,
			TEXT("no_current_decision_point"));
		bHasLastExecutionResult = true;
		bHasCurrentDecisionPoint = DecisionPointCoordinator->HasCurrentDecisionPoint();
		CurrentStatus = DecisionPointCoordinator->GetCurrentStatus();
		AddSelectedActionStatus(CurrentStatus, SelectedActionId, LastExecutionResult);
		return LastExecutionResult;
	}

	LastExecutionResult = DecisionPointCoordinator->ExecuteSelectedActionId(
		State,
		SelectedActionId,
		RuntimeContext,
		RuntimeControllerFacade.Get());
	bHasLastExecutionResult = true;
	bHasCurrentDecisionPoint = DecisionPointCoordinator->HasCurrentDecisionPoint();
	CurrentStatus = DecisionPointCoordinator->GetCurrentStatus();
	return LastExecutionResult;
}

FWBRuntimeActionSelectionExecutionResult
UWBRuntimeDecisionPointOwnerComponent::ExecuteSelectedActionIdAndRefreshFromExternalData(
	FWBGameStateData& State,
	const FString& SelectedActionId,
	FWBRuntimeTurnResolutionContext& RuntimeContext,
	const TArray<FWBAction>& PostActionPrecomputedLegalActions,
	const FWBPublicBoardSummary& PostActionPublicBoardSummary)
{
	const FWBRuntimeActionSelectionExecutionResult Result = ExecuteSelectedActionId(
		State,
		SelectedActionId,
		RuntimeContext);

	if (DidExecutionSucceed(Result))
	{
		RefreshDecisionPointFromExternalData(
			PostActionPrecomputedLegalActions,
			PostActionPublicBoardSummary);
	}

	return Result;
}

bool UWBRuntimeDecisionPointOwnerComponent::HasCurrentDecisionPoint() const
{
	return bHasCurrentDecisionPoint;
}

FWBRuntimeDecisionPointStatus UWBRuntimeDecisionPointOwnerComponent::GetCurrentStatus() const
{
	return CurrentStatus;
}

FWBRuntimeInteractionRefreshResult
UWBRuntimeDecisionPointOwnerComponent::GetLastRefreshResult() const
{
	return LastRefreshResult;
}

bool UWBRuntimeDecisionPointOwnerComponent::HasLastExecutionResult() const
{
	return bHasLastExecutionResult;
}

FWBRuntimeActionSelectionExecutionResult
UWBRuntimeDecisionPointOwnerComponent::GetLastExecutionResult() const
{
	return LastExecutionResult;
}

void UWBRuntimeDecisionPointOwnerComponent::ClearActivationPresentation()
{
	if (DecisionPointCoordinator != nullptr)
	{
		DecisionPointCoordinator->ClearActivationPresentation();
	}
}

void UWBRuntimeDecisionPointOwnerComponent::Clear()
{
	bHasCurrentDecisionPoint = false;
	CurrentStatus = FWBRuntimeDecisionPointStatus();
	LastRefreshResult = FWBRuntimeInteractionRefreshResult();
	LastExecutionResult = FWBRuntimeActionSelectionExecutionResult();
	bHasLastExecutionResult = false;

	if (DecisionPointCoordinator != nullptr)
	{
		DecisionPointCoordinator->ClearDecisionPoint();
		DecisionPointCoordinator->ClearActivationPresentation();
	}
}
