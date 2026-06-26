#include "WBRuntimeActivationDecisionSessionFacadeComponent.h"

#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"

namespace
{
FWBRuntimeActivationSelectionResolution MakeSessionSelectionFailure(
	const FString& SelectedActivationActionId,
	const FString& Reason)
{
	FWBRuntimeActivationSelectionResolution Result;
	Result.bOk = false;
	Result.SelectedActivationActionId = SelectedActivationActionId;
	Result.Reason = Reason;
	return Result;
}

FWBRuntimeActivationExecutionHandoffResult MakeSessionHandoffFailure(
	const FString& SelectedActivationActionId,
	const FString& Reason)
{
	return WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(
		MakeSessionSelectionFailure(SelectedActivationActionId, Reason));
}

FWBRuntimePostActivationRefreshInput ToPostActivationRefreshInput(
	const FWBRuntimeActivationDecisionSessionRefreshInput& Input)
{
	FWBRuntimePostActivationRefreshInput Result;
	Result.PostActionPrecomputedLegalActions = Input.NormalLegalActions;
	Result.PostActionActivationActionSet = Input.ActivationActionSet;
	Result.PostActionPublicBoardSummary = Input.PublicBoardSummary;
	return Result;
}

FWBRuntimeActivationDecisionSessionStatus BuildSessionStatusFromOwner(
	const UWBRuntimeDecisionPointOwnerComponent* Owner)
{
	FWBRuntimeActivationDecisionSessionStatus Status;
	if (Owner == nullptr)
	{
		return Status;
	}

	const FWBRuntimeDecisionPointStatus OwnerStatus = Owner->GetCurrentStatus();
	Status.bNormalDecisionPointReady = Owner->HasCurrentDecisionPoint();
	Status.NormalLegalActionCount = OwnerStatus.LegalActionCount;
	Status.NormalPresentationEntryCount = OwnerStatus.PresentationEntryCount;
	Status.ActivationActionCount = OwnerStatus.ActivationActionCount;
	Status.ActivationPresentationEntryCount = OwnerStatus.ActivationPresentationEntryCount;
	Status.ActivationTargetPresentationEntryCount = OwnerStatus.ActivationTargetPresentationEntryCount;

	if (const UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = Owner->GetDecisionPointCoordinator())
	{
		if (const UWBRuntimeActivationPresentationModelComponent* ActivationModel =
			Coordinator->GetActivationPresentationModel())
		{
			const FWBRuntimeActivationPresentationStatus ActivationStatus =
				ActivationModel->GetCurrentActivationPresentationStatus();
			Status.bActivationPresentationReady = ActivationStatus.bReady;
			Status.ActivationActionCount = ActivationStatus.ActivationActionCount;
			Status.ActivationPresentationEntryCount = ActivationStatus.ActivationPresentationEntryCount;
			Status.ActivationTargetPresentationEntryCount = ActivationStatus.TargetPresentationEntryCount;
		}
	}

	Status.bReady = Status.bNormalDecisionPointReady && Status.bActivationPresentationReady;
	return Status;
}

FString MakeRefreshFailureReason(
	const FWBRuntimeInteractionRefreshResult& NormalResult,
	const FWBRuntimeActivationPresentationRefreshResult& ActivationResult)
{
	if (!NormalResult.bOk)
	{
		return TEXT("normal_decision_refresh_failed");
	}

	if (!ActivationResult.bOk)
	{
		return TEXT("activation_presentation_refresh_failed");
	}

	return FString();
}

FString ReasonForSessionExecutionResult(
	const FWBRuntimeActivationSelectionResolution& Selection,
	const FWBRuntimePostActivationRefreshResult& PostActivationResult)
{
	if (!Selection.bOk)
	{
		return Selection.Reason;
	}

	return PostActivationResult.Reason;
}
}

UWBRuntimeActivationDecisionSessionFacadeComponent::UWBRuntimeActivationDecisionSessionFacadeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeActivationDecisionSessionFacadeComponent::SetDecisionPointOwner(
	UWBRuntimeDecisionPointOwnerComponent* InOwner)
{
	DecisionPointOwner = InOwner;
}

UWBRuntimeDecisionPointOwnerComponent*
UWBRuntimeActivationDecisionSessionFacadeComponent::GetDecisionPointOwner() const
{
	return DecisionPointOwner.Get();
}

FWBRuntimeActivationDecisionSessionRefreshResult
UWBRuntimeActivationDecisionSessionFacadeComponent::RefreshSessionFromExternalData(
	const FWBRuntimeActivationDecisionSessionRefreshInput& Input)
{
	FWBRuntimeActivationDecisionSessionRefreshResult Result;
	if (DecisionPointOwner == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("decision_point_owner_missing");
		CurrentStatus = FWBRuntimeActivationDecisionSessionStatus();
		CurrentStatus.LastReason = Result.Reason;
		Result.Status = CurrentStatus;
		bHasSessionState = false;
		return Result;
	}

	Result.NormalRefreshResult = DecisionPointOwner->RefreshDecisionPointFromExternalData(
		Input.NormalLegalActions,
		Input.PublicBoardSummary);
	Result.ActivationRefreshResult = DecisionPointOwner->RefreshActivationPresentationFromExternalData(
		Input.ActivationActionSet,
		Input.PublicBoardSummary);

	CurrentStatus = BuildSessionStatusFromOwner(DecisionPointOwner.Get());
	Result.Reason = MakeRefreshFailureReason(Result.NormalRefreshResult, Result.ActivationRefreshResult);
	Result.bOk = Result.Reason.IsEmpty();
	CurrentStatus.bReady = Result.bOk;
	CurrentStatus.LastReason = Result.Reason;
	Result.Status = CurrentStatus;
	bHasSessionState = Result.bOk;
	return Result;
}

