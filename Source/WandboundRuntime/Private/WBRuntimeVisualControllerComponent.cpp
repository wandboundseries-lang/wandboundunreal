#include "WBRuntimeVisualControllerComponent.h"

#include "WBBoardViewStateApplierComponent.h"
#include "WBRuntimeTurnResolutionAdapter.h"

UWBRuntimeVisualControllerComponent::UWBRuntimeVisualControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBRuntimeVisualControllerComponent::SetBoardViewStateApplier(UWBBoardViewStateApplierComponent* InApplier)
{
	BoardViewStateApplier = InApplier;
}

UWBBoardViewStateApplierComponent* UWBRuntimeVisualControllerComponent::GetBoardViewStateApplier() const
{
	return BoardViewStateApplier.Get();
}

FWBBoardViewRefreshResult UWBRuntimeVisualControllerComponent::ApplyPublicBoardSummary(const FWBPublicBoardSummary& Summary)
{
	bHasReceivedPublicBoardSummary = true;

	if (BoardViewStateApplier == nullptr)
	{
		LastBoardRefreshResult = FWBBoardViewRefreshResult();
		return LastBoardRefreshResult;
	}

	const bool bApplierWillApplyOnSet = BoardViewStateApplier->bApplyOnSummarySet;
	BoardViewStateApplier->SetLatestPublicBoardSummary(Summary);

	if (bAutoApplyPublicBoardSummaries && !bApplierWillApplyOnSet)
	{
		LastBoardRefreshResult = BoardViewStateApplier->ApplyLatestPublicBoardSummary();
	}
	else
	{
		LastBoardRefreshResult = BoardViewStateApplier->GetLastRefreshResult();
	}

	return LastBoardRefreshResult;
}

FWBBoardViewRefreshResult UWBRuntimeVisualControllerComponent::ApplyRuntimeSelectedActionResult(
	const FWBRuntimeSelectedActionResult& Result)
{
	return ApplyPublicBoardSummary(Result.FinalPublicBoardSummary);
}

bool UWBRuntimeVisualControllerComponent::HasReceivedPublicBoardSummary() const
{
	return bHasReceivedPublicBoardSummary;
}

FWBBoardViewRefreshResult UWBRuntimeVisualControllerComponent::GetLastBoardRefreshResult() const
{
	return LastBoardRefreshResult;
}
