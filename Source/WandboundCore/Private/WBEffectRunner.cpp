#include "WBEffectRunner.h"

#include "WBRules.h"

namespace
{
const FName BurnStatusId(TEXT("Burn"));
const FName PoisonStatusId(TEXT("Poison"));
const FName RootedStatusId(TEXT("Rooted"));
const FName StunnedStatusId(TEXT("Stunned"));
const FName FrozenStatusId(TEXT("Frozen"));

void AppendStartTurnStatusTickTrace(TArray<FWBTraceEvent>& TraceEvents, const int32 PlayerId, const int32 TurnNumber)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("start_turn_status_ticks"));
	Event.PlayerId = PlayerId;
	Event.TurnNumber = TurnNumber;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendEndTurnStatusTickTrace(TArray<FWBTraceEvent>& TraceEvents, const int32 PlayerId, const int32 TurnNumber)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("end_turn_status_ticks"));
	Event.PlayerId = PlayerId;
	Event.TurnNumber = TurnNumber;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

FWBTraceEvent MakeTurnTransitionTrace(
	const int32 EndingPlayerId,
	const int32 NextPlayerId,
	const int32 TurnNumberBefore,
	const int32 TurnNumberAfter,
	const int32 NextPlayerExplicitMPRoll)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("turn_transition"));
	Event.PlayerId = EndingPlayerId;
	Event.FromPlayer = EndingPlayerId;
	Event.ToPlayer = NextPlayerId;
	Event.NextPlayerId = NextPlayerId;
	Event.TurnNumberBefore = TurnNumberBefore;
	Event.TurnNumberAfter = TurnNumberAfter;
	Event.MPRoll = NextPlayerExplicitMPRoll;
	Event.bOk = true;
	return Event;
}

void AppendBurnTickTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const int32 PlayerId,
	const int32 TargetUnitId,
	const int32 PreviousHP,
	const int32 NewHP,
	const int32 PreviousMaxHP,
	const int32 NewMaxHP,
	const int32 PreviousStatusTurns,
	const int32 NewStatusTurns)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_tick"));
	Event.StatusId = BurnStatusId;
	Event.PlayerId = PlayerId;
	Event.TargetUnitId = TargetUnitId;
	Event.PreviousHP = PreviousHP;
	Event.NewHP = NewHP;
	Event.PreviousMaxHP = PreviousMaxHP;
	Event.NewMaxHP = NewMaxHP;
	Event.PreviousStatusTurns = PreviousStatusTurns;
	Event.NewStatusTurns = NewStatusTurns;
	Event.bAtOrBelowZeroHP = NewHP <= 0;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendPoisonTickTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const int32 PlayerId,
	const int32 TargetUnitId,
	const int32 PreviousHP,
	const int32 NewHP,
	const int32 PreviousMaxHP,
	const int32 NewMaxHP,
	const int32 PreviousStatusTurns,
	const int32 NewStatusTurns)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_tick"));
	Event.StatusId = PoisonStatusId;
	Event.PlayerId = PlayerId;
	Event.TargetUnitId = TargetUnitId;
	Event.PreviousHP = PreviousHP;
	Event.NewHP = NewHP;
	Event.PreviousMaxHP = PreviousMaxHP;
	Event.NewMaxHP = NewMaxHP;
	Event.PreviousStatusTurns = PreviousStatusTurns;
	Event.NewStatusTurns = NewStatusTurns;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendStatusExpiredTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const int32 PlayerId,
	const FName StatusId,
	const int32 TargetUnitId,
	const int32 PreviousStatusTurns)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_expired"));
	Event.StatusId = StatusId;
	Event.PlayerId = PlayerId;
	Event.TargetUnitId = TargetUnitId;
	Event.PreviousStatusTurns = PreviousStatusTurns;
	Event.NewStatusTurns = 0;
	Event.bExpiredStatus = true;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

bool DecayTimedStatus(FWBUnitState& Unit, const FName StatusId, int32& OutPreviousStatusTurns, int32& OutNewStatusTurns)
{
	OutPreviousStatusTurns = Unit.GetStatusTurnsRemaining(StatusId);
	OutNewStatusTurns = OutPreviousStatusTurns;
	if (OutPreviousStatusTurns <= 0)
	{
		return false;
	}

	OutNewStatusTurns = OutPreviousStatusTurns - 1;
	if (OutNewStatusTurns <= 0)
	{
		Unit.RemoveStatus(StatusId);
		OutNewStatusTurns = 0;
		return true;
	}

	Unit.SetStatusTurnsRemaining(StatusId, OutNewStatusTurns);
	return false;
}
}

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
	case EWBActionType::PassResponse:
		return ApplyPassResponse(State, Action);
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

	FWBPlayerStateData* Player = State.GetMutablePlayerById(Action.PlayerId);
	if (Player == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("missing_player_state");
		return Result;
	}

	const FWBTile OriginalTile(Unit->X, Unit->Y);
	Unit->X = Action.ToTile.X;
	Unit->Y = Action.ToTile.Y;
	Player->RemainingMP -= MoveQuery.CostMP;
	// Legacy fixture mirror only. Movement legality and spending use player RemainingMP.
	Unit->MPRemaining = FMath::Max(Unit->MPRemaining - MoveQuery.CostMP, 0);

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
	State.AdvanceTurnBasic();

	FWBTraceEvent EndTurnEvent;
	EndTurnEvent.Kind = FName(TEXT("end_turn"));
	EndTurnEvent.PlayerId = Action.PlayerId;
	EndTurnEvent.FromPlayer = PreviousPlayer;
	EndTurnEvent.ToPlayer = State.CurrentPlayer;
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
	State.Phase = EWBGamePhase::NormalTurn;

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

