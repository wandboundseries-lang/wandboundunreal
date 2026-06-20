#include "WBRuntimeControllerFacadeComponent.h"

UWBRuntimeControllerFacadeComponent::UWBRuntimeControllerFacadeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeControllerFacadeComponent::SetVisualController(
	UWBRuntimeVisualControllerComponent* InVisualController)
{
	VisualController = InVisualController;
}

UWBRuntimeVisualControllerComponent* UWBRuntimeControllerFacadeComponent::GetVisualController() const
{
	return VisualController.Get();
}

FWBSelectedActionVisualHarnessResult UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction(
	FWBGameStateData& State,
	const FWBAction& SelectedAction,
	FWBRuntimeTurnResolutionContext& RuntimeContext)
{
	FWBSelectedActionVisualHarnessOptions Options;
	Options.bRefreshVisualsOnSuccess = bRefreshVisualsOnActionSuccess;
	Options.bRefreshVisualsOnFailure = bRefreshVisualsOnActionFailure;

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			SelectedAction,
			RuntimeContext,
			VisualController.Get(),
			Options);

	LastRuntimeResult = Result.RuntimeResult;
	LastVisualRefreshResult = Result.VisualRefreshResult;
	bHasExecutedAction = true;

	return Result;
}

bool UWBRuntimeControllerFacadeComponent::HasExecutedAction() const
{
	return bHasExecutedAction;
}

const FWBRuntimeSelectedActionResult& UWBRuntimeControllerFacadeComponent::GetLastRuntimeResult() const
{
	return LastRuntimeResult;
}

FWBBoardViewRefreshResult UWBRuntimeControllerFacadeComponent::GetLastVisualRefreshResult() const
{
	return LastVisualRefreshResult;
}

void UWBRuntimeControllerFacadeComponent::ClearLastResults()
{
	bHasExecutedAction = false;
	LastRuntimeResult = FWBRuntimeSelectedActionResult();
	LastVisualRefreshResult = FWBBoardViewRefreshResult();
}
