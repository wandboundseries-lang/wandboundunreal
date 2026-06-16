#include "WBEffectRunner.h"

#include "WBRules.h"

FWBApplyActionResult WBEffectRunner::ApplyAction(FWBGameStateData& State, const FWBAction& Action)
{
	switch (Action.Type)
	{
	case EWBActionType::Move:
		return ApplyMove(State, Action);
	case EWBActionType::EndTurn:
		return ApplyEndTurn(State, Action);
	case EWBActionType::Pass:
		return ApplyPass(State, Action);
	default:
		FWBApplyActionResult Result;
		Result.bOk = false;
		Result.Reason = TEXT("unsupported_action_kind");
		return Result;
	}
}

FWBApplyActionResult WBEffectRunner::ApplyMove(FWBGameStateData& State, const FWBAction& Action)
{
	FWBApplyActionResult Result;

	const FWBMoveQueryResult MoveQuery = WBRules::QueryMove(State, Action);
	if (!MoveQuery.bOk)
	{
		Result.bOk = false;
		Result.Reason = MoveQuery.Reason;
		return Result;
	}

	FWBUnitState* Unit = State.GetMutableUnitById(Action.SourceUnitId);
	if (Unit == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("Source unit disappeared before movement could be applied.");
		return Result;
	}

	const FWBTile OriginalTile(Unit->X, Unit->Y);
	Unit->X = Action.ToTile.X;
	Unit->Y = Action.ToTile.Y;
	Unit->MPRemaining -= MoveQuery.CostMP;

	FWBTraceEvent MoveEvent;
	MoveEvent.Kind = FName(TEXT("move"));
	MoveEvent.PlayerId = Action.PlayerId;
	MoveEvent.SourceUnitId = Action.SourceUnitId;
	MoveEvent.FromTile = OriginalTile;
	MoveEvent.ToTile = Action.ToTile;
	MoveEvent.bOk = true;

	Result.bOk = true;
	Result.TraceEvents.Add(MoveEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyEndTurn(FWBGameStateData& State, const FWBAction& Action)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult Query = WBRules::QueryEndTurn(State, Action);
	if (!Query.bOk)
	{
		Result.bOk = false;
		Result.Reason = Query.Reason;
		return Result;
	}

	const int32 PreviousPlayer = State.CurrentPlayer;
	const int32 NextPlayer = 1 - PreviousPlayer;

	State.CurrentPlayer = NextPlayer;
	State.PriorityPlayer = NextPlayer;
	State.TurnNumber += 1;

	FWBTraceEvent EndTurnEvent;
	EndTurnEvent.Kind = FName(TEXT("end_turn"));
	EndTurnEvent.PlayerId = Action.PlayerId;
	EndTurnEvent.FromPlayer = PreviousPlayer;
	EndTurnEvent.ToPlayer = NextPlayer;
	EndTurnEvent.TurnNumber = State.TurnNumber;
	EndTurnEvent.bOk = true;

	Result.bOk = true;
	Result.TraceEvents.Add(EndTurnEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyPass(FWBGameStateData& State, const FWBAction& Action)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult Query = WBRules::QueryPass(State, Action);
	if (!Query.bOk)
	{
		Result.bOk = false;
		Result.Reason = Query.Reason;
		return Result;
	}

	const int32 PreviousPriorityPlayer = State.PriorityPlayer;
	State.PriorityPlayer = State.CurrentPlayer;

	FWBTraceEvent PassEvent;
	PassEvent.Kind = FName(TEXT("pass"));
	PassEvent.PlayerId = Action.PlayerId;
	PassEvent.FromPlayer = PreviousPriorityPlayer;
	PassEvent.ToPlayer = State.PriorityPlayer;
	PassEvent.TurnNumber = State.TurnNumber;
	PassEvent.bOk = true;

	Result.bOk = true;
	Result.TraceEvents.Add(PassEvent);
	return Result;
}
