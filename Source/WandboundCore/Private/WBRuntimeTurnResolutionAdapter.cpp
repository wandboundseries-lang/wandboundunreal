#include "WBRuntimeTurnResolutionAdapter.h"

#include "WBSelectedActionExecutor.h"

namespace
{
FWBApplyActionResult MakeFailureResult(const TCHAR* Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

int32 GetNextPlayerIdForTwoPlayerBaseline(const int32 PlayerId)
{
	return PlayerId == 0 ? 1 : 0;
}
}

FWBApplyActionResult WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(
	FWBGameStateData& State,
	const FWBAction& SelectedAction,
	FWBRuntimeTurnResolutionContext& Context)
{
	if (SelectedAction.Type != EWBActionType::EndTurn)
	{
		FWBSelectedActionExecutionContext SelectedActionContext;
		return WBSelectedActionExecutor::ApplySelectedAction(State, SelectedAction, SelectedActionContext);
	}

	if (!Context.bResolveEndTurnAsFullTransition)
	{
		FWBSelectedActionExecutionContext SelectedActionContext;
		SelectedActionContext.bUseFullTurnTransitionForEndTurn = false;
		return WBSelectedActionExecutor::ApplySelectedAction(State, SelectedAction, SelectedActionContext);
	}

	if (Context.MPRollSource == nullptr)
	{
		return MakeFailureResult(TEXT("missing_mp_roll_source"));
	}

	const int32 NextPlayerId = GetNextPlayerIdForTwoPlayerBaseline(SelectedAction.PlayerId);
	int32 NextPlayerExplicitMPRoll = 0;
	FString RollReason;
	if (!Context.MPRollSource->TryGetNextMPRoll(NextPlayerId, NextPlayerExplicitMPRoll, RollReason))
	{
		FWBApplyActionResult Result;
		Result.bOk = false;
		Result.Reason = RollReason;
		return Result;
	}

	FWBSelectedActionExecutionContext SelectedActionContext;
	SelectedActionContext.bUseFullTurnTransitionForEndTurn = true;
	SelectedActionContext.NextPlayerExplicitMPRoll = NextPlayerExplicitMPRoll;
	return WBSelectedActionExecutor::ApplySelectedAction(State, SelectedAction, SelectedActionContext);
}
