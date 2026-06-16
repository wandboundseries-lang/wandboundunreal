#include "WBRules.h"

namespace
{
constexpr int32 BoardSize = 9;
const FWBTile MoveDirections[] = {
	FWBTile(1, 0),
	FWBTile(-1, 0),
	FWBTile(0, 1),
	FWBTile(0, -1)
};
bool HasMovementBlockingStatus(const FWBUnitState& Unit)
{
	return Unit.HasStatus(FName(TEXT("Rooted")))
		|| Unit.HasStatus(FName(TEXT("Stunned")));
}

}

FWBMoveQueryResult FWBMoveQueryResult::Ok(const int32 InCostMP)
{
	FWBMoveQueryResult Result;
	Result.bOk = true;
	Result.CostMP = InCostMP;
	return Result;
}

FWBMoveQueryResult FWBMoveQueryResult::Deny(const TCHAR* InReason)
{
	FWBMoveQueryResult Result;
	Result.bOk = false;
	Result.Reason = InReason;
	return Result;
}

FWBActionQueryResult FWBActionQueryResult::Ok()
{
	FWBActionQueryResult Result;
	Result.bOk = true;
	return Result;
}

FWBActionQueryResult FWBActionQueryResult::Deny(const TCHAR* InReason)
{
	FWBActionQueryResult Result;
	Result.bOk = false;
	Result.Reason = InReason;
	return Result;
}

bool WBRules::IsTileInBounds(const FWBTile& Tile)
{
	return Tile.X >= 0 && Tile.X < BoardSize && Tile.Y >= 0 && Tile.Y < BoardSize;
}

bool WBRules::AreOrthogonallyAdjacent(const FWBTile& A, const FWBTile& B)
{
	const int32 DeltaX = FMath::Abs(A.X - B.X);
	const int32 DeltaY = FMath::Abs(A.Y - B.Y);
	return DeltaX + DeltaY == 1;
}

bool WBRules::IsValidWallEdge(const FWBWallEdge& Edge)
{
	return IsTileInBounds(Edge.A) && IsTileInBounds(Edge.B) && AreOrthogonallyAdjacent(Edge.A, Edge.B);
}

FWBWallEdge WBRules::NormalizeWallEdge(const FWBWallEdge& Edge)
{
	return Edge.GetNormalized();
}

bool WBRules::AreSameWallEdge(const FWBWallEdge& A, const FWBWallEdge& B)
{
	return NormalizeWallEdge(A).IsSameUndirectedEdge(NormalizeWallEdge(B));
}

bool WBRules::HasWallBetween(const FWBGameStateData& State, const FWBTile& A, const FWBTile& B)
{
	const FWBWallEdge CandidateEdge(A, B);
	if (!IsValidWallEdge(CandidateEdge))
	{
		return false;
	}

	for (const FWBWallEdge& Wall : State.Walls)
	{
		if (AreSameWallEdge(Wall, CandidateEdge))
		{
			return true;
		}
	}

	return false;
}

bool WBRules::IsTileOccupied(const FWBGameStateData& State, const FWBTile& Tile)
{
	return State.IsTileOccupied(Tile);
}

const FWBUnitState* WBRules::GetUnitById(const FWBGameStateData& State, const int32 UnitId)
{
	return State.GetUnitById(UnitId);
}

FWBMoveQueryResult WBRules::QueryMove(const FWBGameStateData& State, const FWBAction& Action)
{
	if (Action.Type != EWBActionType::Move)
	{
		return FWBMoveQueryResult::Deny(TEXT("invalid_action"));
	}

	if (State.bGameOver)
	{
		return FWBMoveQueryResult::Deny(TEXT("game_over"));
	}

	if (!State.IsNormalTurnPhase())
	{
		return FWBMoveQueryResult::Deny(TEXT("not_normal_turn"));
	}

	const FWBUnitState* Unit = State.GetUnitById(Action.SourceUnitId);
	if (Unit == nullptr)
	{
		return FWBMoveQueryResult::Deny(TEXT("no_unit"));
	}

	if (HasMovementBlockingStatus(*Unit))
	{
		return FWBMoveQueryResult::Deny(TEXT("cannot_move"));
	}

	if (Unit->OwnerId == -1)
	{
		return FWBMoveQueryResult::Deny(TEXT("bad_player"));
	}

	if (!FWBGameStateData::IsValidPlayerId(Action.PlayerId))
	{
		return FWBMoveQueryResult::Deny(TEXT("bad_player"));
	}

	if (Action.PlayerId != State.CurrentPlayer)
	{
		return FWBMoveQueryResult::Deny(TEXT("not_active_player"));
	}

	if (Action.PlayerId != State.PriorityPlayer)
	{
		return FWBMoveQueryResult::Deny(TEXT("no_priority"));
	}

	if (Action.PlayerId != Unit->OwnerId)
	{
		return FWBMoveQueryResult::Deny(TEXT("wrong_player"));
	}

	const FWBPlayerStateData* Player = State.GetPlayerById(Action.PlayerId);
	if (Player == nullptr)
	{
		return FWBMoveQueryResult::Deny(TEXT("missing_player_state"));
	}

	const FWBTile UnitTile(Unit->X, Unit->Y);
	if (Action.FromTile != UnitTile)
	{
		return FWBMoveQueryResult::Deny(TEXT("source_tile_mismatch"));
	}

	if (!IsTileInBounds(Action.ToTile))
	{
		return FWBMoveQueryResult::Deny(TEXT("out_of_bounds"));
	}

	if (IsTileOccupied(State, Action.ToTile))
	{
		return FWBMoveQueryResult::Deny(TEXT("tile_occupied"));
	}

	if (!AreOrthogonallyAdjacent(Action.FromTile, Action.ToTile))
	{
		return FWBMoveQueryResult::Deny(TEXT("illegal_move_distance"));
	}

	if (HasWallBetween(State, Action.FromTile, Action.ToTile))
	{
		return FWBMoveQueryResult::Deny(TEXT("blocked_by_wall"));
	}

	if (Player->RemainingMP <= 0)
	{
		return FWBMoveQueryResult::Deny(TEXT("insufficient_mp"));
	}

	return FWBMoveQueryResult::Ok(1);
}

