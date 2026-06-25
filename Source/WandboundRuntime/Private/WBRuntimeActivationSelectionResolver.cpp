#include "WBRuntimeActivationSelectionResolver.h"

#include "WBRuntimeActivationPresentationModelComponent.h"

namespace
{
FWBRuntimeActivationSelectionResolution MakeActivationSelectionFailure(
	const FString& SelectedActivationActionId,
	const FString& Reason)
{
	FWBRuntimeActivationSelectionResolution Resolution;
	Resolution.bOk = false;
	Resolution.SelectedActivationActionId = SelectedActivationActionId;
	Resolution.Reason = Reason;
	return Resolution;
}
}

FWBRuntimeActivationSelectionResolution
WBRuntimeActivationSelectionResolver::ResolveSelectedActivationActionId(
	const UWBRuntimeActivationPresentationModelComponent* ActivationPresentationModel,
	const FString& SelectedActivationActionId)
{
	if (ActivationPresentationModel == nullptr)
	{
		return MakeActivationSelectionFailure(
			SelectedActivationActionId,
			TEXT("activation_presentation_model_missing"));
	}

	if (!ActivationPresentationModel->HasCurrentActivationPresentationState())
	{
		return MakeActivationSelectionFailure(
			SelectedActivationActionId,
			TEXT("no_current_activation_presentation_state"));
	}

	const FWBCardActivationLegalActionSet& ActionSet =
		ActivationPresentationModel->GetCurrentActivationActionSet();

	bool bFound = false;
	FWBCardActivationLegalAction FoundAction;
	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		if (Action.ActivationActionId != SelectedActivationActionId)
		{
			continue;
		}

		if (bFound)
		{
			return MakeActivationSelectionFailure(
				SelectedActivationActionId,
				TEXT("activation_action_id_ambiguous"));
		}

		bFound = true;
		FoundAction = Action;
	}

	if (!bFound)
	{
		return MakeActivationSelectionFailure(
			SelectedActivationActionId,
			TEXT("activation_action_id_not_found"));
	}

	FWBRuntimeActivationSelectionResolution Resolution;
	Resolution.bOk = true;
	Resolution.SelectedActivationActionId = SelectedActivationActionId;
	Resolution.ActivationAction = FoundAction;
	Resolution.bHasActivationPresentationEntry =
		ActivationPresentationModel->TryFindActivationPresentationEntryByActionId(
			SelectedActivationActionId,
			Resolution.ActivationPresentationEntry);
	Resolution.bHasTargetPresentationEntry =
		ActivationPresentationModel->TryFindTargetPresentationEntryByActionId(
			SelectedActivationActionId,
			Resolution.TargetPresentationEntry);
	return Resolution;
}