FWBRuntimeActivationSelectionResolution
UWBRuntimeActivationDecisionSessionFacadeComponent::ResolveActivationActionId(
	const FString& SelectedActivationActionId) const
{
	if (DecisionPointOwner == nullptr)
	{
		return MakeSessionSelectionFailure(
			SelectedActivationActionId,
			TEXT("decision_point_owner_missing"));
	}

	return DecisionPointOwner->ResolveSelectedActivationActionId(SelectedActivationActionId);
}

FWBRuntimeActivationExecutionHandoffResult
UWBRuntimeActivationDecisionSessionFacadeComponent::CreateActivationExecutionHandoff(
	const FString& SelectedActivationActionId) const
{
	if (DecisionPointOwner == nullptr)
	{
		return MakeSessionHandoffFailure(
			SelectedActivationActionId,
			TEXT("decision_point_owner_missing"));
	}

	return DecisionPointOwner->CreateActivationExecutionHandoff(SelectedActivationActionId);
}

FWBRuntimeActivationDecisionSessionExecutionResult
UWBRuntimeActivationDecisionSessionFacadeComponent::ExecuteActivationActionIdAndRefreshFromExternalData(
	FWBGameStateData& State,
	const FString& SelectedActivationActionId,
	const FWBRuntimeActivationDecisionSessionRefreshInput& PostActionInput,
	const FWBRuntimePostActivationRefreshOptions& Options)
{
	FWBRuntimeActivationDecisionSessionExecutionResult Result;
	if (DecisionPointOwner == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("decision_point_owner_missing");
		Result.SelectionResult = MakeSessionSelectionFailure(SelectedActivationActionId, Result.Reason);
		Result.HandoffResult = WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(
			Result.SelectionResult);
		CurrentStatus = FWBRuntimeActivationDecisionSessionStatus();
		CurrentStatus.LastSelectedActivationActionId = SelectedActivationActionId;
		CurrentStatus.LastReason = Result.Reason;
		Result.Status = CurrentStatus;
		return Result;
	}

	LastActivationSelection = DecisionPointOwner->ResolveSelectedActivationActionId(
		SelectedActivationActionId);
	LastActivationHandoff = DecisionPointOwner->CreateActivationExecutionHandoff(
		SelectedActivationActionId);
	LastPostActivationRefreshResult =
		DecisionPointOwner->ExecuteActivationActionIdAndRefreshFromExternalData(
			State,
			SelectedActivationActionId,
			ToPostActivationRefreshInput(PostActionInput),
			Options);

	CurrentStatus = BuildSessionStatusFromOwner(DecisionPointOwner.Get());
	CurrentStatus.bHasLastActivationSelection = true;
	CurrentStatus.bHasLastActivationExecution = true;
	CurrentStatus.LastSelectedActivationActionId = SelectedActivationActionId;
	CurrentStatus.LastReason = ReasonForSessionExecutionResult(
		LastActivationSelection,
		LastPostActivationRefreshResult);
	CurrentStatus.bReady = CurrentStatus.bNormalDecisionPointReady
		&& CurrentStatus.bActivationPresentationReady
		&& LastPostActivationRefreshResult.bOk;

	Result.SelectionResult = LastActivationSelection;
	Result.HandoffResult = LastActivationHandoff;
	Result.PostActivationRefreshResult = LastPostActivationRefreshResult;
	Result.bOk = LastPostActivationRefreshResult.bOk;
	Result.Reason = CurrentStatus.LastReason;
	Result.Status = CurrentStatus;
	bHasSessionState = CurrentStatus.bReady;
	return Result;
}

bool UWBRuntimeActivationDecisionSessionFacadeComponent::HasSessionState() const
{
	return bHasSessionState;
}

FWBRuntimeActivationDecisionSessionStatus
UWBRuntimeActivationDecisionSessionFacadeComponent::GetCurrentStatus() const
{
	return CurrentStatus;
}

FWBRuntimeActivationSelectionResolution
UWBRuntimeActivationDecisionSessionFacadeComponent::GetLastActivationSelection() const
{
	return LastActivationSelection;
}

FWBRuntimeActivationExecutionHandoffResult
UWBRuntimeActivationDecisionSessionFacadeComponent::GetLastActivationHandoff() const
{
	return LastActivationHandoff;
}

FWBRuntimePostActivationRefreshResult
UWBRuntimeActivationDecisionSessionFacadeComponent::GetLastPostActivationRefreshResult() const
{
	return LastPostActivationRefreshResult;
}

void UWBRuntimeActivationDecisionSessionFacadeComponent::ClearSession()
{
	bHasSessionState = false;
	CurrentStatus = FWBRuntimeActivationDecisionSessionStatus();
	LastActivationSelection = FWBRuntimeActivationSelectionResolution();
	LastActivationHandoff = FWBRuntimeActivationExecutionHandoffResult();
	LastPostActivationRefreshResult = FWBRuntimePostActivationRefreshResult();
}