FWBActionQueryResult WBRules::QueryEndTurn(const FWBGameStateData& State, const FWBAction& Action)
{
	if (Action.Type != EWBActionType::EndTurn)
	{
		return FWBActionQueryResult::Deny(TEXT("invalid_action"));
	}

	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	if (!FWBGameStateData::IsValidPlayerId(Action.PlayerId))
	{
		return FWBActionQueryResult::Deny(TEXT("bad_player"));
	}

	if (Action.PlayerId != State.CurrentPlayer)
	{
		return FWBActionQueryResult::Deny(TEXT("not_active_player"));
	}

	if (Action.PlayerId != State.PriorityPlayer)
	{
		return FWBActionQueryResult::Deny(TEXT("no_priority"));
	}

	if (!State.IsNormalTurnPhase())
	{
		return FWBActionQueryResult::Deny(TEXT("not_normal_turn"));
	}

	return FWBActionQueryResult::Ok();
}

FWBActionQueryResult WBRules::QueryPass(const FWBGameStateData& State, const FWBAction& Action)
{
	if (Action.Type != EWBActionType::Pass)
	{
		return FWBActionQueryResult::Deny(TEXT("invalid_action"));
	}

	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	if (!FWBGameStateData::IsValidPlayerId(Action.PlayerId))
	{
		return FWBActionQueryResult::Deny(TEXT("bad_player"));
	}

	if (Action.PlayerId != State.PriorityPlayer)
	{
		return FWBActionQueryResult::Deny(TEXT("not_priority_player"));
	}

	if (State.CurrentPlayer == State.PriorityPlayer)
	{
		return FWBActionQueryResult::Deny(TEXT("pass_only_valid_in_response"));
	}

	return FWBActionQueryResult::Ok();
}

FWBActionQueryResult WBRules::QueryPassResponse(const FWBGameStateData& State, const FWBAction& Action)
{
	if (Action.Type != EWBActionType::PassResponse)
	{
		return FWBActionQueryResult::Deny(TEXT("invalid_action"));
	}

	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	if (!FWBGameStateData::IsValidPlayerId(Action.PlayerId))
	{
		return FWBActionQueryResult::Deny(TEXT("bad_player"));
	}

	if (!State.IsResponsePhase())
	{
		return FWBActionQueryResult::Deny(TEXT("not_response_phase"));
	}

	if (Action.PlayerId != State.PriorityPlayer)
	{
		return FWBActionQueryResult::Deny(TEXT("not_priority_player"));
	}

	return FWBActionQueryResult::Ok();
}

FWBActionQueryResult WBRules::CanEndTurn(const FWBGameStateData& State, const FWBAction& Action)
{
	return QueryEndTurn(State, Action);
}

FWBActionQueryResult WBRules::CanPassResponse(const FWBGameStateData& State, const FWBAction& Action)
{
	return QueryPassResponse(State, Action);
}

