#include "WBRuntimeDecisionPointCoordinatorComponent.h"

#include "WBRuntimeActivationPresentationModelComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeVisualControllerComponent.h"

namespace
{
FWBRuntimeActionSelectionExecutionResult MakeCoordinatorSelectionFailure(
	const FString& SelectedActionId,
	const FString& Reason)
{
	FWBRuntimeActionSelectionExecutionResult Result;
	Result.Selection.bOk = false;
	Result.Selection.SelectedActionId = SelectedActionId;
	Result.Selection.Reason = Reason;
	return Result;
}

FWBRuntimeActivationExecutionHandoffResult MakeCoordinatorActivationHandoffFailure(
	const FString& SelectedActivationActionId,
	const FString& Reason)
{
	FWBRuntimeActivationSelectionResolution Resolution;
	Resolution.bOk = false;
	Resolution.SelectedActivationActionId = SelectedActivationActionId;
	Resolution.Reason = Reason;
	return WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(Resolution);
}

FWBRuntimeDecisionPointStatus MakeDecisionPointStatus(
	const FWBRuntimeInteractionRefreshResult& RefreshResult,
	const FWBPublicBoardSummary& PublicBoardSummary,
	const UWBRuntimeTurnInteractionModelComponent* TurnInteractionModel)
{
	FWBRuntimeDecisionPointStatus Status;
	Status.bReady = true;
	Status.bHasActions = RefreshResult.LegalActionCount > 0;
	Status.LegalActionCount = RefreshResult.LegalActionCount;
	Status.PresentationEntryCount = RefreshResult.PresentationEntryCount;
	Status.PublicUnitCount = PublicBoardSummary.Units.Num();
	Status.PublicWallCount = PublicBoardSummary.Walls.Num();
	Status.PublicTerrainCount = PublicBoardSummary.TerrainTiles.Num();
	Status.LastRefreshReason = RefreshResult.Reason;

	if (TurnInteractionModel != nullptr && TurnInteractionModel->HasCurrentInteractionState())
	{
		Status.PresentationEntryCount = TurnInteractionModel->GetCurrentPresentationSnapshot().Entries.Num();
	}

	return Status;
}

FWBRuntimeDecisionPointStatus MakeFailedDecisionPointStatus(const FString& Reason)
{
	FWBRuntimeDecisionPointStatus Status;
	Status.LastRefreshReason = Reason;
	return Status;
}

FString ReasonForSelectedActionResult(const FWBRuntimeActionSelectionExecutionResult& Result)
{
	if (!Result.Selection.bOk)
	{
		return Result.Selection.Reason;
	}

	return Result.Execution.RuntimeResult.ApplyResult.Reason;
}
}

UWBRuntimeDecisionPointCoordinatorComponent::UWBRuntimeDecisionPointCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeDecisionPointCoordinatorComponent::SetTurnInteractionModel(
	UWBRuntimeTurnInteractionModelComponent* InModel)
{
	TurnInteractionModel = InModel;
}

UWBRuntimeTurnInteractionModelComponent* UWBRuntimeDecisionPointCoordinatorComponent::GetTurnInteractionModel() const
{
	return TurnInteractionModel.Get();
}

void UWBRuntimeDecisionPointCoordinatorComponent::SetVisualController(
	UWBRuntimeVisualControllerComponent* InVisualController)
{
	VisualController = InVisualController;
}

UWBRuntimeVisualControllerComponent* UWBRuntimeDecisionPointCoordinatorComponent::GetVisualController() const
{
	return VisualController.Get();
}

void UWBRuntimeDecisionPointCoordinatorComponent::SetActivationPresentationModel(
	UWBRuntimeActivationPresentationModelComponent* InModel)
{
	ActivationPresentationModel = InModel;
}

UWBRuntimeActivationPresentationModelComponent*
UWBRuntimeDecisionPointCoordinatorComponent::GetActivationPresentationModel() const
{
	return ActivationPresentationModel.Get();
}

FWBRuntimeInteractionRefreshResult UWBRuntimeDecisionPointCoordinatorComponent::RefreshDecisionPoint(
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	FWBRuntimeInteractionRefreshOptions Options;
	Options.bRefreshTurnInteractionModel = bRefreshTurnInteractionModel;
	Options.bRefreshVisualController = bRefreshVisualController;

	LastRefreshResult = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		PrecomputedLegalActions,
		PublicBoardSummary,
		TurnInteractionModel.Get(),
		VisualController.Get(),
		Options);

	if (!LastRefreshResult.bOk)
	{
		bHasCurrentDecisionPoint = false;
		CurrentStatus = MakeFailedDecisionPointStatus(LastRefreshResult.Reason);
		ClearLastSelectedActionExecution();
		return LastRefreshResult;
	}

	bHasCurrentDecisionPoint = true;
	CurrentStatus = MakeDecisionPointStatus(
		LastRefreshResult,
		PublicBoardSummary,
		TurnInteractionModel.Get());
	ClearLastSelectedActionExecution();

	return LastRefreshResult;
}

FWBRuntimeActivationPresentationRefreshResult
UWBRuntimeDecisionPointCoordinatorComponent::RefreshActivationPresentationFromExternalData(
	const FWBCardActivationLegalActionSet& ActivationActionSet,
	const FWBPublicBoardSummary& PublicBoardSummary)
{
	const FWBRuntimeActivationPresentationRefreshResult Result =
		WBRuntimeActivationPresentationRefreshAdapter::RefreshActivationPresentation(
			ActivationActionSet,
			PublicBoardSummary,
			ActivationPresentationModel.Get());

	if (Result.bOk)
	{
		CurrentStatus.ActivationActionCount = Result.ActivationActionCount;
		CurrentStatus.ActivationPresentationEntryCount = Result.ActivationPresentationEntryCount;
		CurrentStatus.ActivationTargetPresentationEntryCount = Result.TargetPresentationEntryCount;
	}

	return Result;
}

