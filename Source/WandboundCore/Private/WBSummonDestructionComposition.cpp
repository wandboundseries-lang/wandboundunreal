#include "WBSummonDestructionComposition.h"

#include "WBCardZoneState.h"
#include "WBCSNInheritance.h"
#include "WBEffectRunner.h"

namespace
{
FWBSummonDestructionCompositionResult MakeSummonDestructionCompositionFailure(
	const FString& Reason)
{
	FWBSummonDestructionCompositionResult Result;
	Result.Reason = Reason;
	return Result;
}
}

FWBSummonDestructionCompositionResult WBSummonDestructionComposition::Apply(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FWBSummonDestructionCompositionRequest& Request)
{
	const FWBUnitState* Target = State.GetUnitById(
		Request.DestructionTargetUnitId);
	if (Target == nullptr || !Target->IsUnitOnBoard() || Target->bDefeated)
	{
		return MakeSummonDestructionCompositionFailure(
			TEXT("composition_destruction_target_unavailable"));
	}
	const FWBTile VacatedTile(Target->X, Target->Y);
	if (Request.Summon.TargetTile != VacatedTile)
	{
		return MakeSummonDestructionCompositionFailure(
			TEXT("composition_destination_not_destroyed_tile"));
	}
	const FWBPlayerStateData* Owner = State.GetPlayerById(
		Target->GetOwnerPlayerIdForRules());
	const bool bTargetIsHero = Owner != nullptr
		&& Owner->HeroUnitId == Target->UnitId;
	if (bTargetIsHero
		&& Request.HeroPolicy == EWBDestructionSummonHeroPolicy::RejectHero)
	{
		return MakeSummonDestructionCompositionFailure(
			TEXT("composition_hero_destruction_unsupported"));
	}
	if (Request.InheritancePolicy
		== EWBDestructionSummonInheritancePolicy::ApplyCSNInheritance
		&& Request.TransactionId.IsEmpty())
	{
		return MakeSummonDestructionCompositionFailure(
			TEXT("composition_transaction_id_missing"));
	}
	if (Request.PendingAttackPolicy
		== EWBDestructionSummonPendingAttackPolicy::PreserveAndRedirect)
	{
		if (!State.HasPendingAttack()
			|| Request.PendingAttackContinuationId.IsEmpty()
			|| State.PendingAttack.ContinuationId
				!= Request.PendingAttackContinuationId
			|| (State.PendingAttack.AttackerUnitId != Target->UnitId
				&& State.PendingAttack.DefenderUnitId != Target->UnitId))
		{
			return MakeSummonDestructionCompositionFailure(
				TEXT("composition_pending_attack_mismatch"));
		}
	}

	FWBGameStateData WorkingState = State;
	FWBUnitDestructionRequest Destruction;
	Destruction.TargetUnitId = Request.DestructionTargetUnitId;
	Destruction.Cause = Request.DestructionCause;
	Destruction.EquipmentDisposition = Request.InheritancePolicy
		== EWBDestructionSummonInheritancePolicy::ApplyCSNInheritance
		? EWBDestructionEquipmentDisposition::DetachForContinuation
		: EWBDestructionEquipmentDisposition::Discard;
	Destruction.PendingAttackPolicy = Request.PendingAttackPolicy
		== EWBDestructionSummonPendingAttackPolicy::PreserveAndRedirect
		? EWBDestructionPendingAttackPolicy::PreserveForRedirect
		: EWBDestructionPendingAttackPolicy::ClearIfParticipant;
	Destruction.TerminalPolicy = bTargetIsHero
		? EWBDestructionTerminalPolicy::DeferToComposition
		: EWBDestructionTerminalPolicy::CommitImmediately;
	Destruction.TerminalSource = EWBTerminalSource::Effect;
	const FWBUnitDestructionResult Destroyed =
		WBDeathResolution::ApplyGenuineUnitDestruction(
			WorkingState, Destruction);
	if (!Destroyed.bOk)
	{
		return MakeSummonDestructionCompositionFailure(Destroyed.Reason);
	}
	if (!Destroyed.bDestroyed)
	{
		return MakeSummonDestructionCompositionFailure(Destroyed.bPrevented
			? FString(TEXT("required_destruction_prevented"))
			: FString(TEXT("required_destruction_not_committed")));
	}

	const FWBCharacterSummonResult Summoned =
		WBCharacterSummon::SummonExactCharacter(
			WorkingState, Repository, Request.Summon);
	if (!Summoned.bOk)
	{
		return MakeSummonDestructionCompositionFailure(Summoned.Reason);
	}

	TArray<FWBTraceEvent> DownstreamTrace = Summoned.TraceEvents;
	TArray<FWBTraceEvent> RedirectTrace;
	TArray<FWBTraceEvent> TerminalTrace;
	if (Request.PendingAttackPolicy
		== EWBDestructionSummonPendingAttackPolicy::PreserveAndRedirect)
	{
		const FWBApplyActionResult Redirected =
			WBEffectRunner::ApplyPendingAttackRedirect(
				WorkingState,
				Request.PendingAttackContinuationId,
				Summoned.CreatedUnitId);
		if (!Redirected.bOk)
		{
			return MakeSummonDestructionCompositionFailure(Redirected.Reason);
		}
		for (FWBTraceEvent& Event : DownstreamTrace)
		{
			if (Event.Kind == Request.Summon.TraceKind)
			{
				Event.AttackContinuationId =
					Request.PendingAttackContinuationId;
			}
		}
		RedirectTrace = Redirected.TraceEvents;
	}

	if (bTargetIsHero)
	{
		const FWBApplyActionResult Terminal =
			WBDeathResolution::CommitDeferredHeroDestruction(
				WorkingState,
				Destroyed.Snapshot,
				EWBTerminalSource::Effect);
		if (!Terminal.bOk)
		{
			return MakeSummonDestructionCompositionFailure(Terminal.Reason);
		}
		TerminalTrace = Terminal.TraceEvents;
	}

	if (Request.InheritancePolicy
		== EWBDestructionSummonInheritancePolicy::ApplyCSNInheritance)
	{
		FWBCSNInheritanceMutationRequest Inheritance;
		Inheritance.SourceSnapshot =
			Destroyed.Snapshot.DestroyedUnitSnapshot;
		Inheritance.ControllerPlayerId = Request.Summon.ControllerPlayerId;
		Inheritance.SourceUnitId = Destroyed.Snapshot.DestroyedUnitId;
		Inheritance.TargetUnitId = Summoned.CreatedUnitId;
		Inheritance.SourceCurrentRL = Destroyed.Snapshot.CurrentRLSnapshot;
		Inheritance.EquippedWandSnapshot = Destroyed.Snapshot.EquippedWands;
		Inheritance.ExpectedWandLocation =
			EWBCSNInheritanceWandLocation::DetachedSourceSnapshot;
		Inheritance.TransactionId = Request.TransactionId;
		const FWBCSNInheritanceMutationResult Inherited =
			WBCSNInheritance::Apply(WorkingState, Repository, Inheritance);
		if (!Inherited.bOk)
		{
			return MakeSummonDestructionCompositionFailure(Inherited.Reason);
		}
		DownstreamTrace.Append(Inherited.TraceEvents);
	}
	DownstreamTrace.Append(RedirectTrace);
	DownstreamTrace.Append(TerminalTrace);

	FString ZoneReason;
	if (!WBCardZoneState::ValidateZoneStateForTest(
		WorkingState.GetCardZoneState(), ZoneReason))
	{
		return MakeSummonDestructionCompositionFailure(ZoneReason.IsEmpty()
			? FString(TEXT("invalid_zone_state")) : ZoneReason);
	}

	FWBSummonDestructionCompositionResult Result;
	Result.bOk = true;
	Result.DestroyedUnitId = Destroyed.Snapshot.DestroyedUnitId;
	Result.CreatedUnitId = Summoned.CreatedUnitId;
	Result.DestructionSnapshot = Destroyed.Snapshot;
	Result.TraceEvents = Destroyed.TraceEvents;
	Result.TraceEvents.Append(DownstreamTrace);
	State = MoveTemp(WorkingState);
	return Result;
}
