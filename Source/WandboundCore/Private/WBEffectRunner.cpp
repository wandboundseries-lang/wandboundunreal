#include "WBEffectRunner.h"

#include "WBActionCodec.h"
#include "WBArmorEffect.h"
#include "WBCardActivationCommand.h"
#include "WBCardActivationCostPayment.h"
#include "WBCardDefinitionRepository.h"
#include "WBDamageEffect.h"
#include "WBDamageResolution.h"
#include "WBDeathResolution.h"
#include "WBHealEffect.h"
#include "WBMatchCoordinator.h"
#include "WBStatusEffect.h"
#include "WBRules.h"
#include "WBUnitReplacementEffect.h"

namespace
{
const FName BurnStatusId(TEXT("Burn"));
const FName PoisonStatusId(TEXT("Poison"));
const FName RootedStatusId(TEXT("Rooted"));
const FName EffectRunnerStunnedStatusId(TEXT("Stunned"));
const FName EffectRunnerFrozenStatusId(TEXT("Frozen"));

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
	Event.TargetUnitId = DamageResult.Request.TargetUnitId;
	Event.AttackDefenderUnitId = PendingAttack.DefenderUnitId;
	Event.HitUnitId = PendingAttack.DamageCalculation.bValid
		? PendingAttack.DamageCalculation.HitUnitId
		: PendingAttack.DefenderUnitId;
	Event.DamageRecipientUnitId = DamageResult.Request.TargetUnitId;
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
	Event.ActualHPDamageAmount = FMath::Max(
		DamageResult.PreviousHP - DamageResult.NewHP, 0);
	Event.bFrozenBreak = PendingAttack.DamageCalculation.bFrozenBreak;
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

void AppendPendingEffectNegationRequestedTrace(
	TArray<FWBTraceEvent>& TraceEvents,
	const FWBEffectRequest& Request,
	const FString& FrameId)
{
	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("pending_effect_negation_requested"));
	Event.PlayerId = Request.Source.PlayerId;
	Event.SourceUnitId = Request.Source.SourceUnitId;
	Event.PendingEffectFrameId = FrameId;
	Event.bOk = true;
	TraceEvents.Add(MoveTemp(Event));
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

FWBApplyActionResult WBEffectRunner::ApplyAction(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBAction& Action)
{
	if (Action.Type == EWBActionType::Attack)
	{
		return ApplyAttackDeclare(State, Repository, Action);
	}
	return ApplyAction(State, Action);
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

FWBApplyActionResult WBEffectRunner::ApplyNPCMove(FWBGameStateData& State, const FWBAction& Action)
{
	FWBApplyActionResult Result;
	const FWBUnitState* ExistingUnit = State.GetUnitById(Action.SourceUnitId);
	const int32 AvailableMP = ExistingUnit == nullptr ? 0 : ExistingUnit->MPRemaining;
	const FWBMoveQueryResult MoveQuery = WBRules::QueryNPCMove(State, Action, AvailableMP);
	if (!MoveQuery.bOk)
	{
		Result.Reason = MoveQuery.Reason;
		return Result;
	}

	FWBUnitState* Unit = State.GetMutableUnitById(Action.SourceUnitId);
	if (Unit == nullptr)
	{
		Result.Reason = TEXT("npc_disappeared_before_movement");
		return Result;
	}

	const FWBTile OriginalTile(Unit->X, Unit->Y);
	Unit->X = Action.ToTile.X;
	Unit->Y = Action.ToTile.Y;
	Unit->MPRemaining = FMath::Max(Unit->MPRemaining - MoveQuery.CostMP, 0);

	FWBTraceEvent MoveEvent;
	MoveEvent.Kind = FName(TEXT("npc_moved"));
	MoveEvent.PlayerId = -1;
	MoveEvent.SourceUnitId = Action.SourceUnitId;
	MoveEvent.FromTile = OriginalTile;
	MoveEvent.ToTile = Action.ToTile;
	MoveEvent.RemainingMP = Unit->MPRemaining;
	MoveEvent.bOk = true;
	Result.bOk = true;
	Result.TraceEvents.Add(MoveEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyAttackDeclare(FWBGameStateData& State, const FWBAction& Action)
{
	return ApplyAttackDeclare(State, FWBCardDefinitionRepository(), Action);
}

FWBApplyActionResult WBEffectRunner::ApplyAttackDeclare(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBAction& Action)
{
	FWBApplyActionResult Result;

	const FWBActionQueryResult AttackQuery = Repository.RepositoryId.IsEmpty()
		? WBRules::CanDeclareAttack(State, Action)
		: WBRules::CanDeclareAttack(State, Repository, Action);
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
	PendingAttack.AuthorityKind = EWBAttackAuthorityKind::Player;
	PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	PendingAttack.AttackerUnitId = Attacker->UnitId;
	PendingAttack.DefenderUnitId = Defender->UnitId;
	PendingAttack.OriginalAttackerUnitId = Attacker->UnitId;
	PendingAttack.OriginalDefenderUnitId = Defender->UnitId;
	PendingAttack.AttackingPlayerId = Action.PlayerId;
	PendingAttack.AttackerTile = FWBTile(Attacker->X, Attacker->Y);
	PendingAttack.DefenderTile = FWBTile(Defender->X, Defender->Y);
	PendingAttack.DeclarationActionId = WBActionCodec::MakeActionId(Action);
	PendingAttack.ContinuationId = FString::Printf(
		TEXT("attack_continuation:%s"),
		*PendingAttack.DeclarationActionId);
	State.PendingAttack = PendingAttack;

	Result.bOk = true;
	Result.TraceEvents.Add(AttackEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyNPCAttackDeclare(FWBGameStateData& State, const FWBAction& Action)
{
	return ApplyNPCAttackDeclare(State, FWBCardDefinitionRepository(), Action);
}

FWBApplyActionResult WBEffectRunner::ApplyNPCAttackDeclare(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBAction& Action)
{
	FWBApplyActionResult Result;
	const FWBActionQueryResult AttackQuery = Repository.RepositoryId.IsEmpty()
		? WBRules::CanDeclareNPCAttack(State, Action)
		: WBRules::CanDeclareNPCAttack(State, Repository, Action);
	if (!AttackQuery.bOk)
	{
		Result.Reason = AttackQuery.Reason;
		return Result;
	}

	FWBUnitState* Attacker = State.GetMutableUnitById(Action.SourceUnitId);
	const FWBUnitState* Defender = State.GetUnitById(Action.TargetUnitId);
	if (Attacker == nullptr || Defender == nullptr)
	{
		Result.Reason = TEXT("unit_disappeared_before_npc_attack_declaration");
		return Result;
	}

	const int32 AttacksLeftBefore = Attacker->AttacksLeft;
	Attacker->AttacksLeft = FMath::Max(Attacker->AttacksLeft - 1, 0);

	FWBTraceEvent AttackEvent;
	AttackEvent.Kind = FName(TEXT("npc_attack_declared"));
	AttackEvent.PlayerId = -1;
	AttackEvent.SourceUnitId = Attacker->UnitId;
	AttackEvent.TargetUnitId = Defender->UnitId;
	AttackEvent.FromTile = FWBTile(Attacker->X, Attacker->Y);
	AttackEvent.ToTile = FWBTile(Defender->X, Defender->Y);
	AttackEvent.AttacksLeftBefore = AttacksLeftBefore;
	AttackEvent.AttacksLeftAfter = Attacker->AttacksLeft;
	AttackEvent.bOk = true;

	FWBPendingAttackState PendingAttack;
	PendingAttack.bActive = true;
	PendingAttack.AuthorityKind = EWBAttackAuthorityKind::NeutralNPC;
	PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	PendingAttack.AttackerUnitId = Attacker->UnitId;
	PendingAttack.DefenderUnitId = Defender->UnitId;
	PendingAttack.OriginalAttackerUnitId = Attacker->UnitId;
	PendingAttack.OriginalDefenderUnitId = Defender->UnitId;
	PendingAttack.AttackingPlayerId = -1;
	PendingAttack.AttackerTile = FWBTile(Attacker->X, Attacker->Y);
	PendingAttack.DefenderTile = FWBTile(Defender->X, Defender->Y);
	PendingAttack.DeclarationActionId = FString::Printf(
		TEXT("npc_attack:u%d:t%d"),
		Attacker->UnitId,
		Defender->UnitId);
	PendingAttack.ContinuationId = FString::Printf(
		TEXT("attack_continuation:%s"),
		*PendingAttack.DeclarationActionId);
	State.PendingAttack = PendingAttack;

	Result.bOk = true;
	Result.TraceEvents.Add(AttackEvent);
	return Result;
}

FWBApplyActionResult WBEffectRunner::CalculatePendingAttackDamage(
	FWBGameStateData& State)
{
	FWBApplyActionResult Result;
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage
			!= EWBAttackContinuationStage::CalculateDamage)
	{
		Result.Reason = TEXT("pending_attack_not_calculate_damage");
		return Result;
	}
	const FWBActionQueryResult Query = WBRules::CanResolvePendingAttackDamage(State);
	if (!Query.bOk)
	{
		Result.Reason = Query.Reason;
		return Result;
	}

	const FWBUnitState* Attacker =
		State.GetUnitById(State.PendingAttack.AttackerUnitId);
	const FWBUnitState* HitUnit =
		State.GetUnitById(State.PendingAttack.DefenderUnitId);
	if (Attacker == nullptr || HitUnit == nullptr)
	{
		Result.Reason = TEXT("unit_disappeared_before_attack_damage");
		return Result;
	}

	FWBPendingAttackState::FDamageCalculation Calculation;
	Calculation.bValid = true;
	Calculation.HitUnitId = HitUnit->UnitId;
	Calculation.RawAttackDamage = FMath::Max(
		Attacker->ATK + State.PendingAttack.RawDamageModifier, 0);
	Calculation.PreviousHP = HitUnit->HP;
	Calculation.PreviousArmor = HitUnit->GetCurrentArmor();
	Calculation.CalculatedArmor = Calculation.PreviousArmor;
	Calculation.bPrevented = State.PendingAttack.bPrevented;

	if (!Calculation.bPrevented && HitUnit->HasStatus(EffectRunnerFrozenStatusId))
	{
		Calculation.bFrozenBreak = true;
	}
	else if (!Calculation.bPrevented)
	{
		FWBDamageRequest Request;
		Request.DamageKind = EWBDamageKind::Attack;
		Request.SourceUnitId = State.PendingAttack.AttackerUnitId;
		Request.TargetUnitId = HitUnit->UnitId;
		Request.SourcePlayerId = State.PendingAttack.AttackingPlayerId;
		Request.BaseDamage = Calculation.RawAttackDamage;
		Request.DamageCause = FName(TEXT("Attack"));
		const FWBDamageResolutionResult Preview =
			WBDamageResolution::CalculateDamageRequest(State, Request);
		if (!Preview.bOk)
		{
			Result.Reason = Preview.Reason;
			return Result;
		}
		Calculation.CalculatedArmor = Preview.NewArmor;
		Calculation.ArmorAbsorbedAmount = Preview.ArmorAbsorbedAmount;
		Calculation.CalculatedHPDamage = Preview.HPDamageAmount;
	}

	State.PendingAttack.DamageCalculation = Calculation;
	State.PendingAttack.FinalDamageRecipientUnitId = Calculation.HitUnitId;
	State.PendingAttack.Stage = EWBAttackContinuationStage::SubstituteDamage;

	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("attack_damage_calculated"));
	Event.PlayerId = State.PendingAttack.AttackingPlayerId;
	Event.SourceUnitId = State.PendingAttack.AttackerUnitId;
	Event.TargetUnitId = Calculation.HitUnitId;
	Event.AttackDefenderUnitId = State.PendingAttack.DefenderUnitId;
	Event.HitUnitId = Calculation.HitUnitId;
	Event.DamageRecipientUnitId = Calculation.HitUnitId;
	Event.DamageAmount = Calculation.RawAttackDamage;
	Event.PreviousHP = Calculation.PreviousHP;
	Event.NewHP = FMath::Max(
		Calculation.PreviousHP - Calculation.CalculatedHPDamage, 0);
	Event.PreviousArmor = Calculation.PreviousArmor;
	Event.NewArmor = Calculation.CalculatedArmor;
	Event.ArmorAbsorbedAmount = Calculation.ArmorAbsorbedAmount;
	Event.HPDamageAmount = Calculation.CalculatedHPDamage;
	Event.ActualHPDamageAmount = 0;
	Event.bFrozenBreak = Calculation.bFrozenBreak;
	Event.bDamagePrevented = Calculation.bPrevented;
	Event.AttackContinuationId = State.PendingAttack.ContinuationId;
	Event.AttackContinuationStage = FName(TEXT("calculate_damage"));
	Event.bCounterAttack = State.PendingAttack.bCounter;
	Event.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Event));
	Result.bOk = true;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ResolvePendingAttackDamageSubstitution(
	FWBGameStateData& State)
{
	FWBApplyActionResult Result;
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage != EWBAttackContinuationStage::SubstituteDamage
		|| !State.PendingAttack.DamageCalculation.bValid)
	{
		Result.Reason = TEXT("pending_attack_not_substitute_damage");
		return Result;
	}

	const FWBPendingAttackState::FDamageCalculation& Calculation =
		State.PendingAttack.DamageCalculation;
	State.PendingAttack.FinalDamageRecipientUnitId = Calculation.HitUnitId;
	const FWBPendingAttackState::FDamageSubstitution& Substitution =
		State.PendingAttack.DamageSubstitution;
	if (Substitution.bActive
		&& Calculation.HitUnitId == Substitution.ProtectedUnitId
		&& Calculation.CalculatedHPDamage > 0)
	{
		const FWBUnitState* Substitute =
			State.GetUnitById(Substitution.SubstituteUnitId);
		if (Substitute != nullptr
			&& !Substitute->bDefeated
			&& Substitute->IsUnitOnBoard())
		{
			State.PendingAttack.FinalDamageRecipientUnitId =
				Substitution.SubstituteUnitId;
			FWBTraceEvent Event;
			Event.Kind = FName(TEXT("attack_damage_substituted"));
			Event.SourceUnitId = State.PendingAttack.AttackerUnitId;
			Event.PreviousTargetUnitId = Calculation.HitUnitId;
			Event.TargetUnitId = Substitution.SubstituteUnitId;
			Event.AttackDefenderUnitId = State.PendingAttack.DefenderUnitId;
			Event.HitUnitId = Calculation.HitUnitId;
			Event.DamageRecipientUnitId = Substitution.SubstituteUnitId;
			Event.HPDamageAmount = Calculation.CalculatedHPDamage;
			Event.AttackContinuationId = State.PendingAttack.ContinuationId;
			Event.AttackContinuationStage = FName(TEXT("substitute_damage"));
			Event.bCounterAttack = State.PendingAttack.bCounter;
			Event.bOk = true;
			Result.TraceEvents.Add(MoveTemp(Event));
		}
		else
		{
			FWBTraceEvent Event;
			Event.Kind = FName(TEXT("pending_attack_damage_substitution_fallback"));
			Event.SourceUnitId = State.PendingAttack.AttackerUnitId;
			Event.PreviousTargetUnitId = Substitution.SubstituteUnitId;
			Event.TargetUnitId = Calculation.HitUnitId;
			Event.HitUnitId = Calculation.HitUnitId;
			Event.DamageRecipientUnitId = Calculation.HitUnitId;
			Event.AttackContinuationId = State.PendingAttack.ContinuationId;
			Event.AttackContinuationStage = FName(TEXT("substitute_damage"));
			Event.bCounterAttack = State.PendingAttack.bCounter;
			Event.bOk = true;
			Result.TraceEvents.Add(MoveTemp(Event));
		}
	}
	State.PendingAttack.Stage = EWBAttackContinuationStage::ApplyDamage;
	Result.bOk = true;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyCalculatedPendingAttackDamage(
	FWBGameStateData& State,
	const bool bPreservePendingAttack)
{
	FWBApplyActionResult Result;
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage != EWBAttackContinuationStage::ApplyDamage
		|| !State.PendingAttack.DamageCalculation.bValid)
	{
		Result.Reason = TEXT("pending_attack_not_apply_damage");
		return Result;
	}

	const FWBPendingAttackState PendingAttack = State.PendingAttack;
	const FWBPendingAttackState::FDamageCalculation& Calculation =
		PendingAttack.DamageCalculation;
	FWBUnitState* HitUnit = State.GetMutableUnitById(Calculation.HitUnitId);
	if (HitUnit == nullptr || !HitUnit->IsUnitOnBoard())
	{
		Result.Reason = TEXT("damage_target_removed");
		return Result;
	}
	int32 FinalRecipientUnitId = PendingAttack.FinalDamageRecipientUnitId;
	if (FinalRecipientUnitId != Calculation.HitUnitId)
	{
		const FWBUnitState* Recipient = State.GetUnitById(FinalRecipientUnitId);
		if (Recipient == nullptr || Recipient->bDefeated || !Recipient->IsUnitOnBoard())
		{
			FWBTraceEvent Fallback;
			Fallback.Kind = FName(TEXT("pending_attack_damage_substitution_fallback"));
			Fallback.SourceUnitId = PendingAttack.AttackerUnitId;
			Fallback.PreviousTargetUnitId = FinalRecipientUnitId;
			Fallback.TargetUnitId = Calculation.HitUnitId;
			Fallback.HitUnitId = Calculation.HitUnitId;
			Fallback.DamageRecipientUnitId = Calculation.HitUnitId;
			Fallback.AttackContinuationId = PendingAttack.ContinuationId;
			Fallback.AttackContinuationStage = FName(TEXT("apply_damage"));
			Fallback.bCounterAttack = PendingAttack.bCounter;
			Fallback.bOk = true;
			Result.TraceEvents.Add(MoveTemp(Fallback));
			FinalRecipientUnitId = Calculation.HitUnitId;
			State.PendingAttack.FinalDamageRecipientUnitId = Calculation.HitUnitId;
		}
	}

	FWBDamageResolutionResult Applied;
	Applied.bOk = true;
	Applied.Request.DamageKind = EWBDamageKind::Attack;
	Applied.Request.SourceUnitId = PendingAttack.AttackerUnitId;
	Applied.Request.TargetUnitId = FinalRecipientUnitId;
	Applied.Request.SourcePlayerId = PendingAttack.AttackingPlayerId;
	Applied.Request.BaseDamage = Calculation.RawAttackDamage;
	Applied.Request.DamageCause = FName(TEXT("Attack"));
	Applied.Prevention.bPrevented = Calculation.bPrevented;
	Applied.Prevention.PreventedAmount = Calculation.bPrevented
		? Calculation.RawAttackDamage : 0;
	Applied.Prevention.FinalDamage = Calculation.bPrevented
		? 0 : Calculation.RawAttackDamage;
	if (Calculation.bFrozenBreak)
	{
		Applied.Prevention.FinalDamage = 0;
	}
	Applied.PreviousHP = HitUnit->HP;
	Applied.NewHP = HitUnit->HP;
	Applied.PreviousArmor = Calculation.PreviousArmor;
	Applied.NewArmor = Calculation.CalculatedArmor;
	Applied.ArmorAbsorbedAmount = Calculation.ArmorAbsorbedAmount;
	Applied.HPDamageAmount = Calculation.CalculatedHPDamage;

	if (Calculation.bFrozenBreak)
	{
		HitUnit->RemoveStatus(EffectRunnerFrozenStatusId);
		AppendStatusRemovedTrace(
			Result.TraceEvents,
			PendingAttack.AttackingPlayerId,
			EffectRunnerFrozenStatusId,
			Calculation.HitUnitId,
			PendingAttack.AttackerUnitId,
			PendingAttack.AttackerTile,
			FWBTile(HitUnit->X, HitUnit->Y),
			HitUnit->HP,
			HitUnit->HP);
	}
	else if (!Calculation.bPrevented)
	{
		FWBDamageResolutionResult Preview;
		Preview.bOk = true;
		Preview.Request = Applied.Request;
		Preview.Request.TargetUnitId = Calculation.HitUnitId;
		Preview.Prevention = Applied.Prevention;
		Preview.PreviousHP = Calculation.PreviousHP;
		Preview.NewHP = FMath::Max(
			Calculation.PreviousHP - Calculation.CalculatedHPDamage, 0);
		Preview.PreviousArmor = Calculation.PreviousArmor;
		Preview.NewArmor = Calculation.CalculatedArmor;
		Preview.ArmorAbsorbedAmount = Calculation.ArmorAbsorbedAmount;
		Preview.HPDamageAmount = Calculation.CalculatedHPDamage;
		Preview.bAtOrBelowZeroHP = Preview.NewHP <= 0;
		Applied = WBDamageResolution::ApplyCalculatedDamage(
			State, Preview, FinalRecipientUnitId);
		if (!Applied.bOk)
		{
			Result.Reason = Applied.Reason;
			return Result;
		}
	}

	if (bPreservePendingAttack)
	{
		State.PendingAttack.bDamageResolved = true;
		State.PendingAttack.bFrozenBroken = Calculation.bFrozenBreak;
		State.PendingAttack.Stage = EWBAttackContinuationStage::AfterDamage;
	}
	else
	{
		State.ClearPendingAttack();
	}

	FWBTraceEvent AppliedEvent;
	AppliedEvent.Kind = FName(TEXT("attack_damage_applied"));
	AppliedEvent.SourceUnitId = PendingAttack.AttackerUnitId;
	AppliedEvent.TargetUnitId = Applied.Request.TargetUnitId;
	AppliedEvent.AttackDefenderUnitId = PendingAttack.DefenderUnitId;
	AppliedEvent.HitUnitId = Calculation.HitUnitId;
	AppliedEvent.DamageRecipientUnitId = Applied.Request.TargetUnitId;
	AppliedEvent.DamageAmount = Calculation.RawAttackDamage;
	AppliedEvent.PreviousHP = Applied.PreviousHP;
	AppliedEvent.NewHP = Applied.NewHP;
	AppliedEvent.PreviousArmor = Calculation.PreviousArmor;
	AppliedEvent.NewArmor = Calculation.CalculatedArmor;
	AppliedEvent.ArmorAbsorbedAmount = Calculation.ArmorAbsorbedAmount;
	AppliedEvent.HPDamageAmount = Calculation.CalculatedHPDamage;
	AppliedEvent.ActualHPDamageAmount = FMath::Max(
		Applied.PreviousHP - Applied.NewHP, 0);
	AppliedEvent.bFrozenBreak = Calculation.bFrozenBreak;
	AppliedEvent.bDamagePrevented = Calculation.bPrevented;
	AppliedEvent.AttackContinuationId = PendingAttack.ContinuationId;
	AppliedEvent.AttackContinuationStage = FName(TEXT("apply_damage"));
	AppliedEvent.bCounterAttack = PendingAttack.bCounter;
	AppliedEvent.bOk = true;
	Result.TraceEvents.Add(MoveTemp(AppliedEvent));
	AppendAttackDamageResolvedTrace(
		Result.TraceEvents,
		PendingAttack,
		Calculation.bFrozenBreak ? 0 : Calculation.RawAttackDamage,
		Applied);

	if (Applied.NewHP <= 0 && !Calculation.bPrevented
		&& !Calculation.bFrozenBreak)
	{
		FWBApplyActionResult Cleanup = ApplyZeroHPDeathRemoval(
			State,
			EWBUnitDestructionCause::BattleDamage);
		Result.TraceEvents.Append(Cleanup.TraceEvents);
		if (!Cleanup.bOk && Cleanup.Reason != TEXT("no_zero_hp_units"))
		{
			Result.Reason = Cleanup.Reason;
			return Result;
		}
	}
	Result.bOk = true;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyPendingAttackDamage(
	FWBGameStateData& State,
	const bool bPreservePendingAttack)
{
	FWBApplyActionResult Result;
	const FWBActionQueryResult CompatibilityQuery =
		WBRules::CanResolvePendingAttackDamage(State);
	if (!CompatibilityQuery.bOk)
	{
		Result.Reason = CompatibilityQuery.Reason;
		return Result;
	}
	if (State.HasPendingAttack()
		&& (State.PendingAttack.Stage == EWBAttackContinuationStage::PreHit
			|| State.PendingAttack.Stage == EWBAttackContinuationStage::Damage
			|| State.PendingAttack.Stage == EWBAttackContinuationStage::None))
	{
		State.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
	}
	const FWBApplyActionResult Calculated = CalculatePendingAttackDamage(State);
	if (!Calculated.bOk)
	{
		Result.Reason = Calculated.Reason;
		return Result;
	}
	const FWBApplyActionResult Substituted =
		ResolvePendingAttackDamageSubstitution(State);
	Result.TraceEvents.Append(Substituted.TraceEvents);
	if (!Substituted.bOk)
	{
		Result.Reason = Substituted.Reason;
		return Result;
	}
	FWBApplyActionResult Applied =
		ApplyCalculatedPendingAttackDamage(State, bPreservePendingAttack);
	if (Applied.bOk
		&& bPreservePendingAttack
		&& State.HasPendingAttack()
		&& State.PendingAttack.Stage
			== EWBAttackContinuationStage::AfterDamage)
	{
		// Compatibility callers do not provide a definition repository. The
		// production coordinator owns automatic AfterDamage trigger resolution.
		State.PendingAttack.Stage = EWBAttackContinuationStage::PostHit;
	}
	for (const FWBTraceEvent& Event : Applied.TraceEvents)
	{
		if (Event.Kind != FName(TEXT("attack_damage_applied")))
		{
			Result.TraceEvents.Add(Event);
		}
	}
	Result.bOk = Applied.bOk;
	Result.Reason = Applied.Reason;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyPendingAttackRedirect(
	FWBGameStateData& State,
	const FString& PendingAttackContinuationId,
	const int32 NewTargetUnitId)
{
	return ApplyPendingAttackRedirect(
		State, FWBCardDefinitionRepository(),
		PendingAttackContinuationId, NewTargetUnitId);
}

FWBApplyActionResult WBEffectRunner::ApplyPendingAttackRedirect(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& PendingAttackContinuationId,
	const int32 NewTargetUnitId)
{
	FWBApplyActionResult Result;
	const FWBActionQueryResult Query = WBRules::CanRedirectPendingAttack(
		State, Repository, PendingAttackContinuationId, NewTargetUnitId);
	if (!Query.bOk)
	{
		Result.Reason = Query.Reason;
		return Result;
	}

	const FWBUnitState* NewTarget = State.GetUnitById(NewTargetUnitId);
	if (NewTarget == nullptr)
	{
		Result.Reason = TEXT("no_redirect_target");
		return Result;
	}

	const int32 PreviousTargetUnitId = State.PendingAttack.DefenderUnitId;
	State.PendingAttack.DefenderUnitId = NewTargetUnitId;
	State.PendingAttack.DefenderTile = FWBTile(NewTarget->X, NewTarget->Y);
	if (State.ReactionWindow.Kind == EWBReactionWindowKind::PreHit)
	{
		State.ReactionWindow.SourceUnitId = State.PendingAttack.AttackerUnitId;
		State.ReactionWindow.TargetUnitId = NewTargetUnitId;
	}

	FWBTraceEvent Redirected;
	Redirected.Kind = FName(TEXT("pending_attack_redirected"));
	Redirected.SourceUnitId = State.PendingAttack.AttackerUnitId;
	Redirected.PreviousTargetUnitId = PreviousTargetUnitId;
	Redirected.TargetUnitId = NewTargetUnitId;
	Redirected.FromTile = State.PendingAttack.AttackerTile;
	Redirected.ToTile = State.PendingAttack.DefenderTile;
	Redirected.AttackContinuationId = State.PendingAttack.ContinuationId;
	Redirected.AttackContinuationStage = FName(TEXT("pre_hit"));
	Redirected.bCounterAttack = State.PendingAttack.bCounter;
	Redirected.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Redirected));
	Result.bOk = true;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyPendingAttackDamageSubstitutionRegistration(
	FWBGameStateData& State,
	const FString& PendingAttackContinuationId,
	const int32 SubstituteUnitId)
{
	FWBApplyActionResult Result;
	const FWBActionQueryResult Query =
		WBRules::CanRegisterPendingAttackDamageSubstitution(
			State, PendingAttackContinuationId, SubstituteUnitId);
	if (!Query.bOk)
	{
		Result.Reason = Query.Reason;
		return Result;
	}

	const int32 PreviousRecipientUnitId = State.PendingAttack.DamageSubstitution.bActive
		? State.PendingAttack.DamageSubstitution.SubstituteUnitId
		: INDEX_NONE;
	State.PendingAttack.DamageSubstitution.bActive = true;
	State.PendingAttack.DamageSubstitution.ProtectedUnitId =
		State.PendingAttack.DefenderUnitId;
	State.PendingAttack.DamageSubstitution.SubstituteUnitId = SubstituteUnitId;

	FWBTraceEvent Substituted;
	Substituted.Kind =
		FName(TEXT("pending_attack_damage_substitution_registered"));
	Substituted.SourceUnitId = State.PendingAttack.AttackerUnitId;
	Substituted.PreviousTargetUnitId = PreviousRecipientUnitId;
	Substituted.TargetUnitId = SubstituteUnitId;
	Substituted.AttackDefenderUnitId = State.PendingAttack.DefenderUnitId;
	Substituted.HitUnitId = State.PendingAttack.DefenderUnitId;
	Substituted.DamageRecipientUnitId = SubstituteUnitId;
	Substituted.AttackContinuationId = State.PendingAttack.ContinuationId;
	Substituted.AttackContinuationStage = FName(TEXT("pre_hit"));
	Substituted.bCounterAttack = State.PendingAttack.bCounter;
	Substituted.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Substituted));
	Result.bOk = true;
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyZeroHPDeathRemoval(
	FWBGameStateData& State,
	const EWBUnitDestructionCause Cause)
{
	return WBDeathResolution::ApplyZeroHPDeathResolution(State, Cause);
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
		FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(
			State,
			EWBUnitDestructionCause::EffectDamage);
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
	return ApplyEffectRequest(
		State, Request, FWBCardDefinitionRepository());
}

FWBEffectRequestResult WBEffectRunner::ApplyEffectRequest(
	FWBGameStateData& State,
	const FWBEffectRequest& Request,
	const FWBCardDefinitionRepository& Repository)
{
	FWBEffectRequestResult Result;
	Result.Request = Request;

	const FWBActionQueryResult Query = Repository.RepositoryId.IsEmpty()
		? WBRules::CanApplyEffectRequest(State, Request)
		: WBRules::CanApplyEffectRequest(State, Repository, Request);
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
		case EWBGenericEffectOp::NegatePendingEffect:
		{
			PayloadResult.bOk = true;
			AppendPendingEffectNegationRequestedTrace(
				PayloadResult.TraceEvents,
				Request,
				Payload.PendingEffectFrameId);
			break;
		}
		case EWBGenericEffectOp::PreventPendingAttack:
		{
			PayloadResult.bOk = true;
			break;
		}
		case EWBGenericEffectOp::RedirectPendingAttack:
		{
			PayloadResult = ApplyPendingAttackRedirect(
				WorkingState,
				Repository,
				Payload.PendingAttackContinuationId,
				Request.Target.TargetUnitId);
			break;
		}
		case EWBGenericEffectOp::RegisterPendingAttackHPDamageSubstitution:
		{
			PayloadResult = ApplyPendingAttackDamageSubstitutionRegistration(
				WorkingState,
				Payload.PendingAttackContinuationId,
				Request.Target.TargetUnitId);
			break;
		}
		case EWBGenericEffectOp::ReplacePendingAttackDefenderFromHand:
		{
			if (Repository.RepositoryId.IsEmpty())
			{
				PayloadResult.Reason =
					TEXT("card_definition_repository_required");
			}
			else
			{
				PayloadResult =
					WBUnitReplacementEffect::ApplyPendingAttackDefenderReplacement(
						WorkingState, Request, Payload, Repository);
			}
			break;
		}
		case EWBGenericEffectOp::SacrificeSourceThenSummonCharacterFromDeckToSourceTile:
		{
			// Coordinator resolution owns the multi-decision continuation after
			// the generic pending-effect reaction window closes.
			PayloadResult.bOk = true;
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
	return ApplyCardActivationCommand(
		State, Command, FWBCardDefinitionRepository());
}

FWBCardActivationCommandResult WBEffectRunner::ApplyCardActivationCommand(
	FWBGameStateData& State,
	const FWBCardActivationCommand& Command,
	const FWBCardDefinitionRepository& Repository)
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

	const FWBEffectRequestResult EffectResult = ApplyEffectRequest(
		WorkingState, FilledCommand.EffectRequest, Repository);
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

	TArray<int32> OrderedUnitIds;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.IsUnitOnBoard())
		{
			OrderedUnitIds.Add(Unit.UnitId);
		}
	}
	OrderedUnitIds.Sort();
	for (const int32 UnitId : OrderedUnitIds)
	{
		FWBUnitState* UnitPtr =
			State.GetMutableUnitById(UnitId);
		if (UnitPtr == nullptr
			|| !UnitPtr->IsUnitOnBoard())
		{
			continue;
		}
		FWBUnitState& Unit = *UnitPtr;

		if (!Unit.HasStatus(PoisonStatusId) || Unit.HasStatus(EffectRunnerFrozenStatusId))
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
		FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(
			State,
			EWBUnitDestructionCause::StatusDamage);
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
			EffectRunnerStunnedStatusId,
			EffectRunnerFrozenStatusId
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
		FWBApplyActionResult CleanupResult = ApplyZeroHPDeathRemoval(
			State,
			EWBUnitDestructionCause::StatusDamage);
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
	return WBMatchCoordinator::ApplyLegacyCompatibilityTurnTransition(
		State,
		EndingPlayerId,
		NextPlayerExplicitMPRoll);
}

FWBApplyActionResult WBEffectRunner::ApplyTurnStartMPRoll(
	FWBGameStateData& State,
	const int32 PlayerId,
	const int32 ExplicitMPRoll)
{
	FWBApplyActionResult Result;
	FString Reason;
	if (!WBRules::CanApplyTurnStartResourceSetup(
		State,
		PlayerId,
		ExplicitMPRoll,
		Reason)
		|| !State.ApplyTurnStartMPRollForPlayer(
			PlayerId,
			ExplicitMPRoll,
			Reason))
	{
		Result.Reason = Reason;
		return Result;
	}

	const FWBPlayerStateData* Player =
		State.GetPlayerById(PlayerId);
	if (Player == nullptr)
	{
		Result.Reason = TEXT("missing_player_state");
		return Result;
	}

	FWBTraceEvent Event;
	Event.Kind = FName(TEXT("turn_start_mp_rolled"));
	Event.PlayerId = PlayerId;
	Event.TurnNumber = State.TurnNumber;
	Event.MPRoll = ExplicitMPRoll;
	Event.RemainingMP = Player->RemainingMP;
	Event.MatchPhase = FName(TEXT("turn_start"));
	Event.bOk = true;
	Result.bOk = true;
	Result.TraceEvents.Add(Event);
	return Result;
}

FWBApplyActionResult WBEffectRunner::ApplyTurnStartResourceReset(
	FWBGameStateData& State,
	const int32 PlayerId)
{
	FWBApplyActionResult Result;
	if (State.bGameOver)
	{
		Result.Reason = TEXT("game_over");
		return Result;
	}
	if (!FWBGameStateData::IsValidPlayerId(PlayerId))
	{
		Result.Reason = TEXT("bad_player");
		return Result;
	}
	if (PlayerId != State.CurrentPlayer)
	{
		Result.Reason = TEXT("not_active_player");
		return Result;
	}

	int32 ResetUnitCount = 0;
	for (const FWBUnitState& Unit : State.Units)
	{
		if (Unit.OwnerId == PlayerId && Unit.IsUnitOnBoard())
		{
			++ResetUnitCount;
		}
	}

	FString Reason;
	if (!State.ResetTurnStartResourcesForPlayer(
		PlayerId,
		Reason))
	{
		Result.Reason = Reason;
		return Result;
	}
	const FWBPlayerStateData* Player =
		State.GetPlayerById(PlayerId);
	if (Player == nullptr)
	{
		Result.Reason = TEXT("missing_player_state");
		return Result;
	}

	FWBTraceEvent Attacks;
	Attacks.Kind =
		FName(TEXT("turn_start_attacks_reset"));
	Attacks.PlayerId = PlayerId;
	Attacks.TurnNumber = State.TurnNumber;
	Attacks.CardCount = ResetUnitCount;
	Attacks.MatchPhase = FName(TEXT("turn_start"));
	Attacks.bOk = true;
	Result.TraceEvents.Add(Attacks);

	FWBTraceEvent Walls;
	Walls.Kind =
		FName(TEXT("turn_start_wall_restored"));
	Walls.PlayerId = PlayerId;
	Walls.TurnNumber = State.TurnNumber;
	Walls.WallsLeft = Player->WallsLeft;
	Walls.WallRemovalsLeft =
		Player->WallRemovalsLeft;
	Walls.MatchPhase = FName(TEXT("turn_start"));
	Walls.bOk = true;
	Result.TraceEvents.Add(Walls);
	Result.bOk = true;
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
