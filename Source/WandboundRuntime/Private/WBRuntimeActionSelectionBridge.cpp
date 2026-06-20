#include "WBRuntimeActionSelectionBridge.h"

#include "WBActionCodec.h"
#include "WBRuntimeControllerFacadeComponent.h"

namespace
{
FWBResolvedRuntimeActionSelection MakeSelectionFailure(
	const FString& SelectedActionId,
	const FString& Reason)
{
	FWBResolvedRuntimeActionSelection Selection;
	Selection.bOk = false;
	Selection.SelectedActionId = SelectedActionId;
	Selection.Reason = Reason;
	return Selection;
}
}

FWBResolvedRuntimeActionSelection WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FString& SelectedActionId)
{
	int32 MatchCount = 0;
	FWBAction MatchedAction;

	for (const FWBAction& CandidateAction : PrecomputedLegalActions)
	{
		if (WBActionCodec::MakeActionId(CandidateAction) == SelectedActionId)
		{
			++MatchCount;
			MatchedAction = CandidateAction;
		}
	}

	if (MatchCount == 0)
	{
		return MakeSelectionFailure(SelectedActionId, TEXT("selected_action_id_not_found"));
	}

	if (MatchCount > 1)
	{
		return MakeSelectionFailure(SelectedActionId, TEXT("selected_action_id_ambiguous"));
	}

	FWBResolvedRuntimeActionSelection Selection;
	Selection.bOk = true;
	Selection.SelectedActionId = SelectedActionId;
	Selection.ResolvedAction = MatchedAction;
	return Selection;
}

FWBRuntimeActionSelectionExecutionResult WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
	FWBGameStateData& State,
	const TArray<FWBAction>& PrecomputedLegalActions,
	const FString& SelectedActionId,
	FWBRuntimeTurnResolutionContext& RuntimeContext,
	UWBRuntimeControllerFacadeComponent* RuntimeControllerFacade)
{
	FWBRuntimeActionSelectionExecutionResult Result;
	Result.Selection = ResolveSelectedActionId(PrecomputedLegalActions, SelectedActionId);
	if (!Result.Selection.bOk)
	{
		return Result;
	}

	if (RuntimeControllerFacade == nullptr)
	{
		Result.Selection = MakeSelectionFailure(SelectedActionId, TEXT("runtime_controller_facade_missing"));
		return Result;
	}

	Result.Execution = RuntimeControllerFacade->ExecuteSelectedAction(
		State,
		Result.Selection.ResolvedAction,
		RuntimeContext);
	return Result;
}
