#include "WBBoardViewStateApplierComponent.h"

UWBBoardViewStateApplierComponent::UWBBoardViewStateApplierComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWBBoardViewStateApplierComponent::SetBoardViewActor(AWBBoardViewActor* InBoardViewActor)
{
	BoardViewActor = InBoardViewActor;
}

AWBBoardViewActor* UWBBoardViewStateApplierComponent::GetBoardViewActor() const
{
	return BoardViewActor.Get();
}

void UWBBoardViewStateApplierComponent::SetLatestPublicBoardSummary(const FWBPublicBoardSummary& Summary)
{
	LatestPublicBoardSummary = Summary;
	bHasLatestPublicBoardSummary = true;

	if (bApplyOnSummarySet)
	{
		ApplyLatestPublicBoardSummary();
	}
}

void UWBBoardViewStateApplierComponent::SetLatestPublicBoardSummaryFromState(const FWBGameStateData& State)
{
	SetLatestPublicBoardSummary(WBBoardSummaryBridge::BuildPublicSummaryFromState(State));
}

bool UWBBoardViewStateApplierComponent::HasLatestPublicBoardSummary() const
{
	return bHasLatestPublicBoardSummary;
}

const FWBPublicBoardSummary& UWBBoardViewStateApplierComponent::GetLatestPublicBoardSummary() const
{
	return LatestPublicBoardSummary;
}

FWBBoardViewRefreshResult UWBBoardViewStateApplierComponent::ApplyLatestPublicBoardSummary()
{
	if (!bHasLatestPublicBoardSummary || BoardViewActor == nullptr)
	{
		LastRefreshResult = FWBBoardViewRefreshResult();
		return LastRefreshResult;
	}

	LastRefreshResult = WBBoardSummaryBridge::RenderSummaryToBoardView(LatestPublicBoardSummary, BoardViewActor);
	return LastRefreshResult;
}

FWBBoardViewRefreshResult UWBBoardViewStateApplierComponent::GetLastRefreshResult() const
{
	return LastRefreshResult;
}