FWBApplyActionResult WBEffectRunner::ApplyStartOfTurnStatusTicks(FWBGameStateData& State, const int32 PlayerId)
{
	FWBApplyActionResult Result;

	FString Reason;
	if (!WBRules::CanApplyStartOfTurnStatusTicks(State, PlayerId, Reason))
	{
		Result.bOk = false;
		Result.Reason = Reason;
		return Result;
	}

	Result.bOk = true;
	AppendStartTurnStatusTickTrace(Result.TraceEvents, PlayerId, State.TurnNumber);

	for (FWBUnitState& Unit : State.Units)
	{
		if (!Unit.HasStatus(PoisonStatusId) || Unit.HasStatus(FrozenStatusId))
		{
			continue;
		}

		const int32 PreviousHP = Unit.HP;
		const int32 PreviousMaxHP = Unit.MaxHP;
		const int32 PreviousStatusTurns = Unit.GetStatusTurnsRemaining(PoisonStatusId);
		const int32 NewMaxHP = FMath::Max(PreviousMaxHP - 1, 1);

		Unit.MaxHP = NewMaxHP;
		Unit.HP = FMath::Min(Unit.HP, Unit.MaxHP);

		int32 NewStatusTurns = PreviousStatusTurns;
		bool bExpired = false;
		if (PreviousStatusTurns > 0)
		{
			NewStatusTurns = PreviousStatusTurns - 1;
			if (NewStatusTurns <= 0)
			{
				Unit.RemoveStatus(PoisonStatusId);
				bExpired = true;
				NewStatusTurns = 0;
			}
			else
			{
				Unit.SetStatusTurnsRemaining(PoisonStatusId, NewStatusTurns);
			}
		}

		AppendPoisonTickTrace(
			Result.TraceEvents,
			PlayerId,
			Unit.UnitId,
			PreviousHP,
			Unit.HP,
			PreviousMaxHP,
			Unit.MaxHP,
			PreviousStatusTurns,
			NewStatusTurns);

		if (bExpired)
		{
			AppendStatusExpiredTrace(Result.TraceEvents, PlayerId, PoisonStatusId, Unit.UnitId, PreviousStatusTurns);
		}
	}

	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyEndOfTurnStatusTicks(FWBGameStateData& State, const int32 PlayerId)
{
	FWBApplyActionResult Result;

	FString Reason;
	if (!WBRules::CanApplyEndOfTurnStatusTicks(State, PlayerId, Reason))
	{
		Result.bOk = false;
		Result.Reason = Reason;
		return Result;
	}

	Result.bOk = true;
	AppendEndTurnStatusTickTrace(Result.TraceEvents, PlayerId, State.TurnNumber);

	for (FWBUnitState& Unit : State.Units)
	{
		if (Unit.HasStatus(BurnStatusId))
		{
			const int32 PreviousHP = Unit.HP;
			const int32 PreviousMaxHP = Unit.MaxHP;
			int32 PreviousStatusTurns = 0;
			int32 NewStatusTurns = 0;
			const bool bExpired = DecayTimedStatus(Unit, BurnStatusId, PreviousStatusTurns, NewStatusTurns);

			Unit.HP = FMath::Max(Unit.HP - 1, 0);
			AppendBurnTickTrace(
				Result.TraceEvents,
				PlayerId,
				Unit.UnitId,
				PreviousHP,
				Unit.HP,
				PreviousMaxHP,
				Unit.MaxHP,
				PreviousStatusTurns,
				NewStatusTurns);

			if (bExpired)
			{
				AppendStatusExpiredTrace(Result.TraceEvents, PlayerId, BurnStatusId, Unit.UnitId, PreviousStatusTurns);
			}
		}

		const FName EndTurnDurationStatusIds[] = {
			RootedStatusId,
			StunnedStatusId,
			FrozenStatusId
		};

		for (const FName StatusId : EndTurnDurationStatusIds)
		{
			if (!Unit.HasStatus(StatusId))
			{
				continue;
			}

			int32 PreviousStatusTurns = 0;
			int32 NewStatusTurns = 0;
			if (DecayTimedStatus(Unit, StatusId, PreviousStatusTurns, NewStatusTurns))
			{
				AppendStatusExpiredTrace(Result.TraceEvents, PlayerId, StatusId, Unit.UnitId, PreviousStatusTurns);
			}
		}
	}

	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyDeterministicTurnTransition(
	FWBGameStateData& State,
	const int32 EndingPlayerId,
	const int32 NextPlayerExplicitMPRoll)
{
	FWBApplyActionResult Result;

	FString Reason;
	if (!WBRules::CanApplyDeterministicTurnTransition(State, EndingPlayerId, NextPlayerExplicitMPRoll, Reason))
	{
		Result.bOk = false;
		Result.Reason = Reason;
		return Result;
	}

	FWBGameStateData WorkingState = State;
	const int32 TurnNumberBefore = WorkingState.TurnNumber;

	FWBApplyActionResult EndStatusResult = ApplyEndOfTurnStatusTicks(WorkingState, EndingPlayerId);
	if (!EndStatusResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = EndStatusResult.Reason;
		return Result;
	}

	FWBAction EndTurnAction;
	EndTurnAction.Type = EWBActionType::EndTurn;
	EndTurnAction.PlayerId = EndingPlayerId;
	FWBApplyActionResult EndTurnResult = ApplyEndTurn(WorkingState, EndTurnAction);
	if (!EndTurnResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = EndTurnResult.Reason;
		return Result;
	}

	const int32 NextPlayerId = WorkingState.CurrentPlayer;
	FWBApplyActionResult StartStatusResult = ApplyStartOfTurnStatusTicks(WorkingState, NextPlayerId);
	if (!StartStatusResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = StartStatusResult.Reason;
		return Result;
	}

	FWBApplyActionResult ResourceSetupResult = ApplyTurnStartResourceSetup(
		WorkingState,
		NextPlayerId,
		NextPlayerExplicitMPRoll);
	if (!ResourceSetupResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = ResourceSetupResult.Reason;
		return Result;
	}

	const int32 TurnNumberAfter = WorkingState.TurnNumber;
	Result.bOk = true;
	Result.TraceEvents.Add(MakeTurnTransitionTrace(
		EndingPlayerId,
		NextPlayerId,
		TurnNumberBefore,
		TurnNumberAfter,
		NextPlayerExplicitMPRoll));
	Result.TraceEvents.Append(EndStatusResult.TraceEvents);
	Result.TraceEvents.Append(EndTurnResult.TraceEvents);
	Result.TraceEvents.Append(StartStatusResult.TraceEvents);
	Result.TraceEvents.Append(ResourceSetupResult.TraceEvents);

	State = WorkingState;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyTurnStartResourceSetup(
	FWBGameStateData& State,
	const int32 PlayerId,
	const int32 ExplicitMPRoll)
{
	FWBApplyActionResult Result;

	FString Reason;
	if (!WBRules::CanApplyTurnStartResourceSetup(State, PlayerId, ExplicitMPRoll, Reason))
	{
		Result.bOk = false;
		Result.Reason = Reason;
		return Result;
	}

	if (!State.ApplyTurnStartResourceSetupForPlayer(PlayerId, ExplicitMPRoll, Reason))
	{
		Result.bOk = false;
		Result.Reason = Reason;
		return Result;
	}

	const FWBPlayerStateData* Player = State.GetPlayerById(PlayerId);
	if (Player == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("missing_player_state");
		return Result;
	}

	FWBTraceEvent TurnStartEvent;
	TurnStartEvent.Kind = FName(TEXT("turn_start_resource_setup"));
	TurnStartEvent.PlayerId = PlayerId;
	TurnStartEvent.TurnNumber = State.TurnNumber;
	TurnStartEvent.MPRoll = ExplicitMPRoll;
	TurnStartEvent.RemainingMP = Player->RemainingMP;
	TurnStartEvent.WallsLeft = Player->WallsLeft;
	TurnStartEvent.WallRemovalsLeft = Player->WallRemovalsLeft;
	TurnStartEvent.bOk = true;

	Result.bOk = true;
	Result.TraceEvents.Add(TurnStartEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyPassResponse(FWBGameStateData& State, const FWBAction& Action)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult Query = WBRules::QueryPassResponse(State, Action);
	if (!Query.bOk)
	{
		Result.bOk = false;
		Result.Reason = Query.Reason;
		return Result;
	}

	const int32 PreviousPriorityPlayer = State.PriorityPlayer;
	State.PriorityPlayer = State.CurrentPlayer;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBTraceEvent PassEvent;
	PassEvent.Kind = FName(TEXT("pass_response"));
	PassEvent.PlayerId = Action.PlayerId;
	PassEvent.FromPlayer = PreviousPriorityPlayer;
	PassEvent.ToPlayer = State.PriorityPlayer;
	PassEvent.TurnNumber = State.TurnNumber;
	PassEvent.bOk = true;

	Result.bOk = true;
	Result.TraceEvents.Add(PassEvent);
	return Result;
}
