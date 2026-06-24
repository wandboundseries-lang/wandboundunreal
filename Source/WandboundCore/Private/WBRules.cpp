#include "WBRules.h"

#include "WBCardActivationCommand.h"
#include "WBCardActivationCostPayment.h"
#include "WBEffectRequest.h"
#include "WBStatusEffect.h"

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

bool HasAttackBlockingStatus(const FWBUnitState& Unit)
{
	return Unit.HasStatus(FName(TEXT("Stunned")))
		|| Unit.HasStatus(FName(TEXT("Frozen")))
		|| Unit.HasStatus(FName(TEXT("CannotAttack")))
		|| Unit.HasStatus(FName(TEXT("Cannot Attack")))
		|| Unit.HasStatus(FName(TEXT("cannot_attack")))
		|| Unit.HasStatus(FName(TEXT("no_attack")));
}

bool IsFrozen(const FWBUnitState& Unit)
{
	return Unit.HasStatus(FName(TEXT("Frozen")));
}

bool UnitTileLess(const FWBUnitState& A, const FWBUnitState& B)
{
	if (A.Y != B.Y)
	{
		return A.Y < B.Y;
	}

	if (A.X != B.X)
	{
		return A.X < B.X;
	}

	return A.UnitId < B.UnitId;
}

TArray<FWBAction> GenerateLegalAttackActions(const FWBGameStateData& State, const int32 PlayerId)
{
	TArray<const FWBUnitState*> Attackers;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.OwnerId == PlayerId && Unit.IsUnitOnBoard() && !Unit.bDefeated)
		{
			Attackers.Add(&Unit);
		}
	}
	Attackers.Sort(UnitTileLess);

	TArray<const FWBUnitState*> Targets;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard() && !Unit.bDefeated)
		{
			Targets.Add(&Unit);
		}
	}
	Targets.Sort(UnitTileLess);

	TArray<FWBAction> LegalAttacks;
	for (const FWBUnitState* Attacker : Attackers)
	{
		if (Attacker == nullptr)
		{
			continue;
		}

		for (const FWBUnitState* Target : Targets)
		{
			if (Target == nullptr || Target->UnitId == Attacker->UnitId)
			{
				continue;
			}

			FWBAction Candidate;
			Candidate.Type = EWBActionType::Attack;
			Candidate.PlayerId = PlayerId;
			Candidate.SourceUnitId = Attacker->UnitId;
			Candidate.TargetUnitId = Target->UnitId;
			Candidate.FromTile = FWBTile(Attacker->X, Attacker->Y);
			Candidate.ToTile = FWBTile(Target->X, Target->Y);

			if (WBRules::CanDeclareAttack(State, Candidate).bOk)
			{
				LegalAttacks.Add(Candidate);
			}
		}
	}

	return LegalAttacks;
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

