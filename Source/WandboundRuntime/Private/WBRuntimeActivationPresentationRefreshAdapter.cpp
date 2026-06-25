#include "WBRuntimeActivationPresentationRefreshAdapter.h"

#include "WBRuntimeActivationPresentationModelComponent.h"

FWBRuntimeActivationPresentationRefreshResult
WBRuntimeActivationPresentationRefreshAdapter::RefreshActivationPresentation(
	const FWBCardActivationLegalActionSet& ActivationActionSet,
	const FWBPublicBoardSummary& PublicBoardSummary,
	UWBRuntimeActivationPresentationModelComponent* ActivationPresentationModel)
{
	FWBRuntimeActivationPresentationRefreshResult Result;
	Result.ActivationActionCount = ActivationActionSet.Actions.Num();

	if (ActivationPresentationModel == nullptr)
	{
		Result.Reason = TEXT("activation_presentation_model_missing");
		return Result;
	}

	ActivationPresentationModel->SetCurrentActivationPresentationState(
		ActivationActionSet,
		PublicBoardSummary);

	const FWBRuntimeActivationPresentationStatus Status =
		ActivationPresentationModel->GetCurrentActivationPresentationStatus();
	Result.bOk = true;
	Result.bActivationPresentationRefreshed = true;
	Result.ActivationActionCount = Status.ActivationActionCount;
	Result.ActivationPresentationEntryCount = Status.ActivationPresentationEntryCount;
	Result.TargetPresentationEntryCount = Status.TargetPresentationEntryCount;
	Result.Reason = Status.LastRefreshReason;
	return Result;
}
