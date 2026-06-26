#include "WBRuntimeActivationExecutionHandoff.h"

FWBRuntimeActivationExecutionHandoffResult
WBRuntimeActivationExecutionHandoff::CreateNotImplementedHandoff(
	const FWBRuntimeActivationSelectionResolution& SelectionResolution)
{
	FWBRuntimeActivationExecutionHandoffResult Result;
	Result.SelectedActivationActionId = SelectionResolution.SelectedActivationActionId;
	Result.bExecutionImplemented = false;
	Result.bExecutionAttempted = false;

	if (!SelectionResolution.bOk)
	{
		Result.bHandoffOk = false;
		Result.bSelectionResolved = false;
		Result.Reason = SelectionResolution.Reason.IsEmpty()
			? FString(TEXT("activation_selection_not_resolved"))
			: SelectionResolution.Reason;
		return Result;
	}

	Result.bHandoffOk = true;
	Result.bSelectionResolved = true;
	Result.Reason = TEXT("activation_execution_not_implemented");
	Result.ActivationAction = SelectionResolution.ActivationAction;
	Result.bHasActivationPresentationEntry =
		SelectionResolution.bHasActivationPresentationEntry;
	Result.ActivationPresentationEntry =
		SelectionResolution.ActivationPresentationEntry;
	Result.bHasTargetPresentationEntry =
		SelectionResolution.bHasTargetPresentationEntry;
	Result.TargetPresentationEntry =
		SelectionResolution.TargetPresentationEntry;
	return Result;
}
