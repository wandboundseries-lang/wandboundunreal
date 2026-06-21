#include "WBRuntimeDecisionPointCoordinatorComponent.h"

#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeVisualControllerComponent.h"

namespace
{
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
		return LastRefreshResult;
	}

	bHasCurrentDecisionPoint = true;
	CurrentStatus = MakeDecisionPointStatus(
		LastRefreshResult,
		PublicBoardSummary,
		TurnInteractionModel.Get());

	return LastRefreshResult;
}

void UWBRuntimeDecisionPointCoordinatorComponent::ClearDecisionPoint()
{
	bHasCurrentDecisionPoint = false;
	CurrentStatus = FWBRuntimeDecisionPointStatus();
	LastRefreshResult = FWBRuntimeInteractionRefreshResult();

	if (TurnInteractionModel != nullptr)
	{
		TurnInteractionModel->ClearCurrentInteractionState();
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