FWBRuntimeActivationSelectionResolution
UWBRuntimeDecisionPointCoordinatorComponent::ResolveSelectedActivationActionId(
	const FString& SelectedActivationActionId) const
{
	return WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
		ActivationPresentationModel.Get(),
		SelectedActivationActionId);
}

FWBRuntimeActivationExecutionHandoffResult
UWBRuntimeDecisionPointCoordinatorComponent::CreateActivationExecutionHandoff(
	const FString& SelectedActivationActionId) const
{
	if (ActivationPresentationModel == nullptr)
	{
		return MakeCoordinatorActivationHandoffFailure(
			SelectedActivationActionId,
			TEXT("activation_presentation_model_missing"));
	}

	return ActivationPresentationModel->CreateExecutionHandoffForActivationActionId(
		SelectedActivationActionId);
}

void UWBRuntimeDecisionPointCoordinatorComponent::ClearDecisionPoint()
{
	bHasCurrentDecisionPoint = false;
	CurrentStatus = FWBRuntimeDecisionPointStatus();
	LastRefreshResult = FWBRuntimeInteractionRefreshResult();
	ClearLastSelectedActionExecution();

	if (TurnInteractionModel != nullptr)
	{
		TurnInteractionModel->ClearCurrentInteractionState();
	}
}

void UWBRuntimeDecisionPointCoordinatorComponent::ClearActivationPresentation()
{
	CurrentStatus.ActivationActionCount = 0;
	CurrentStatus.ActivationPresentationEntryCount = 0;
	CurrentStatus.ActivationTargetPresentationEntryCount = 0;

	if (ActivationPresentationModel != nullptr)
	{
		ActivationPresentationModel->ClearCurrentActivationPresentationState();
	}
}

bool UWBRuntimeDecisionPointCoordinatorComponent::HasCurrentDecisionPoint() const
{
	return bHasCurrentDecisionPoint;
}

FWBRuntimeDecisionPointStatus UWBRuntimeDecisionPointCoordinatorComponent::GetCurrentStatus() const
{
	return CurrentStatus;
}

FWBRuntimeInteractionRefreshResult UWBRuntimeDecisionPointCoordinatorComponent::GetLastRefreshResult() const
{
	return LastRefreshResult;
}

const FWBRuntimeLegalActionPresentationSnapshot*
UWBRuntimeDecisionPointCoordinatorComponent::GetCurrentPresentationSnapshot() const
{
	if (!bHasCurrentDecisionPoint || TurnInteractionModel == nullptr)
	{
		return nullptr;
	}

	return &TurnInteractionModel->GetCurrentPresentationSnapshot();
}

bool UWBRuntimeDecisionPointCoordinatorComponent::TryFindPresentationEntryByActionId(
	const FString& ActionId,
	FWBRuntimeActionPresentationEntry& OutEntry) const
{
	if (!bHasCurrentDecisionPoint || TurnInteractionModel == nullptr)
	{
		return false;
	}

	return TurnInteractionModel->TryFindPresentationEntryByActionId(ActionId, OutEntry);
}

FWBRuntimeActionSelectionExecutionResult UWBRuntimeDecisionPointCoordinatorComponent::ExecuteSelectedActionId(
	FWBGameStateData& State,
	const FString& SelectedActionId,
	FWBRuntimeTurnResolutionContext& RuntimeContext,
	UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade)
{
	if (!bHasCurrentDecisionPoint)
	{
		LastSelectedActionExecutionResult = MakeCoordinatorSelectionFailure(
			SelectedActionId,
			TEXT("no_current_decision_point"));
	}
	else if (TurnInteractionModel == nullptr)
	{
		LastSelectedActionExecutionResult = MakeCoordinatorSelectionFailure(
			SelectedActionId,
			TEXT("turn_interaction_model_missing"));
	}
	else
	{
		LastSelectedActionExecutionResult = TurnInteractionModel->ExecuteSelectedActionId(
			State,
			SelectedActionId,
			RuntimeContext,
			RuntimeControllerFacade);
	}

	bHasLastSelectedActionExecution = true;
	CurrentStatus.bHasLastSelectedAction = true;
	CurrentStatus.LastSelectedActionId = SelectedActionId;
	CurrentStatus.bLastSelectedActionResolved = LastSelectedActionExecutionResult.Selection.bOk;
	CurrentStatus.LastSelectedActionReason = ReasonForSelectedActionResult(LastSelectedActionExecutionResult);

	return LastSelectedActionExecutionResult;
}

bool UWBRuntimeDecisionPointCoordinatorComponent::HasLastSelectedActionExecution() const
{
	return bHasLastSelectedActionExecution;
}

FWBRuntimeActionSelectionExecutionResult
UWBRuntimeDecisionPointCoordinatorComponent::GetLastSelectedActionExecutionResult() const
{
	return LastSelectedActionExecutionResult;
}

void UWBRuntimeDecisionPointCoordinatorComponent::ClearLastSelectedActionExecution()
{
	LastSelectedActionExecutionResult = FWBRuntimeActionSelectionExecutionResult();
	bHasLastSelectedActionExecution = false;
	CurrentStatus.bHasLastSelectedAction = false;
	CurrentStatus.LastSelectedActionId.Reset();
	CurrentStatus.bLastSelectedActionResolved = false;
	CurrentStatus.LastSelectedActionReason.Reset();
}