int32 WBRules::OrthogonalDistance(const FWBTile& A, const FWBTile& B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

bool WBRules::AreTilesOrthogonallyAligned(const FWBTile& A, const FWBTile& B)
{
	return A != B && (A.X == B.X || A.Y == B.Y);
}

bool WBRules::HasOrthogonalLineOfSight(
	const FWBGameStateData& State,
	const FWBTile& From,
	const FWBTile& To,
	FString& OutReason)
{
	if (!IsTileInBounds(From) || !IsTileInBounds(To))
	{
		OutReason = TEXT("out_of_bounds");
		return false;
	}

	if (!AreTilesOrthogonallyAligned(From, To))
	{
		OutReason = TEXT("not_in_line");
		return false;
	}

	const int32 StepX = From.X == To.X ? 0 : (To.X > From.X ? 1 : -1);
	const int32 StepY = From.Y == To.Y ? 0 : (To.Y > From.Y ? 1 : -1);
	FWBTile Current = From;
	while (Current != To)
	{
		const FWBTile Next(Current.X + StepX, Current.Y + StepY);
		if (HasWallBetween(State, Current, Next))
		{
			OutReason = TEXT("blocked_by_wall");
			return false;
		}

		if (Next != To && State.UnitIdAt(Next) != -1)
		{
			OutReason = TEXT("blocked_by_unit");
			return false;
		}

		Current = Next;
	}

	OutReason.Reset();
	return true;
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

	if (Unit->bDefeated || !Unit->IsUnitOnBoard())
	{
		return FWBMoveQueryResult::Deny(TEXT("unit_removed"));
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

FWBActionQueryResult WBRules::CanDeclareAttack(const FWBGameStateData& State, const FWBAction& Action)
{
	if (Action.Type != EWBActionType::Attack)
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

	if (State.HasPendingAttack())
	{
		return FWBActionQueryResult::Deny(TEXT("pending_attack_already_active"));
	}

	const FWBUnitState* Attacker = State.GetUnitById(Action.SourceUnitId);
	if (Attacker == nullptr)
	{
		return FWBActionQueryResult::Deny(TEXT("no_attacker"));
	}

	if (Attacker->bDefeated || !Attacker->IsUnitOnBoard())
	{
		return FWBActionQueryResult::Deny(TEXT("attacker_removed"));
	}

	const FWBUnitState* Defender = State.GetUnitById(Action.TargetUnitId);
	if (Defender == nullptr)
	{
		return FWBActionQueryResult::Deny(TEXT("no_target"));
	}

	if (Defender->bDefeated || !Defender->IsUnitOnBoard())
	{
		return FWBActionQueryResult::Deny(TEXT("target_removed"));
	}

	if (Attacker->UnitId == Defender->UnitId)
	{
		return FWBActionQueryResult::Deny(TEXT("same_unit"));
	}

	if (Attacker->OwnerId != Action.PlayerId)
	{
		return FWBActionQueryResult::Deny(TEXT("wrong_player"));
	}

	if (Defender->OwnerId == Attacker->OwnerId)
	{
		if (!IsFrozen(*Defender))
		{
			return FWBActionQueryResult::Deny(TEXT("friendly_target"));
		}
	}

	const FWBTile AttackerTile(Attacker->X, Attacker->Y);
	const FWBTile DefenderTile(Defender->X, Defender->Y);
	if (!IsTileInBounds(AttackerTile) || !IsTileInBounds(DefenderTile))
	{
		return FWBActionQueryResult::Deny(TEXT("out_of_bounds"));
	}

	if (Attacker->AttacksLeft <= 0)
	{
		return FWBActionQueryResult::Deny(TEXT("no_attacks_left"));
	}

	if (HasAttackBlockingStatus(*Attacker))
	{
		return FWBActionQueryResult::Deny(TEXT("cannot_attack"));
	}

	if (!AreTilesOrthogonallyAligned(AttackerTile, DefenderTile))
	{
		return FWBActionQueryResult::Deny(TEXT("not_in_line"));
	}

	if (OrthogonalDistance(AttackerTile, DefenderTile) > Attacker->AR)
	{
		return FWBActionQueryResult::Deny(TEXT("out_of_range"));
	}

	FString LOSReason;
	if (!HasOrthogonalLineOfSight(State, AttackerTile, DefenderTile, LOSReason))
	{
		return FWBActionQueryResult::Deny(*LOSReason);
	}

	return FWBActionQueryResult::Ok();
}

FWBActionQueryResult WBRules::CanResolvePendingAttackDamage(const FWBGameStateData& State)
{
	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	if (!State.HasPendingAttack())
	{
		return FWBActionQueryResult::Deny(TEXT("missing_pending_attack"));
	}

	if (!FWBGameStateData::IsValidPlayerId(State.PendingAttack.AttackingPlayerId))
	{
		return FWBActionQueryResult::Deny(TEXT("bad_player"));
	}

	if (State.PendingAttack.AttackerUnitId == State.PendingAttack.DefenderUnitId)
	{
		return FWBActionQueryResult::Deny(TEXT("same_unit"));
	}

	const FWBUnitState* Attacker = State.GetUnitById(State.PendingAttack.AttackerUnitId);
	if (Attacker == nullptr)
	{
		return FWBActionQueryResult::Deny(TEXT("missing_attacker"));
	}

	if (Attacker->bDefeated || !Attacker->IsUnitOnBoard())
	{
		return FWBActionQueryResult::Deny(TEXT("attacker_removed"));
	}

	const FWBUnitState* Defender = State.GetUnitById(State.PendingAttack.DefenderUnitId);
	if (Defender == nullptr)
	{
		return FWBActionQueryResult::Deny(TEXT("missing_defender"));
	}

	if (Defender->bDefeated || !Defender->IsUnitOnBoard())
	{
		return FWBActionQueryResult::Deny(TEXT("defender_removed"));
	}

	return FWBActionQueryResult::Ok();
}

bool WBRules::ShouldUnitBeDefeatedAtZeroHP(const FWBGameStateData& State, const FWBUnitState& Unit)
{
	(void)State;
	// TODO: Future prevention/replacement effects should hook in before returning true.
	return Unit.HP <= 0 && !Unit.bDefeated && Unit.IsUnitOnBoard();
}

FWBActionQueryResult WBRules::CanApplyZeroHPDeathRemoval(const FWBGameStateData& State)
{
	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	for (const FWBUnitState& Unit : State.Units)
	{
		if (ShouldUnitBeDefeatedAtZeroHP(State, Unit))
		{
			return FWBActionQueryResult::Ok();
		}
	}

	return FWBActionQueryResult::Deny(TEXT("no_zero_hp_units"));
}

FWBActionQueryResult WBRules::CanApplyCardActivationCommand(
	const FWBGameStateData& State,
	const FWBCardActivationCommand& Command)
{
	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	if (!FWBGameStateData::IsValidPlayerId(Command.Source.PlayerId))
	{
		return FWBActionQueryResult::Deny(TEXT("invalid_card_activation_source_player"));
	}

	if (State.GetPlayerById(Command.Source.PlayerId) == nullptr)
	{
		return FWBActionQueryResult::Deny(TEXT("missing_card_activation_source_player"));
	}

	if (Command.Source.SourceUnitId != -1)
	{
		const FWBUnitState* SourceUnit = State.GetUnitById(Command.Source.SourceUnitId);
		if (SourceUnit == nullptr)
		{
			return FWBActionQueryResult::Deny(TEXT("missing_card_activation_source_unit"));
		}

		if (SourceUnit->bDefeated || !SourceUnit->IsUnitOnBoard())
		{
			return FWBActionQueryResult::Deny(TEXT("card_activation_source_unit_removed"));
		}

		if (SourceUnit->OwnerId != Command.Source.PlayerId)
		{
			return FWBActionQueryResult::Deny(TEXT("card_activation_source_owner_mismatch"));
		}
	}

	if (Command.EffectRequest.Source.PlayerId != -1
		&& Command.EffectRequest.Source.PlayerId != Command.Source.PlayerId)
	{
		return FWBActionQueryResult::Deny(TEXT("card_activation_effect_source_player_mismatch"));
	}

	if (Command.EffectRequest.Source.SourceUnitId != -1
		&& Command.EffectRequest.Source.SourceUnitId != Command.Source.SourceUnitId)
	{
		return FWBActionQueryResult::Deny(TEXT("card_activation_effect_source_unit_mismatch"));
	}

	if (Command.EffectRequest.Payloads.Num() == 0)
	{
		return FWBActionQueryResult::Deny(TEXT("empty_effect_payloads"));
	}

	if (Command.CostPaymentCommit.bPayCostOnSuccess)
	{
		FWBCardActivationCostPaymentRequest PaymentRequest;
		PaymentRequest.PlayerId = Command.CostPaymentCommit.PlayerId;
		PaymentRequest.SourceUnitId = Command.CostPaymentCommit.SourceUnitId;
		PaymentRequest.RequiredRR = Command.CostPaymentCommit.RequiredRR;
		PaymentRequest.CostKind = Command.CostPaymentCommit.CostKind;

		const FWBCardActivationCostPaymentResult PaymentResult =
			WBCardActivationCostPayment::CanPayCost(State, PaymentRequest);
		if (!PaymentResult.bOk)
		{
			return FWBActionQueryResult::Deny(*PaymentResult.Reason);
		}
	}

	if (Command.UsageCommit.bMarkUsageOnSuccess)
	{
		if (!FWBGameStateData::IsValidPlayerId(Command.UsageCommit.PlayerId)
			|| Command.UsageCommit.UsageKey.IsEmpty())
		{
			return FWBActionQueryResult::Deny(TEXT("usage_commit_invalid"));
		}

		if (State.HasActivationUsageKeyThisTurn(Command.UsageCommit.PlayerId, Command.UsageCommit.UsageKey))
		{
			return FWBActionQueryResult::Deny(TEXT("once_per_turn_already_used"));
		}
	}

	FWBEffectRequest FilledRequest = Command.EffectRequest;
	if (FilledRequest.Source.PlayerId == -1)
	{
		FilledRequest.Source.PlayerId = Command.Source.PlayerId;
	}
	if (FilledRequest.Source.SourceUnitId == -1)
	{
		FilledRequest.Source.SourceUnitId = Command.Source.SourceUnitId;
	}
	if (FilledRequest.Source.SourceCardId.IsEmpty())
	{
		FilledRequest.Source.SourceCardId = Command.Source.SourceCardId;
	}
	if (FilledRequest.Source.SourceEffectId.IsEmpty())
	{
		FilledRequest.Source.SourceEffectId = Command.Source.SourceEffectId;
	}

	return CanApplyEffectRequest(State, FilledRequest);
}

FWBActionQueryResult WBRules::CanApplyEffectRequest(
	const FWBGameStateData& State,
	const FWBEffectRequest& Request)
{
	if (State.bGameOver)
	{
		return FWBActionQueryResult::Deny(TEXT("game_over"));
	}

	if (Request.Source.PlayerId != -1 && !FWBGameStateData::IsValidPlayerId(Request.Source.PlayerId))
	{
		return FWBActionQueryResult::Deny(TEXT("invalid_effect_source_player"));
	}

	if (Request.Source.SourceUnitId != -1)
	{
		const FWBUnitState* SourceUnit = State.GetUnitById(Request.Source.SourceUnitId);
		if (SourceUnit == nullptr)
		{
			return FWBActionQueryResult::Deny(TEXT("missing_effect_source_unit"));
		}

		if (SourceUnit->bDefeated || !SourceUnit->IsUnitOnBoard())
		{
			return FWBActionQueryResult::Deny(TEXT("effect_source_unit_removed"));
		}
	}

	if (Request.Payloads.Num() == 0)
	{
		return FWBActionQueryResult::Deny(TEXT("empty_effect_payloads"));
	}

	for (const FWBGenericEffectPayload& Payload : Request.Payloads)
	{
		switch (Payload.Operation)
		{
		case EWBGenericEffectOp::ArmorEffect:
		{
			const int32 RequestTargetUnitId = Request.Target.TargetUnitId;
			if (RequestTargetUnitId == -1)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (Payload.ArmorEffect.TargetUnitId != -1
				&& Payload.ArmorEffect.TargetUnitId != RequestTargetUnitId)
			{
				return FWBActionQueryResult::Deny(TEXT("effect_payload_target_mismatch"));
			}

			const FWBUnitState* TargetUnit = State.GetUnitById(RequestTargetUnitId);
			if (TargetUnit == nullptr)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (TargetUnit->bDefeated || !TargetUnit->IsUnitOnBoard())
			{
				return FWBActionQueryResult::Deny(TEXT("effect_target_unit_removed"));
			}

			if (Payload.ArmorEffect.Operation == EWBArmorEffectOp::Unknown)
			{
				return FWBActionQueryResult::Deny(TEXT("unknown_armor_effect_operation"));
			}

			if (Payload.ArmorEffect.Amount < 0)
			{
				return FWBActionQueryResult::Deny(TEXT("negative_armor_effect_amount"));
			}
			break;
		}
		case EWBGenericEffectOp::StatusEffect:
		{
			const int32 RequestTargetUnitId = Request.Target.TargetUnitId;
			if (RequestTargetUnitId == -1)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (Payload.StatusEffect.TargetUnitId != -1
				&& Payload.StatusEffect.TargetUnitId != RequestTargetUnitId)
			{
				return FWBActionQueryResult::Deny(TEXT("effect_payload_target_mismatch"));
			}

			const FWBUnitState* TargetUnit = State.GetUnitById(RequestTargetUnitId);
			if (TargetUnit == nullptr)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (TargetUnit->bDefeated || !TargetUnit->IsUnitOnBoard())
			{
				return FWBActionQueryResult::Deny(TEXT("effect_target_unit_removed"));
			}

			if (Payload.StatusEffect.Operation == EWBStatusEffectOp::Unknown)
			{
				return FWBActionQueryResult::Deny(TEXT("unknown_status_effect_operation"));
			}

			if (Payload.StatusEffect.Duration < 0)
			{
				return FWBActionQueryResult::Deny(TEXT("negative_status_effect_duration"));
			}

			if (WBStatusEffect::DoesOperationRequireStatusId(Payload.StatusEffect.Operation)
				&& WBStatusEffect::CanonicalizeStatusId(Payload.StatusEffect.StatusId).IsNone())
			{
				return FWBActionQueryResult::Deny(TEXT("missing_status_id"));
			}
			break;
		}
		case EWBGenericEffectOp::DamageEffect:
		{
			const int32 RequestTargetUnitId = Request.Target.TargetUnitId;
			if (RequestTargetUnitId == -1)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (Payload.DamageEffect.TargetUnitId != -1
				&& Payload.DamageEffect.TargetUnitId != RequestTargetUnitId)
			{
				return FWBActionQueryResult::Deny(TEXT("effect_payload_target_mismatch"));
			}

			const FWBUnitState* TargetUnit = State.GetUnitById(RequestTargetUnitId);
			if (TargetUnit == nullptr)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (TargetUnit->bDefeated || !TargetUnit->IsUnitOnBoard())
			{
				return FWBActionQueryResult::Deny(TEXT("effect_target_unit_removed"));
			}

			if (Payload.DamageEffect.Amount < 0)
			{
				return FWBActionQueryResult::Deny(TEXT("negative_damage_effect_amount"));
			}
			break;
		}
		case EWBGenericEffectOp::HealEffect:
		{
			const int32 RequestTargetUnitId = Request.Target.TargetUnitId;
			if (RequestTargetUnitId == -1)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (Payload.HealEffect.TargetUnitId != -1
				&& Payload.HealEffect.TargetUnitId != RequestTargetUnitId)
			{
				return FWBActionQueryResult::Deny(TEXT("effect_payload_target_mismatch"));
			}

			const FWBUnitState* TargetUnit = State.GetUnitById(RequestTargetUnitId);
			if (TargetUnit == nullptr)
			{
				return FWBActionQueryResult::Deny(TEXT("missing_effect_target_unit"));
			}

			if (TargetUnit->bDefeated || !TargetUnit->IsUnitOnBoard())
			{
				return FWBActionQueryResult::Deny(TEXT("effect_target_unit_removed"));
			}

			if (Payload.HealEffect.Amount < 0)
			{
				return FWBActionQueryResult::Deny(TEXT("negative_heal_effect_amount"));
			}
			break;
		}
		default:
			return FWBActionQueryResult::Deny(TEXT("unknown_effect_payload_operation"));
		}
	}

	return FWBActionQueryResult::Ok();
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
	if (Unit == nullptr || Unit->bDefeated || !Unit->IsUnitOnBoard())
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
			if (Unit.OwnerId != PlayerId || Unit.bDefeated || !Unit.IsUnitOnBoard())
			{
				continue;
			}

			LegalActions.Append(GenerateLegalMoveActions(State, PlayerId, Unit.UnitId));
		}

		LegalActions.Append(GenerateLegalAttackActions(State, PlayerId));

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
