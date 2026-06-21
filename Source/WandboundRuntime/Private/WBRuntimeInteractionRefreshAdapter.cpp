#include "WBRuntimeInteractionRefreshAdapter.h"

#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeVisualControllerComponent.h"

FWBRuntimeInteractionRefreshResult WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FWBPublicBoardSummary& PublicBoardSummary,
	UWBRuntimeTurnInteractionModelComponent* TurnInteractionModel,
	UWBRuntimeVisualControllerComponent* VisualController,
	const FWBRuntimeInteractionRefreshOptions& Options)
{
	FWBRuntimeInteractionRefreshResult Result;
	Result.LegalActionCount = PrecomputedLegalActions.Num();

	if (!Options.bRefreshTurnInteractionModel && !Options.bRefreshVisualController)
	{
		Result.Reason = TEXT("no_refresh_targets_enabled");
		return Result;
	}

	if (Options.bRefreshTurnInteractionModel && TurnInteractionModel == nullptr)
	{
		Result.Reason = TEXT("turn_interaction_model_missing");
		return Result;
	}

	if (Options.bRefreshVisualController && VisualController == nullptr)
	{
		Result.Reason = TEXT("visual_controller_missing");
		return Result;
	}

	if (Options.bRefreshTurnInteractionModel)
	{
		TurnInteractionModel->SetCurrentInteractionState(PrecomputedLegalActions, PublicBoardSummary);
		Result.bInteractionModelRefreshed = true;
		Result.PresentationEntryCount = TurnInteractionModel->GetCurrentPresentationSnapshot().Entries.Num();
	}

	if (Options.bRefreshVisualController)
	{
		Result.VisualRefreshResult = VisualController->ApplyPublicBoardSummary(PublicBoardSummary);
		Result.bVisualControllerRefreshed = Result.VisualRefreshResult.bRendered;
	}

	Result.bOk = true;
	return Result;
}
