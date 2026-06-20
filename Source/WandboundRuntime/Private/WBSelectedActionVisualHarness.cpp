#include "WBSelectedActionVisualHarness.h"

#include "WBRuntimeVisualControllerComponent.h"

FWBSelectedActionVisualHarnessResult WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
	FWBGameStateData& State,
	const FWBAction& SelectedAction,
	FWBRuntimeTurnResolutionContext& RuntimeContext,
	UWBRuntimeVisualControllerComponent* VisualController,
	const FWBSelectedActionVisualHarnessOptions& Options)
{
	FWBSelectedActionVisualHarnessResult Result;
	Result.RuntimeResult = WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
		State,
		SelectedAction,
		RuntimeContext);

	const bool bShouldRefresh =
		(Result.RuntimeResult.ApplyResult.bOk && Options.bRefreshVisualsOnSuccess)
		|| (!Result.RuntimeResult.ApplyResult.bOk && Options.bRefreshVisualsOnFailure);

	if (bShouldRefresh && VisualController != nullptr)
	{
		Result.VisualRefreshResult = VisualController->ApplyRuntimeSelectedActionResult(Result.RuntimeResult);
	}

	return Result;
}
