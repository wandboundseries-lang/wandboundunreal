#include "WBEffectRunner.h"

#include "WBActionCodec.h"
#include "WBArmorEffect.h"
#include "WBCardActivationCommand.h"
#include "WBCardActivationCostPayment.h"
#include "WBDamageEffect.h"
#include "WBDamageResolution.h"
#include "WBDeathResolution.h"
#include "WBHealEffect.h"
#include "WBStatusEffect.h"
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

void AppendStatusModifiedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBStatusEffectResult& StatusResult,
	const FWBUnitState* Target)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_modified"));
	Event.TargetUnitId = StatusResult.Request.TargetUnitId;
	Event.PlayerId = Target != nullptr ? Target->OwnerId : -1;
	Event.StatusId = StatusResult.Request.StatusId;
	Event.StatusEffectOperation = WBStatusEffect::GetOperationName(StatusResult.Request.Operation);
	Event.PreviousStatusTurns = StatusResult.PreviousDuration;
	Event.NewStatusTurns = StatusResult.NewDuration;
	Event.RemovedStatuses = StatusResult.RemovedStatuses;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendStatusEffectRemovedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBStatusEffectResult& StatusResult,
	const FName StatusId,
	const FWBUnitState* Target)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("status_removed"));
	Event.StatusId = StatusId;
	Event.TargetUnitId = StatusResult.Request.TargetUnitId;
	Event.PlayerId = Target != nullptr ? Target->OwnerId : -1;
	Event.StatusEffectOperation = WBStatusEffect::GetOperationName(StatusResult.Request.Operation);
	Event.PreviousStatusTurns = StatusId == StatusResult.Request.StatusId ? StatusResult.PreviousDuration : -1;
	Event.NewStatusTurns = 0;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendDamageEffectResolvedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBDamageEffectResult& DamageResult,
	const FWBUnitState* Target)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("damage_effect_resolved"));
	Event.TargetUnitId = DamageResult.Request.TargetUnitId;
	Event.SourceUnitId = DamageResult.Request.SourceUnitId;
	Event.PlayerId = FWBGameStateData::IsValidPlayerId(DamageResult.Request.SourcePlayerId)
		? DamageResult.Request.SourcePlayerId
		: (Target != nullptr ? Target->OwnerId : -1);
	Event.DamageAmount = DamageResult.Request.Amount;
	Event.PreviousHP = DamageResult.PreviousHP;
	Event.NewHP = DamageResult.NewHP;
	Event.PreviousArmor = DamageResult.PreviousArmor;
	Event.NewArmor = DamageResult.NewArmor;
	Event.ArmorAbsorbedAmount = DamageResult.ArmorAbsorbedAmount;
	Event.bBypassedArmor = DamageResult.bBypassedArmor;
	Event.HPDamageAmount = DamageResult.HPDamageAmount;
	Event.DamageCause = DamageResult.Request.DamageCause.IsNone()
		? FName(TEXT("Effect"))
		: DamageResult.Request.DamageCause;
	Event.bAtOrBelowZeroHP = DamageResult.bAtOrBelowZeroHP;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendHealEffectResolvedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBHealEffectResult& HealResult,
	const FWBUnitState* Target)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("heal_effect_resolved"));
	Event.TargetUnitId = HealResult.Request.TargetUnitId;
	Event.SourceUnitId = HealResult.Request.SourceUnitId;
	Event.PlayerId = FWBGameStateData::IsValidPlayerId(HealResult.Request.SourcePlayerId)
		? HealResult.Request.SourcePlayerId
		: (Target != nullptr ? Target->OwnerId : -1);
	Event.PreviousHP = HealResult.PreviousHP;
	Event.NewHP = HealResult.NewHP;
	Event.HealAmount = HealResult.Request.Amount;
	Event.EffectiveHealAmount = HealResult.EffectiveHealAmount;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendEffectRequestResolvedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBEffectRequest& Request)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("effect_request_resolved"));
	Event.PlayerId = Request.Source.PlayerId;
	Event.SourceUnitId = Request.Source.SourceUnitId;
	Event.TargetUnitId = Request.Target.TargetUnitId;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendCardActivationResolvedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBCardActivationCommand& Command)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("card_activation_resolved"));
	Event.PlayerId = Command.Source.PlayerId;
	Event.SourceUnitId = Command.Source.SourceUnitId;
	Event.TargetUnitId = Command.EffectRequest.Target.TargetUnitId;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendCardActivationUsageMarkedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const int32 PlayerId)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("card_activation_usage_marked"));
	Event.PlayerId = PlayerId;
	Event.bOk = true;
	TraceEvents.Add(Event);
}

void AppendCardActivationCostPaidTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBCardActivationCostPaymentResult& PaymentResult)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("card_activation_cost_paid"));
	Event.PlayerId = PaymentResult.Request.PlayerId;
	Event.SourceUnitId = PaymentResult.Request.SourceUnitId;
	Event.CostAmount = PaymentResult.Request.RequiredRR;
	Event.PreviousRLUsed = PaymentResult.PreviousRLUsed;
	Event.NewRLUsed = PaymentResult.NewRLUsed;
	Event.AvailableRLBefore = PaymentResult.AvailableRLBefore;
	Event.AvailableRLAfter = PaymentResult.AvailableRLAfter;
	Event.CostKind = PaymentResult.Request.CostKind;
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
	return WBDeathResolution::ApplyZeroHPDeathResolution(State);
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

FWBApplyActionResult WBEffectRunner::ApplyStatusEffect(
	FWBGameStateData& State,
	const FWBStatusEffectRequest& Request)
{
	FWBApplyActionResult Result;
	const FWBStatusEffectResult StatusResult = WBStatusEffect::ApplyStatusEffect(State, Request);
	if (!StatusResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = StatusResult.Reason;
		return Result;
	}

	Result.bOk = true;
	const FWBUnitState* Target = State.GetUnitById(StatusResult.Request.TargetUnitId);
	AppendStatusModifiedTrace(Result.TraceEvents, StatusResult, Target);
	for (const FName& RemovedStatus : StatusResult.RemovedStatuses)
	{
		AppendStatusEffectRemovedTrace(Result.TraceEvents, StatusResult, RemovedStatus, Target);
	}
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyDamageEffect(
	FWBGameStateData& State,
	const FWBDamageEffectRequest& Request)
{
	FWBApplyActionResult Result;
	const FWBDamageEffectResult DamageResult = WBDamageEffect::ApplyDamageEffect(State, Request);
	if (!DamageResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = DamageResult.Reason;
		return Result;
	}

	Result.bOk = true;
	AppendDamageEffectResolvedTrace(Result.TraceEvents, DamageResult, State.GetUnitById(Request.TargetUnitId));
	if (DamageResult.NewHP <= 0)
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

FWBApplyActionResult WBEffectRunner::ApplyHealEffect(
	FWBGameStateData& State,
	const FWBHealEffectRequest& Request)
{
	FWBApplyActionResult Result;
	const FWBHealEffectResult HealResult = WBHealEffect::ApplyHealEffect(State, Request);
	if (!HealResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = HealResult.Reason;
		return Result;
	}

	Result.bOk = true;
	AppendHealEffectResolvedTrace(Result.TraceEvents, HealResult, State.GetUnitById(Request.TargetUnitId));
	return Result;
}

FWBEffectRequestResult WBEffectRunner::ApplyEffectRequest(
	FWBGameStateData& State,
	const FWBEffectRequest& Request)
{
	FWBEffectRequestResult Result;
	Result.Request = Request;

	const FWBActionQueryResult Query = WBRules::CanApplyEffectRequest(State, Request);
	if (!Query.bOk)
	{
		Result.bOk = false;
		Result.Reason = Query.Reason;
		return Result;
	}

	FWBGameStateData WorkingState = State;
	TArray<FWBApplyActionResult> PayloadResults;
	TArray<FWBTraceEvent> WorkingTraceEvents;
	AppendEffectRequestResolvedTrace(WorkingTraceEvents, Request);

	for (const FWBGenericEffectPayload& Payload : Request.Payloads)
	{
		FWBApplyActionResult PayloadResult;
		switch (Payload.Operation)
		{
		case EWBGenericEffectOp::ArmorEffect:
		{
			FWBArmorEffectRequest ArmorRequest = Payload.ArmorEffect;
			if (ArmorRequest.TargetUnitId == -1)
			{
				ArmorRequest.TargetUnitId = Request.Target.TargetUnitId;
			}

			PayloadResult = ApplyArmorEffect(WorkingState, ArmorRequest);
			break;
		}
		case EWBGenericEffectOp::StatusEffect:
		{
			FWBStatusEffectRequest StatusRequest = Payload.StatusEffect;
			if (StatusRequest.TargetUnitId == -1)
			{
				StatusRequest.TargetUnitId = Request.Target.TargetUnitId;
			}

			PayloadResult = ApplyStatusEffect(WorkingState, StatusRequest);
			break;
		}
		case EWBGenericEffectOp::DamageEffect:
		{
			FWBDamageEffectRequest DamageRequest = Payload.DamageEffect;
			if (DamageRequest.TargetUnitId == -1)
			{
				DamageRequest.TargetUnitId = Request.Target.TargetUnitId;
			}
			if (DamageRequest.SourceUnitId == -1)
			{
				DamageRequest.SourceUnitId = Request.Source.SourceUnitId;
			}
			if (DamageRequest.SourcePlayerId == -1)
			{
				DamageRequest.SourcePlayerId = Request.Source.PlayerId;
			}

			PayloadResult = ApplyDamageEffect(WorkingState, DamageRequest);
			break;
		}
		case EWBGenericEffectOp::HealEffect:
		{
			FWBHealEffectRequest HealRequest = Payload.HealEffect;
			if (HealRequest.TargetUnitId == -1)
			{
				HealRequest.TargetUnitId = Request.Target.TargetUnitId;
			}
			if (HealRequest.SourceUnitId == -1)
			{
				HealRequest.SourceUnitId = Request.Source.SourceUnitId;
			}
			if (HealRequest.SourcePlayerId == -1)
			{
				HealRequest.SourcePlayerId = Request.Source.PlayerId;
			}

			PayloadResult = ApplyHealEffect(WorkingState, HealRequest);
			break;
		}
		default:
			Result.bOk = false;
			Result.Reason = TEXT("unknown_effect_payload_operation");
			return Result;
		}

		if (!PayloadResult.bOk)
		{
			Result.bOk = false;
			Result.Reason = PayloadResult.Reason;
			return Result;
		}

		WorkingTraceEvents.Append(PayloadResult.TraceEvents);
		PayloadResults.Add(MoveTemp(PayloadResult));
	}

	State = WorkingState;
	Result.bOk = true;
	Result.PayloadResults = MoveTemp(PayloadResults);
	Result.TraceEvents = MoveTemp(WorkingTraceEvents);
	return Result;
}

FWBCardActivationCommandResult WBEffectRunner::ApplyCardActivationCommand(
	FWBGameStateData& State,
	const FWBCardActivationCommand& Command)
{
	FWBCardActivationCommandResult Result;
	Result.Command = Command;

	const FWBActionQueryResult Query = WBRules::CanApplyCardActivationCommand(State, Command);
	if (!Query.bOk)
	{
		Result.bOk = false;
		Result.Reason = Query.Reason;
		return Result;
	}

	FWBCardActivationCommand FilledCommand = Command;
	if (FilledCommand.EffectRequest.Source.PlayerId == -1)
	{
		FilledCommand.EffectRequest.Source.PlayerId = FilledCommand.Source.PlayerId;
	}
	if (FilledCommand.EffectRequest.Source.SourceUnitId == -1)
	{
		FilledCommand.EffectRequest.Source.SourceUnitId = FilledCommand.Source.SourceUnitId;
	}
	if (FilledCommand.EffectRequest.Source.SourceCardId.IsEmpty())
	{
		FilledCommand.EffectRequest.Source.SourceCardId = FilledCommand.Source.SourceCardId;
	}
	if (FilledCommand.EffectRequest.Source.SourceEffectId.IsEmpty())
	{
		FilledCommand.EffectRequest.Source.SourceEffectId = FilledCommand.Source.SourceEffectId;
	}

	FWBGameStateData WorkingState = State;
	FWBCardActivationCostPaymentResult PaymentResult;
	bool bPaidCost = false;
	if (FilledCommand.CostPaymentCommit.bPayCostOnSuccess)
	{
		FWBCardActivationCostPaymentRequest PaymentRequest;
		PaymentRequest.PlayerId = FilledCommand.CostPaymentCommit.PlayerId;
		PaymentRequest.SourceUnitId = FilledCommand.CostPaymentCommit.SourceUnitId;
		PaymentRequest.RequiredRR = FilledCommand.CostPaymentCommit.RequiredRR;
		PaymentRequest.CostKind = FilledCommand.CostPaymentCommit.CostKind;

		PaymentResult = WBCardActivationCostPayment::PayCost(WorkingState, PaymentRequest);
		if (!PaymentResult.bOk)
		{
			Result.bOk = false;
			Result.Reason = PaymentResult.Reason;
			Result.Command = FilledCommand;
			return Result;
		}

		bPaidCost = true;
	}

	const FWBEffectRequestResult EffectResult = ApplyEffectRequest(WorkingState, FilledCommand.EffectRequest);
	Result.Command = FilledCommand;
	Result.EffectResult = EffectResult;
	if (!EffectResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = EffectResult.Reason;
		return Result;
	}

	if (FilledCommand.UsageCommit.bMarkUsageOnSuccess)
	{
		if (!FWBGameStateData::IsValidPlayerId(FilledCommand.UsageCommit.PlayerId)
			|| FilledCommand.UsageCommit.UsageKey.IsEmpty())
		{
			Result.bOk = false;
			Result.Reason = TEXT("usage_commit_invalid");
			return Result;
		}

		if (WorkingState.HasActivationUsageKeyThisTurn(
			FilledCommand.UsageCommit.PlayerId,
			FilledCommand.UsageCommit.UsageKey))
		{
			Result.bOk = false;
			Result.Reason = TEXT("once_per_turn_already_used");
			return Result;
		}

		WorkingState.MarkActivationUsageKeyForTest(
			FilledCommand.UsageCommit.PlayerId,
			FilledCommand.UsageCommit.UsageKey);
	}

	State = WorkingState;
	Result.bOk = true;
	AppendCardActivationResolvedTrace(Result.TraceEvents, FilledCommand);
	if (bPaidCost)
	{
		AppendCardActivationCostPaidTrace(Result.TraceEvents, PaymentResult);
	}
	Result.TraceEvents.Append(EffectResult.TraceEvents);
	if (FilledCommand.UsageCommit.bMarkUsageOnSuccess)
	{
		AppendCardActivationUsageMarkedTrace(Result.TraceEvents, FilledCommand.UsageCommit.PlayerId);
	}
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

	const FWBActionQueryResult CleanupQuery = WBRules::CanApplyZeroHPDeathRemoval(State);
	if (CleanupQuery.bOk)
	{
		FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(State);
		Result.TraceEvents.Append(CleanupResult.TraceEvents);
		if (!CleanupResult.bOk)
		{
			Result.bOk = false;
			Result.Reason = CleanupResult.Reason;
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

	const FWBActionQueryResult CleanupQuery = WBRules::CanApplyZeroHPDeathRemoval(State);
	if (CleanupQuery.bOk)
	{
		FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(State);
		Result.TraceEvents.Append(CleanupResult.TraceEvents);
		if (!CleanupResult.bOk)
		{
			Result.bOk = false;
			Result.Reason = CleanupResult.Reason;
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