bool WBRules::CanApplyTurnStartResourceSetup(
	const FWBGameStateData& State,
	const int32 PlayerId,
	const int32 ExplicitMPRoll,
	FString& OutReason)
{
	if (State.bGameOver)
	{
		OutReason = TEXT("game_over");
		return false;
	}

	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}

	if (State.GetPlayerById(PlayerId) == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	if (ExplicitMPRoll < 1 || ExplicitMPRoll > 6)
	{
		OutReason = TEXT("invalid_mp_roll");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool WBRules::CanApplyStartOfTurnStatusTicks(
	const FWBGameStateData& State,
	const int32 PlayerId,
	FString& OutReason)
{
	if (State.bGameOver)
	{
		OutReason = TEXT("game_over");
		return false;
	}

	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}

	if (State.GetPlayerById(PlayerId) == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	if (PlayerId != State.CurrentPlayer)
	{
		OutReason = TEXT("not_active_player");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool WBRules::CanApplyEndOfTurnStatusTicks(
	const FWBGameStateData& State,
	const int32 PlayerId,
	FString& OutReason)
{
	if (State.bGameOver)
	{
		OutReason = TEXT("game_over");
		return false;
	}

	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}

	if (State.GetPlayerById(PlayerId) == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	if (PlayerId != State.CurrentPlayer)
	{
		OutReason = TEXT("not_active_player");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool WBRules::CanApplyDeterministicTurnTransition(
	const FWBGameStateData& State,
	const int32 EndingPlayerId,
	const int32 NextPlayerExplicitMPRoll,
	FString& OutReason)
{
	if (State.bGameOver)
	{
		OutReason = TEXT("game_over");
		return false;
	}

	if (!FWBGameStateData::IsValidPlayerId(EndingPlayerId))
	{
		OutReason = TEXT("bad_player");
		return false;
	}

	if (EndingPlayerId != State.CurrentPlayer)
	{
		OutReason = TEXT("not_active_player");
		return false;
	}

	if (NextPlayerExplicitMPRoll < 1 || NextPlayerExplicitMPRoll > 6)
	{
		OutReason = TEXT("invalid_mp_roll");
		return false;
	}

	FWBAction EndTurnAction;
	EndTurnAction.Type = EWBActionType::EndTurn;
	EndTurnAction.PlayerId = EndingPlayerId;
	const FWBActionQueryResult EndTurnQuery = QueryEndTurn(State, EndTurnAction);
	if (!EndTurnQuery.bOk)
	{
		OutReason = EndTurnQuery.Reason;
		return false;
	}

	if (State.GetPlayerById(EndingPlayerId) == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	const int32 NextPlayerId = EndingPlayerId == 0 ? 1 : 0;
	if (State.GetPlayerById(NextPlayerId) == nullptr)
	{
		OutReason = TEXT("missing_player_state");
		return false;
	}

	OutReason.Reset();
	return true;
}

TArray<FWBAction> WBRules::GenerateLegalMoveActions(const FWBGameStateData& State, const int32 PlayerId, const int32 UnitId)
{
	TArray<FWBAction> LegalMoves;

	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	if (Unit == nullptr)
	{
		return LegalMoves;
	}

	const FWBTile FromTile(Unit->X, Unit->Y);
	for (const FWBTile& Direction : MoveDirections)
	{
		FWBAction Candidate;
		Candidate.Type = EWBActionType::Move;
		Candidate.PlayerId = PlayerId;
		Candidate.SourceUnitId = UnitId;
		Candidate.FromTile = FromTile;
		Candidate.ToTile = FWBTile(FromTile.X + Direction.X, FromTile.Y + Direction.Y);

		if (QueryMove(State, Candidate).bOk)
		{
			LegalMoves.Add(Candidate);
		}
	}

	return LegalMoves;
}

TArray<FWBAction> WBRules::GenerateLegalActions(const FWBGameStateData& State)
{
	return GenerateLegalActionsForPlayer(State, State.PriorityPlayer);
}

void WBRules::GenerateLegalActions(const FWBGameStateData& State, const int32 PlayerId, TArray<FWBAction>& OutActions)
{
	OutActions = GenerateLegalActionsForPlayer(State, PlayerId);
}

TArray<FWBAction> WBRules::GenerateLegalActionsForPlayer(const FWBGameStateData& State, const int32 PlayerId)
{
	TArray<FWBAction> LegalActions;

	if (State.bGameOver || !FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		return LegalActions;
	}

	const bool bHasPriority = State.PriorityPlayer == PlayerId;
	const bool bCanTakeMainPhaseActions = State.IsNormalTurnPhase() && bHasPriority && State.CurrentPlayer == PlayerId;

	if (bCanTakeMainPhaseActions)
	{
		// Deterministic ordering: units are traversed in stable state order; each unit emits east, west, south, north moves; EndTurn is appended last.
		for (const FWBUnitState& Unit : State.Units)
		{
			if (Unit.OwnerId != PlayerId)
			{
				continue;
			}

			LegalActions.Append(GenerateLegalMoveActions(State, PlayerId, Unit.UnitId));
		}

		FWBAction EndTurnAction;
		EndTurnAction.Type = EWBActionType::EndTurn;
		EndTurnAction.PlayerId = PlayerId;
		LegalActions.Add(EndTurnAction);
	}
	else if (State.IsResponsePhase() && bHasPriority)
	{
		FWBAction PassResponseAction;
		PassResponseAction.Type = EWBActionType::PassResponse;
		PassResponseAction.PlayerId = PlayerId;
		LegalActions.Add(PassResponseAction);
	}
	else if (bHasPriority)
	{
		FWBAction PassAction;
		PassAction.Type = EWBActionType::Pass;
		PassAction.PlayerId = PlayerId;
		LegalActions.Add(PassAction);
	}

	return LegalActions;
}

bool WBRules::CanMoveUnit(const FWBGameStateData& State, const FWBAction& Action, FString& OutReason)
{
	const FWBMoveQueryResult Query = QueryMove(State, Action);
	OutReason = Query.Reason;

	return Query.bOk;
}
