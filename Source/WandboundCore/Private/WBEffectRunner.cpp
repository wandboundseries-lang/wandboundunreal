#include "WBEffectRunner.h"

#include "WBActionCodec.h"
#include "WBArmorEffect.h"
#include "WBDamageResolution.h"
#include "WBDeathResolution.h"
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
	const FWBDamageResolutionResult& DamageResult,
	const int32 PreviousMaxHP,
	const int32 NewMaxHP,
	const int32 PreviousStatusTurns,
	const int32 NewStatusTurns)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_tick"));
	Event.StatusId = BurnStatusId;
	Event.PlayerId = PlayerId;
	Event.TargetUnitId = DamageResult.Request.TargetUnitId;
	Event.DamageAmount = DamageResult.Request.BaseDamage;
	Event.bDamagePrevented = DamageResult.Prevention.bPrevented;
	Event.PreventedDamageAmount = DamageResult.Prevention.PreventedAmount;
	Event.FinalDamageAmount = DamageResult.Prevention.FinalDamage;
	Event.PreventionReason = DamageResult.Prevention.PreventionReason;
	Event.PreviousHP = DamageResult.PreviousHP;
	Event.NewHP = DamageResult.NewHP;
	Event.PreviousArmor = DamageResult.PreviousArmor;
	Event.NewArmor = DamageResult.NewArmor;
	Event.ArmorAbsorbedAmount = DamageResult.ArmorAbsorbedAmount;
	Event.bBypassedArmor = DamageResult.bBypassedArmor;
	Event.HPDamageAmount = DamageResult.HPDamageAmount;
	Event.DamageCause = DamageResult.Request.DamageCause;
	Event.PreviousMaxHP = PreviousMaxHP;
	Event.NewMaxHP = NewMaxHP;
	Event.PreviousStatusTurns = PreviousStatusTurns;
	Event.NewStatusTurns = NewStatusTurns;
	Event.bAtOrBelowZeroHP = DamageResult.NewHP <= 0;
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

void AppendStatusRemovedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const int32 PlayerId,
	const FName StatusId,
	const int32 TargetUnitId,
	const int32 SourceUnitId,
	const FWBTile& FromTile,
	const FWBTile& ToTile,
	const int32 PreviousHP,
	const int32 NewHP)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_removed"));
	Event.StatusId = StatusId;
	Event.PlayerId = PlayerId;
	Event.SourceUnitId = SourceUnitId;
	Event.TargetUnitId = TargetUnitId;
	Event.FromTile = FromTile;
	Event.ToTile = ToTile;
	Event.DamageAmount = 0;
	Event.PreviousHP = PreviousHP;
	Event.NewHP = NewHP;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendAttackDamageResolvedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBPendingAttackState& PendingAttack,
	const int32 DamageAmount,
	const FWBDamageResolutionResult& DamageResult)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("attack_damage_resolved"));
	Event.PlayerId = PendingAttack.AttackingPlayerId;
	Event.SourceUnitId = PendingAttack.AttackerUnitId;
	Event.TargetUnitId = PendingAttack.DefenderUnitId;
	Event.FromTile = PendingAttack.AttackerTile;
	Event.ToTile = PendingAttack.DefenderTile;
	Event.ActionId = PendingAttack.DeclarationActionId;
	Event.DamageAmount = DamageAmount;
	Event.bDamagePrevented = DamageResult.Prevention.bPrevented;
	Event.PreventedDamageAmount = DamageResult.Prevention.PreventedAmount;
	Event.FinalDamageAmount = DamageResult.Prevention.FinalDamage;
	Event.PreventionReason = DamageResult.Prevention.PreventionReason;
	Event.PreviousHP = DamageResult.PreviousHP;
	Event.NewHP = DamageResult.NewHP;
	Event.PreviousArmor = DamageResult.PreviousArmor;
	Event.NewArmor = DamageResult.NewArmor;
	Event.ArmorAbsorbedAmount = DamageResult.ArmorAbsorbedAmount;
	Event.bBypassedArmor = DamageResult.bBypassedArmor;
	Event.HPDamageAmount = DamageResult.HPDamageAmount;
	Event.DamageCause = DamageResult.Request.DamageCause;
	Event.bAtOrBelowZeroHP = DamageResult.NewHP <= 0;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendUnitDefeatedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBUnitState& Unit,
	const int32 PreviousHP)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("unit_defeated"));
	Event.PlayerId = Unit.OwnerId;
	Event.TargetUnitId = Unit.UnitId;
	Event.PreviousHP = PreviousHP;
	Event.NewHP = Unit.HP;
	Event.bAtOrBelowZeroHP = true;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendUnitRemovedFromBoardTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBUnitState& Unit,
	const FWBTile& PreviousTile)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("unit_removed_from_board"));
	Event.PlayerId = Unit.OwnerId;
	Event.TargetUnitId = Unit.UnitId;
	Event.FromTile = PreviousTile;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendArmorModifiedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBArmorEffectResult& ArmorResult,
	const FWBUnitState* Target)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("armor_modified"));
	Event.TargetUnitId = ArmorResult.Request.TargetUnitId;
	Event.PlayerId = Target != nullptr ? Target->OwnerId : -1;
	Event.PreviousArmor = ArmorResult.PreviousCurrentArmor;
	Event.NewArmor = ArmorResult.NewCurrentArmor;
	Event.PreviousMaxArmor = ArmorResult.PreviousMaxArmor;
	Event.NewMaxArmor = ArmorResult.NewMaxArmor;
	Event.ArmorEffectOperation = WBArmorEffect::GetOperationName(ArmorResult.Request.Operation);
	Event.ArmorEffectAmount = ArmorResult.Request.Amount;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendHeroDefeatedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBUnitState& Unit,
	const int32 WinningPlayerId)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("hero_defeated"));
	Event.PlayerId = Unit.OwnerId;
	Event.TargetUnitId = Unit.UnitId;
	Event.WinningPlayerId = WinningPlayerId;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

