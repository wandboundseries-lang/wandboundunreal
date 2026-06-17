#include "WBSelectedActionExecutor.h"

#include "WBEffectRunner.h"
#include "WBTurnController.h"

namespace
{
FWBApplyActionResult MakeFailureResult(const TCHAR* Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}
}

FWBApplyActionResult WBSelectedActionExecutor::ApplySelectedAction(
	FWBGameStateData& State,
	const FWBAction& Action,
	const FWBSelectedActionExecutionContext& Context)
{
	switch (Action.Type)
	{
	case EWBActionType::Move:
		return WBEffectRunner::ApplyMove(State, Action);
	case EWBActionType::EndTurn:
	{
		if (!Context.bUseFullTurnTransitionForEndTurn)
		{
			return WBEffectRunner::ApplyEndTurn(State, Action);
		}

		FWBTurnCommand Command;
		Command.Mode = EWBTurnCommandMode::DeterministicFullTransition;
		Command.ActingPlayerId = Action.PlayerId;
		Command.NextPlayerExplicitMPRoll = Context.NextPlayerExplicitMPRoll;
		return WBTurnController::ApplyTurnCommand(State, Command);
	}
	case EWBActionType::PassResponse:
		return WBEffectRunner::ApplyPassResponse(State, Action);
	case EWBActionType::Pass:
		return WBEffectRunner::ApplyPass(State, Action);
	default:
		return MakeFailureResult(TEXT("unsupported_selected_action_kind"));
	}
}
