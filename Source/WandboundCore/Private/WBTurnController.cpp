#include "WBTurnController.h"

#include "WBAction.h"
#include "WBEffectRunner.h"
#include "WBRules.h"

namespace
{
FWBAction MakeEndTurnAction(const int32 PlayerId)
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = PlayerId;
	return Action;
}

FWBApplyActionResult MakeFailureResult(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	return Result;
}
}

bool WBTurnController::CanApplyTurnCommand(
	const FWBGameStateData& State,
	const FWBTurnCommand& Command,
	FString& OutReason)
{
	switch (Command.Mode)
	{
	case EWBTurnCommandMode::BasicEndTurn:
	{
		const FWBActionQueryResult Query = WBRules::CanEndTurn(State, MakeEndTurnAction(Command.ActingPlayerId));
		if (!Query.bOk)
		{
			OutReason = Query.Reason;
			return false;
		}

		OutReason.Reset();
		return true;
	}
	case EWBTurnCommandMode::DeterministicFullTransition:
		return WBRules::CanApplyDeterministicTurnTransition(
			State,
			Command.ActingPlayerId,
			Command.NextPlayerExplicitMPRoll,
			OutReason);
	default:
		OutReason = TEXT("unsupported_turn_command_mode");
		return false;
	}
}

FWBApplyActionResult WBTurnController::ApplyTurnCommand(FWBGameStateData& State, const FWBTurnCommand& Command)
{
	FString Reason;
	if (!CanApplyTurnCommand(State, Command, Reason))
	{
		return MakeFailureResult(Reason);
	}

	switch (Command.Mode)
	{
	case EWBTurnCommandMode::BasicEndTurn:
		return WBEffectRunner::ApplyEndTurn(State, MakeEndTurnAction(Command.ActingPlayerId));
	case EWBTurnCommandMode::DeterministicFullTransition:
		return WBEffectRunner::ApplyDeterministicTurnTransition(
			State,
			Command.ActingPlayerId,
			Command.NextPlayerExplicitMPRoll);
	default:
		return MakeFailureResult(TEXT("unsupported_turn_command_mode"));
	}
}
