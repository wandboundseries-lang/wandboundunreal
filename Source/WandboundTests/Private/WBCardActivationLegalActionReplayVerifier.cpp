#include "WBCardActivationLegalActionReplayVerifier.h"

#include "WBEffectRunner.h"

FWBCardActivationLegalActionReplaySelection FWBCardActivationLegalActionReplayVerifier::ResolveActivationActionId(
	const FWBCardActivationLegalActionSet& ActionSet,
	const FString& SelectedActivationActionId)
{
	FWBCardActivationLegalActionReplaySelection Selection;
	Selection.SelectedActivationActionId = SelectedActivationActionId;

	const FWBCardActivationLegalAction* Match = nullptr;
	int32 MatchCount = 0;
	for (const FWBCardActivationLegalAction& Action : ActionSet.Actions)
	{
		if (Action.ActivationActionId == SelectedActivationActionId)
		{
			Match = &Action;
			++MatchCount;
		}
	}

	if (MatchCount == 0)
	{
		Selection.Reason = TEXT("activation_action_id_not_found");
		return Selection;
	}

	if (MatchCount > 1)
	{
		Selection.Reason = TEXT("activation_action_id_ambiguous");
		return Selection;
	}

	Selection.bOk = true;
	Selection.SelectedAction = *Match;
	Selection.Reason.Reset();
	return Selection;
}

FWBCardActivationLegalActionReplayResult FWBCardActivationLegalActionReplayVerifier::ApplySelectedActivationActionId(
	FWBGameStateData& State,
	const FWBCardActivationLegalActionSet& ActionSet,
	const FString& SelectedActivationActionId)
{
	FWBCardActivationLegalActionReplayResult Result;
	Result.Selection = ResolveActivationActionId(ActionSet, SelectedActivationActionId);
	if (!Result.Selection.bOk)
	{
		Result.ActivationResult.bOk = false;
		Result.ActivationResult.Reason = Result.Selection.Reason;
		return Result;
	}

	Result.ActivationResult = WBEffectRunner::ApplyCardActivationCommand(
		State,
		Result.Selection.SelectedAction.Command);
	return Result;
}
