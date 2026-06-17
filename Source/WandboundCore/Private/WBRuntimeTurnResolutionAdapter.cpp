#include "WBRuntimeTurnResolutionAdapter.h"

#include "WBActionCodec.h"
#include "WBSelectedActionExecutor.h"

namespace
{
FWBApplyActionResult MakeFailureResult(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}

void BuildFinalPublicSummaries(const FWBGameStateData& State, FWBRuntimeSelectedActionResult& Envelope)
{
	Envelope.FinalPublicTurnSummary = WBPublicTurnSummary::Build(State);
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(State);
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
	return ApplyRuntimeSelectedActionWithResult(State, SelectedAction, Context).ApplyResult;
}

FWBRuntimeSelectedActionResult WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(
	FWBGameStateData& State,
	const FWBAction& SelectedAction,
	FWBRuntimeTurnResolutionContext& Context)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionType = FName(*WBActionCodec::GetActionKind(SelectedAction.Type));
	Envelope.SelectedActionId = WBActionCodec::MakeActionId(SelectedAction);

	if (SelectedAction.Type != EWBActionType::EndTurn)
	{
		FWBSelectedActionExecutionContext SelectedActionContext;
		Envelope.ApplyResult = WBSelectedActionExecutor::ApplySelectedAction(State, SelectedAction, SelectedActionContext);
		BuildFinalPublicSummaries(State, Envelope);
		return Envelope;
	}

	if (!Context.bResolveEndTurnAsFullTransition)
	{
		FWBSelectedActionExecutionContext SelectedActionContext;
		SelectedActionContext.bUseFullTurnTransitionForEndTurn = false;
		Envelope.ApplyResult = WBSelectedActionExecutor::ApplySelectedAction(State, SelectedAction, SelectedActionContext);
		BuildFinalPublicSummaries(State, Envelope);
		return Envelope;
	}

	if (Context.MPRollSource == nullptr)
	{
		Envelope.ApplyResult = MakeFailureResult(TEXT("missing_mp_roll_source"));
		BuildFinalPublicSummaries(State, Envelope);
		return Envelope;
	}

	const int32 NextPlayerId = GetNextPlayerIdForTwoPlayerBaseline(SelectedAction.PlayerId);
	int32 NextPlayerExplicitMPRoll = 0;
	FString RollReason;
	if (!Context.MPRollSource->TryGetNextMPRoll(NextPlayerId, NextPlayerExplicitMPRoll, RollReason))
	{
		Envelope.ApplyResult = MakeFailureResult(RollReason);
		BuildFinalPublicSummaries(State, Envelope);
		return Envelope;
	}

	Envelope.bConsumedMPRoll = true;
	Envelope.ConsumedMPRoll = NextPlayerExplicitMPRoll;

	FWBSelectedActionExecutionContext SelectedActionContext;
	SelectedActionContext.bUseFullTurnTransitionForEndTurn = true;
	SelectedActionContext.NextPlayerExplicitMPRoll = NextPlayerExplicitMPRoll;
	Envelope.ApplyResult = WBSelectedActionExecutor::ApplySelectedAction(State, SelectedAction, SelectedActionContext);
	BuildFinalPublicSummaries(State, Envelope);
	return Envelope;
}