bool IsHeroUnitForOwner(const FWBGameStateData& State, const FWBUnitState& Unit)
{
	const FWBPlayerStateData* Player = State.GetPlayerById(Unit.OwnerId);
	return Player != nullptr && Player->HeroUnitId == Unit.UnitId;
}

FWBDeathResolutionCandidate MakeDeathResolutionCandidate(
	const FWBGameStateData& State,
	const FWBUnitState& Unit)
{
	FWBDeathResolutionCandidate Candidate;
	Candidate.UnitId = Unit.UnitId;
	Candidate.OwnerId = Unit.OwnerId;
	Candidate.bIsHero = IsHeroUnitForOwner(State, Unit);
	return Candidate;
}

int32 OpposingPlayerId(const int32 PlayerId)
{
	if (PlayerId == 0)
	{
		return 1;
	}

	if (PlayerId == 1)
	{
		return 0;
	}

	return -1;
}

bool UnitTilePointerLess(const FWBUnitState& A, const FWBUnitState& B)
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
	case EWBActionType::Attack:
		return ApplyAttackDeclare(State, Action);
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

FWBApplyActionResult WBEffectRunner::ApplyAttackDeclare(FWBGameStateData& State, const FWBAction& Action)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult AttackQuery = WBRules::CanDeclareAttack(State, Action);
	if (!AttackQuery.bOk)
	{
		Result.bOk = false;
		Result.Reason = AttackQuery.Reason;
		return Result;
	}

	FWBUnitState* Attacker = State.GetMutableUnitById(Action.SourceUnitId);
	const FWBUnitState* Defender = State.GetUnitById(Action.TargetUnitId);
	if (Attacker == nullptr || Defender == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("unit_disappeared_before_attack_declaration");
		return Result;
	}

	const int32 AttacksLeftBefore = Attacker->AttacksLeft;
	Attacker->AttacksLeft = FMath::Max(Attacker->AttacksLeft - 1, 0);

	FWBTraceEvent AttackEvent;
	AttackEvent.Kind = FName(TEXT("attack_declared"));
	AttackEvent.PlayerId = Action.PlayerId;
	AttackEvent.SourceUnitId = Attacker->UnitId;
	AttackEvent.TargetUnitId = Defender->UnitId;
	AttackEvent.FromTile = FWBTile(Attacker->X, Attacker->Y);
	AttackEvent.ToTile = FWBTile(Defender->X, Defender->Y);
	AttackEvent.AttacksLeftBefore = AttacksLeftBefore;
	AttackEvent.AttacksLeftAfter = Attacker->AttacksLeft;
	AttackEvent.bOk = true;

	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = true;
	PendingAttack.AttackerUnitId = Attacker->UnitId;
	PendingAttack.DefenderUnitId = Defender->UnitId;
	PendingAttack.AttackingPlayerId = Action.PlayerId;
	PendingAttack.AttackerTile = FWBTile(Attacker->X, Attacker->Y);
	PendingAttack.DefenderTile = FWBTile(Defender->X, Defender->Y);
	PendingAttack.DeclarationActionId = WBActionCodec::MakeActionId(Action);
	State.PendingAttack = PendingAttack;

	Result.bOk = true;
	Result.TraceEvents.Add(AttackEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyPendingAttackDamage(FWBGameStateData& State)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult DamageQuery = WBRules::CanResolvePendingAttackDamage(State);
	if (!DamageQuery.bOk)
	{
		Result.bOk = false;
		Result.Reason = DamageQuery.Reason;
		return Result;
	}

	const FWBPendingAttackState PendingAttack = State.PendingAttack;
	const FWBUnitState* Attacker = State.GetUnitById(PendingAttack.AttackerUnitId);
	FWBUnitState* Defender = State.GetMutableUnitById(PendingAttack.DefenderUnitId);
	if (Attacker == nullptr || Defender == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("unit_disappeared_before_attack_damage");
		return Result;
	}

	const int32 PreviousHP = Defender->HP;
	if (Defender->HasStatus(FrozenStatusId))
	{
		Defender->RemoveStatus(FrozenStatusId);
		State.ClearPendingAttack();

		Result.bOk = true;
		AppendStatusRemovedTrace(
			Result.TraceEvents,
			PendingAttack.AttackingPlayerId,
			FrozenStatusId,
			PendingAttack.DefenderUnitId,
			PendingAttack.AttackerUnitId,
			PendingAttack.AttackerTile,
			PendingAttack.DefenderTile,
			PreviousHP,
			Defender->HP);
		FWBDamageResolutionResult FrozenDamageResult;
		FrozenDamageResult.bOk = true;
		FrozenDamageResult.Request.DamageKind = EWBDamageKind::Attack;
		FrozenDamageResult.Request.SourceUnitId = PendingAttack.AttackerUnitId;
		FrozenDamageResult.Request.TargetUnitId = PendingAttack.DefenderUnitId;
		FrozenDamageResult.Request.SourcePlayerId = PendingAttack.AttackingPlayerId;
		FrozenDamageResult.Request.BaseDamage = 0;
		FrozenDamageResult.Request.bBypassArmor = false;
		FrozenDamageResult.Request.DamageCause = FName(TEXT("Attack"));
		FrozenDamageResult.Prevention.bPrevented = false;
		FrozenDamageResult.Prevention.PreventedAmount = 0;
		FrozenDamageResult.Prevention.FinalDamage = 0;
		FrozenDamageResult.Prevention.PreventionReason = NAME_None;
		FrozenDamageResult.PreviousHP = PreviousHP;
		FrozenDamageResult.NewHP = Defender->HP;
		FrozenDamageResult.PreviousArmor = Defender->GetCurrentArmor();
		FrozenDamageResult.NewArmor = Defender->GetCurrentArmor();
		FrozenDamageResult.ArmorAbsorbedAmount = 0;
		FrozenDamageResult.bBypassedArmor = false;
		FrozenDamageResult.HPDamageAmount = 0;
		FrozenDamageResult.bAtOrBelowZeroHP = Defender->HP <= 0;
		AppendAttackDamageResolvedTrace(Result.TraceEvents, PendingAttack, 0, FrozenDamageResult);
		if (Defender->HP <= 0)
		{
			FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(State);
			Result.TraceEvents.Append(CleanupResult.TraceEvents);
			if (!CleanupResult.bOk && CleanupResult.Reason != TEXT("no_zero_hp_units"))
			{
				Result.bOk = false;
				Result.Reason = CleanupResult.Reason;
			}
		}
		return Result;
	}

	FWBDamageRequest DamageRequest;
	DamageRequest.DamageKind = EWBDamageKind::Attack;
	DamageRequest.SourceUnitId = PendingAttack.AttackerUnitId;
	DamageRequest.TargetUnitId = PendingAttack.DefenderUnitId;
	DamageRequest.SourcePlayerId = PendingAttack.AttackingPlayerId;
	DamageRequest.BaseDamage = FMath::Max(Attacker->ATK, 0);
	DamageRequest.bBypassArmor = false;
	DamageRequest.DamageCause = FName(TEXT("Attack"));

	const FWBDamageResolutionResult DamageResult = WBDamageResolution::ResolveDamageRequest(State, DamageRequest);
	if (!DamageResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = DamageResult.Reason;
		return Result;
	}

	const int32 DamageAmount = FMath::Max(DamageRequest.BaseDamage, 0);
	const int32 NewHP = DamageResult.NewHP;
	State.ClearPendingAttack();

	Result.bOk = true;
	AppendAttackDamageResolvedTrace(
		Result.TraceEvents,
		PendingAttack,
		DamageAmount,
		DamageResult);
	if (NewHP <= 0)
	{
		FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(State);
		Result.TraceEvents.Append(CleanupResult.TraceEvents);
		if (!CleanupResult.bOk && CleanupResult.Reason != TEXT("no_zero_hp_units"))
		{
			Result.bOk = false;
			Result.Reason = CleanupResult.Reason;
		}
	}
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyZeroHPDeathRemoval(FWBGameStateData& State)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult CleanupQuery = WBRules::CanApplyZeroHPDeathRemoval(State);
	if (!CleanupQuery.bOk)
	{
		Result.bOk = false;
		Result.Reason = CleanupQuery.Reason;
		return Result;
	}

	TArray<FWBUnitState*> UnitsToRemove;
	for (FWBUnitState& Unit : State.Units)
	{
		if (WBRules::ShouldUnitBeDefeatedAtZeroHP(State, Unit))
		{
			UnitsToRemove.Add(&Unit);
		}
	}

	UnitsToRemove.Sort([](const FWBUnitState& A, const FWBUnitState& B)
	{
		return UnitTilePointerLess(A, B);
	});

	for (FWBUnitState* Unit : UnitsToRemove)
	{
		if (Unit == nullptr)
		{
			continue;
		}

		const int32 PreviousHP = Unit->HP;
		const FWBTile PreviousTile(Unit->X, Unit->Y);
		const bool bHeroUnit = IsHeroUnitForOwner(State, *Unit);
		const FWBDeathPreventionResult DeathPrevention = WBDeathResolution::EvaluateDeathPrevention(
			State,
			MakeDeathResolutionCandidate(State, *Unit));
		if (DeathPrevention.bPrevented)
		{
			continue;
		}

		Unit->MarkUnitDefeated();
		Unit->RemoveUnitFromBoard();

		AppendUnitDefeatedTrace(Result.TraceEvents, *Unit, PreviousHP);
		AppendUnitRemovedFromBoardTrace(Result.TraceEvents, *Unit, PreviousTile);

		if (bHeroUnit && !State.bGameOver)
		{
			const int32 WinningPlayerId = OpposingPlayerId(Unit->OwnerId);
			State.bGameOver = true;
			State.WinnerPlayerId = WinningPlayerId;
			AppendHeroDefeatedTrace(Result.TraceEvents, *Unit, WinningPlayerId);
		}
	}

	Result.bOk = true;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyArmorEffect(
	FWBGameStateData& State,
	const FWBArmorEffectRequest& Request)
{
	FWBApplyActionResult Result;
	const FWBArmorEffectResult ArmorResult = WBArmorEffect::ApplyArmorEffect(State, Request);
	if (!ArmorResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = ArmorResult.Reason;
		return Result;
	}

	Result.bOk = true;
	AppendArmorModifiedTrace(Result.TraceEvents, ArmorResult, State.GetUnitById(Request.TargetUnitId));
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
		if (!Unit.IsUnitOnBoard())
		{
			continue;
		}

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
		if (!Unit.IsUnitOnBoard())
		{
			continue;
		}

		if (Unit.HasStatus(BurnStatusId))
		{
			const int32 PreviousMaxHP = Unit.MaxHP;
			int32 PreviousStatusTurns = 0;
			int32 NewStatusTurns = 0;
			const bool bExpired = DecayTimedStatus(Unit, BurnStatusId, PreviousStatusTurns, NewStatusTurns);

			FWBDamageRequest BurnDamageRequest;
			BurnDamageRequest.DamageKind = EWBDamageKind::Burn;
			BurnDamageRequest.SourceUnitId = -1;
			BurnDamageRequest.TargetUnitId = Unit.UnitId;
			BurnDamageRequest.SourcePlayerId = PlayerId;
			BurnDamageRequest.BaseDamage = 1;
			BurnDamageRequest.bBypassArmor = true;
			BurnDamageRequest.DamageCause = FName(TEXT("Burn"));
			const FWBDamageResolutionResult BurnDamageResult = WBDamageResolution::ResolveDamageRequest(State, BurnDamageRequest);
			if (!BurnDamageResult.bOk)
			{
				Result.bOk = false;
				Result.Reason = BurnDamageResult.Reason;
				return Result;
			}

			AppendBurnTickTrace(
				Result.TraceEvents,
				PlayerId,
				BurnDamageResult,
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
