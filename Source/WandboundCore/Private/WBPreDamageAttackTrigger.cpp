#include "WBPreDamageAttackTrigger.h"

#include "WBCharacterPassiveEligibility.h"
#include "WBDeterministicRandom.h"

namespace
{
bool ApplyModifier(
	FWBPendingAttackState& Attack,
	const FWBPendingBattleHitModifierDefinition& Modifier,
	const int32 SourceUnitId,
	FString& OutReason)
{
	switch (Modifier.Operation)
	{
	case EWBPendingBattleHitModifierOperation::ReflectToAttacker:
		Attack.DefenderUnitId = Attack.AttackerUnitId;
		Attack.DefenderTile = Attack.AttackerTile;
		Attack.bPendingBattleHitReflectedToAttacker = true;
		Attack.bCounterSuppressedByPendingHitTransform = true;
		Attack.PendingHitTransformSourceUnitId = SourceUnitId;
		OutReason.Reset();
		return true;

	case EWBPendingBattleHitModifierOperation::AddRawDamage:
		Attack.RawDamageModifier = FMath::Max(
			Attack.RawDamageModifier + Modifier.Amount, 0);
		OutReason.Reset();
		return true;

	case EWBPendingBattleHitModifierOperation::Unknown:
	default:
		OutReason = TEXT("pre_damage_attack_modifier_unsupported");
		return false;
	}
}
}

FString WBPreDamageAttackTrigger::BuildUsageKey(
	const int32 SourceUnitId,
	const FString& TriggerId,
	const int32 TurnNumber)
{
	return FString::Printf(
		TEXT("pre_damage_attack:u%d:%s:turn%d"),
		SourceUnitId,
		*TriggerId,
		TurnNumber);
}

FWBPreDamageAttackTriggerResult WBPreDamageAttackTrigger::Resolve(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	uint32& InOutRandomState)
{
	FWBPreDamageAttackTriggerResult Result;
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage
			!= EWBAttackContinuationStage::AutomaticPreDamageModifiers)
	{
		Result.Reason = TEXT("pending_attack_not_automatic_pre_damage_modifiers");
		return Result;
	}
	if (State.PendingAttack.bAutomaticPreDamageModifiersProcessed)
	{
		Result.Reason = TEXT("automatic_pre_damage_modifiers_already_processed");
		return Result;
	}
	if (State.PendingAttack.bPrevented)
	{
		State.PendingAttack.bAutomaticPreDamageModifiersProcessed = true;
		State.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
		Result.bOk = true;
		return Result;
	}

	const int32 InitialDefenderUnitId = State.PendingAttack.DefenderUnitId;
	const FWBUnitState* Defender = State.GetUnitById(InitialDefenderUnitId);
	if (Defender == nullptr || !Defender->IsUnitOnBoard() || Defender->bDefeated)
	{
		Result.Reason = TEXT("automatic_pre_damage_defender_unavailable");
		return Result;
	}
	if (!WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(*Defender))
	{
		State.PendingAttack.bAutomaticPreDamageModifiersProcessed = true;
		State.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
		Result.bOk = true;
		return Result;
	}

	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, Defender->CardId);
	if (!Lookup.bFound)
	{
		State.PendingAttack.bAutomaticPreDamageModifiersProcessed = true;
		State.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
		Result.bOk = true;
		return Result;
	}

	TArray<FWBPreDamageAttackTriggerDefinition> Triggers =
		Lookup.Definition.PreDamageAttackTriggers;
	Triggers.Sort([](
		const FWBPreDamageAttackTriggerDefinition& A,
		const FWBPreDamageAttackTriggerDefinition& B)
	{
		return A.TriggerId < B.TriggerId;
	});

	for (const FWBPreDamageAttackTriggerDefinition& Trigger : Triggers)
	{
		if (State.PendingAttack.DefenderUnitId != InitialDefenderUnitId)
		{
			break;
		}
		if (Trigger.SourceRole
			!= EWBPreDamageAttackTriggerSourceRole::CurrentDefender)
		{
			Result.Reason = TEXT("pre_damage_attack_source_role_unsupported");
			return Result;
		}
		if (Trigger.Timing
			!= EWBPreDamageAttackTriggerTiming::AfterPreHitBeforeCalculateDamage)
		{
			Result.Reason = TEXT("pre_damage_attack_timing_unsupported");
			return Result;
		}
		if (!Trigger.bMandatory
			|| Trigger.RandomBranch
				!= EWBDeterministicRandomBranchKind::CoinFlip)
		{
			Result.Reason = TEXT("pre_damage_attack_trigger_unsupported");
			return Result;
		}

		const FString UsageKey = BuildUsageKey(
			InitialDefenderUnitId, Trigger.TriggerId, State.TurnNumber);
		if (Trigger.bOncePerTurn
			&& State.HasActivationUsageKeyThisTurn(
				Defender->GetControllerPlayerIdForRules(), UsageKey))
		{
			continue;
		}

		const bool bHeads = WBDeterministicRandom::FlipCoinHeads(InOutRandomState);
		FString ModifierReason;
		if (!ApplyModifier(
			State.PendingAttack,
			bHeads ? Trigger.Heads : Trigger.Tails,
			InitialDefenderUnitId,
			ModifierReason))
		{
			Result.Reason = ModifierReason;
			return Result;
		}
		if (Trigger.bOncePerTurn)
		{
			State.MarkActivationUsageKeyForTest(
				Defender->GetControllerPlayerIdForRules(), UsageKey);
		}

		FWBTraceEvent Event;
		Event.Kind = FName(TEXT("random_branch_resolved"));
		Event.ActionId = Trigger.TriggerId;
		Event.PlayerId = Defender->GetControllerPlayerIdForRules();
		Event.SourceUnitId = InitialDefenderUnitId;
		Event.PreviousTargetUnitId = InitialDefenderUnitId;
		Event.TargetUnitId = State.PendingAttack.DefenderUnitId;
		Event.AttackDefenderUnitId = State.PendingAttack.DefenderUnitId;
		Event.DamageAmount = State.PendingAttack.RawDamageModifier;
		Event.RandomOutcome = bHeads
			? FName(TEXT("heads")) : FName(TEXT("tails"));
		Event.AttackContinuationId = State.PendingAttack.ContinuationId;
		Event.AttackContinuationStage =
			FName(TEXT("automatic_pre_damage_modifiers"));
		Event.bCounterAttack = State.PendingAttack.bCounter;
		Event.bOk = true;
		Result.TraceEvents.Add(MoveTemp(Event));
		++Result.ResolvedTriggerCount;
	}

	State.PendingAttack.bAutomaticPreDamageModifiersProcessed = true;
	State.PendingAttack.Stage = EWBAttackContinuationStage::CalculateDamage;
	Result.bOk = true;
	return Result;
}
