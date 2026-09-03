#include "WBAfterDamageTrigger.h"

#include "WBEffectRunner.h"
#include "WBCharacterPassiveEligibility.h"

namespace
{
bool MatchesSourceRole(
	const int32 SourceUnitId,
	const FWBAfterDamageEventContext& Context,
	const EWBAfterDamageParticipantRole Role)
{
	switch (Role)
	{
	case EWBAfterDamageParticipantRole::Attacker:
		return SourceUnitId == Context.AttackerUnitId;
	case EWBAfterDamageParticipantRole::HitUnit:
		return SourceUnitId == Context.HitUnitId;
	case EWBAfterDamageParticipantRole::FinalDamageRecipient:
		return SourceUnitId == Context.FinalDamageRecipientUnitId;
	case EWBAfterDamageParticipantRole::BattleParticipant:
		return SourceUnitId == Context.AttackerUnitId
			|| SourceUnitId == Context.HitUnitId;
	default:
		return false;
	}
}

bool IsEligibleUnitSource(const FWBUnitState& Unit)
{
	return WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(Unit);
}

bool IsEligibleWandBearer(const FWBUnitState& Unit)
{
	return Unit.IsUnitOnBoard() && !Unit.bDefeated;
}

int32 ResolveOpposingBattleUnitId(
	const int32 SourceUnitId,
	const FWBAfterDamageEventContext& Context)
{
	if (SourceUnitId == Context.AttackerUnitId)
	{
		return Context.HitUnitId;
	}
	if (SourceUnitId == Context.HitUnitId)
	{
		return Context.AttackerUnitId;
	}
	return INDEX_NONE;
}

int32 ResolveTargetUnitId(
	const FWBAfterDamageTriggerInstance& Trigger,
	const FWBAfterDamageEventContext& Context)
{
	switch (Trigger.Definition.TargetRole)
	{
	case EWBAfterDamageTargetRole::None:
		return INDEX_NONE;
	case EWBAfterDamageTargetRole::Self:
		return Trigger.SourceUnitId;
	case EWBAfterDamageTargetRole::Attacker:
		return Context.AttackerUnitId;
	case EWBAfterDamageTargetRole::HitUnit:
		return Context.HitUnitId;
	case EWBAfterDamageTargetRole::FinalDamageRecipient:
		return Context.FinalDamageRecipientUnitId;
	case EWBAfterDamageTargetRole::OpposingBattleUnit:
		return ResolveOpposingBattleUnitId(Trigger.SourceUnitId, Context);
	default:
		return INDEX_NONE;
	}
}

FString BuildStableTriggerId(
	const FWBAfterDamageTriggerInstance& Trigger,
	const FWBAfterDamageEventContext& Context)
{
	const TCHAR* SourceKind = Trigger.SourceKind
		== EWBAfterDamageTriggerSourceKind::EquippedWand
		? TEXT("wand") : TEXT("unit");
	const FString SourceIdentity = Trigger.SourceCardInstanceId.IsEmpty()
		? Trigger.SourceCardId : Trigger.SourceCardInstanceId;
	return FString::Printf(
		TEXT("after_damage:%s:%s:p%d:u%d:%s:%s"),
		*Context.AttackContinuationId,
		SourceKind,
		Trigger.ControllerPlayerId,
		Trigger.SourceUnitId,
		*SourceIdentity,
		*Trigger.Definition.TriggerId);
}

bool AfterDamageTriggerLess(
	const FWBAfterDamageTriggerInstance& A,
	const FWBAfterDamageTriggerInstance& B,
	const int32 OriginatingPlayerId)
{
	const bool bAOriginControlled = A.ControllerPlayerId == OriginatingPlayerId;
	const bool bBOriginControlled = B.ControllerPlayerId == OriginatingPlayerId;
	if (bAOriginControlled != bBOriginControlled)
	{
		return bAOriginControlled;
	}
	if (A.ControllerPlayerId != B.ControllerPlayerId)
	{
		return A.ControllerPlayerId < B.ControllerPlayerId;
	}
	if (A.SourceUnitId != B.SourceUnitId)
	{
		return A.SourceUnitId < B.SourceUnitId;
	}
	if (A.SourceKind != B.SourceKind)
	{
		return static_cast<uint8>(A.SourceKind)
			< static_cast<uint8>(B.SourceKind);
	}
	if (A.EquipOrder != B.EquipOrder)
	{
		return A.EquipOrder < B.EquipOrder;
	}
	return A.StableTriggerId < B.StableTriggerId;
}

void AddDefinitionsForSource(
	const FWBCardDefinition& Card,
	const int32 ControllerPlayerId,
	const int32 SourceUnitId,
	const EWBAfterDamageTriggerSourceKind SourceKind,
	const FString& SourceCardInstanceId,
	const int32 EquipOrder,
	FWBAfterDamageTriggerCollection& Collection)
{
	for (const FWBAfterDamageTriggerDefinition& Definition :
		Card.AfterDamageTriggers)
	{
		if (Definition.TriggerId.IsEmpty()
			|| !MatchesSourceRole(
				SourceUnitId,
				Collection.Context,
				Definition.SourceRole))
		{
			continue;
		}

		FWBAfterDamageTriggerInstance Trigger;
		Trigger.ControllerPlayerId = ControllerPlayerId;
		Trigger.SourceKind = SourceKind;
		Trigger.SourceUnitId = SourceUnitId;
		Trigger.SourceCardId = Card.CardId;
		Trigger.SourceCardInstanceId = SourceCardInstanceId;
		Trigger.EquipOrder = EquipOrder;
		Trigger.Definition = Definition;
		Trigger.StableTriggerId = BuildStableTriggerId(
			Trigger, Collection.Context);
		Collection.Triggers.Add(MoveTemp(Trigger));
	}
}
}

FWBAfterDamageTriggerCollection WBAfterDamageTrigger::CaptureBeforeDamage(
	const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository)
{
	FWBAfterDamageTriggerCollection Result;
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage != EWBAttackContinuationStage::ApplyDamage
		|| !State.PendingAttack.DamageCalculation.bValid)
	{
		Result.Reason = TEXT("after_damage_capture_stage_invalid");
		return Result;
	}

	const FWBPendingAttackState& Attack = State.PendingAttack;
	const FWBPendingAttackState::FDamageCalculation& Calculation =
		Attack.DamageCalculation;
	FWBAfterDamageEventContext& Context = Result.Context;
	Context.AttackerUnitId = Attack.AttackerUnitId;
	Context.HitUnitId = Calculation.HitUnitId;
	Context.FinalDamageRecipientUnitId = Attack.FinalDamageRecipientUnitId;
	Context.RawAttackDamage = Calculation.RawAttackDamage;
	Context.ArmorAbsorbedAmount = Calculation.ArmorAbsorbedAmount;
	Context.CalculatedHPDamage = Calculation.CalculatedHPDamage;
	Context.bDamageSubstituted =
		Context.FinalDamageRecipientUnitId != Context.HitUnitId;
	Context.bPrevented = Calculation.bPrevented;
	Context.bFrozenBreak = Calculation.bFrozenBreak;
	Context.bCounterAttack = Attack.bCounter;
	Context.DeclarationActionId = Attack.DeclarationActionId;
	Context.AttackContinuationId = Attack.ContinuationId;
	Context.HitUnitPreviousHP = Calculation.PreviousHP;
	Context.HitUnitResultingHP = Calculation.PreviousHP;

	const FWBUnitState* Attacker = State.GetUnitById(Context.AttackerUnitId);
	const FWBUnitState* HitUnit = State.GetUnitById(Context.HitUnitId);
	const FWBUnitState* FinalRecipient =
		State.GetUnitById(Context.FinalDamageRecipientUnitId);
	Context.AttackerControllerId = Attacker != nullptr
		? Attacker->GetControllerPlayerIdForRules() : INDEX_NONE;
	Context.HitUnitControllerId = HitUnit != nullptr
		? HitUnit->GetControllerPlayerIdForRules() : INDEX_NONE;
	Context.FinalRecipientControllerId = FinalRecipient != nullptr
		? FinalRecipient->GetControllerPlayerIdForRules() : INDEX_NONE;
	Context.FinalRecipientPreviousHP = FinalRecipient != nullptr
		? FinalRecipient->HP : 0;
	Context.FinalRecipientResultingHP = Context.FinalRecipientPreviousHP;

	TArray<int32> InvolvedUnitIds =
	{
		Context.AttackerUnitId,
		Context.HitUnitId,
		Context.FinalDamageRecipientUnitId
	};
	InvolvedUnitIds.Sort();
	for (int32 Index = InvolvedUnitIds.Num() - 1; Index > 0; --Index)
	{
		if (InvolvedUnitIds[Index] == InvolvedUnitIds[Index - 1])
		{
			InvolvedUnitIds.RemoveAt(Index, 1, EAllowShrinking::No);
		}
	}

	for (const int32 UnitId : InvolvedUnitIds)
	{
		const FWBUnitState* Unit = State.GetUnitById(UnitId);
		if (Unit == nullptr || !IsEligibleUnitSource(*Unit))
		{
			continue;
		}
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(Repository, Unit->CardId);
		if (Lookup.bFound)
		{
			AddDefinitionsForSource(
				Lookup.Definition,
				Unit->GetControllerPlayerIdForRules(),
				Unit->UnitId,
				EWBAfterDamageTriggerSourceKind::Unit,
				FString(),
				INDEX_NONE,
				Result);
		}
	}

	TArray<FWBEquippedCardEntry> Equipped = State.GetCardZoneState().EquippedCards;
	Equipped.Sort([](const FWBEquippedCardEntry& A, const FWBEquippedCardEntry& B)
	{
		if (A.EquippedToUnitId != B.EquippedToUnitId)
		{
			return A.EquippedToUnitId < B.EquippedToUnitId;
		}
		if (A.EquipOrder != B.EquipOrder)
		{
			return A.EquipOrder < B.EquipOrder;
		}
		return A.Card.InstanceId < B.Card.InstanceId;
	});
	for (const FWBEquippedCardEntry& Entry : Equipped)
	{
		if (!InvolvedUnitIds.Contains(Entry.EquippedToUnitId))
		{
			continue;
		}
		const FWBUnitState* Bearer = State.GetUnitById(Entry.EquippedToUnitId);
		if (Bearer == nullptr || !IsEligibleWandBearer(*Bearer))
		{
			continue;
		}
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				Repository, Entry.Card.CardId);
		if (!Lookup.bFound || Lookup.Definition.Kind != EWBCardDefinitionKind::Wand)
		{
			continue;
		}
		AddDefinitionsForSource(
			Lookup.Definition,
			Entry.Card.OwnerPlayerId,
			Entry.EquippedToUnitId,
			EWBAfterDamageTriggerSourceKind::EquippedWand,
			Entry.Card.InstanceId,
			Entry.EquipOrder,
			Result);
	}

	Result.Triggers.Sort(
		[OriginatingPlayerId = Attack.AttackingPlayerId](
			const FWBAfterDamageTriggerInstance& A,
			const FWBAfterDamageTriggerInstance& B)
		{
			return AfterDamageTriggerLess(A, B, OriginatingPlayerId);
		});
	Result.bOk = true;
	return Result;
}

bool WBAfterDamageTrigger::FinalizeContextAfterDamage(
	const FWBGameStateData& State,
	const TArray<FWBTraceEvent>& DamageTraceEvents,
	FWBAfterDamageTriggerCollection& InOutCollection,
	FString& OutReason)
{
	if (!InOutCollection.bOk)
	{
		OutReason = InOutCollection.Reason;
		return false;
	}
	const FWBTraceEvent* Applied = DamageTraceEvents.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("attack_damage_applied"));
		});
	if (Applied == nullptr)
	{
		OutReason = TEXT("after_damage_applied_trace_missing");
		return false;
	}

	FWBAfterDamageEventContext& Context = InOutCollection.Context;
	Context.FinalDamageRecipientUnitId = Applied->DamageRecipientUnitId;
	Context.bDamageSubstituted =
		Context.FinalDamageRecipientUnitId != Context.HitUnitId;
	Context.AppliedHPDamage = FMath::Max(Applied->ActualHPDamageAmount, 0);
	Context.FinalRecipientPreviousHP = FMath::Max(Applied->PreviousHP, 0);
	Context.FinalRecipientResultingHP = FMath::Max(Applied->NewHP, 0);
	if (Context.FinalDamageRecipientUnitId == Context.HitUnitId)
	{
		Context.HitUnitPreviousHP = Context.FinalRecipientPreviousHP;
		Context.HitUnitResultingHP = Context.FinalRecipientResultingHP;
	}
	else if (const FWBUnitState* Hit = State.GetUnitById(Context.HitUnitId))
	{
		Context.HitUnitResultingHP = Hit->HP;
	}

	for (int32 Index = InOutCollection.Triggers.Num() - 1; Index >= 0; --Index)
	{
		if (InOutCollection.Triggers[Index].Definition.DamageRequirement
			== EWBAfterDamageRequirement::PositiveHPDamage
			&& Context.AppliedHPDamage <= 0)
		{
			InOutCollection.Triggers.RemoveAt(
				Index, 1, EAllowShrinking::No);
		}
	}
	OutReason.Reset();
	return true;
}

FString WBAfterDamageTrigger::BuildUsageKey(
	const FWBAfterDamageTriggerInstance& Trigger,
	const FWBAfterDamageEventContext& Context)
{
	const TCHAR* SourceKind = Trigger.SourceKind
		== EWBAfterDamageTriggerSourceKind::EquippedWand
		? TEXT("wand") : TEXT("unit");
	const FString SourceIdentity = Trigger.SourceCardInstanceId.IsEmpty()
		? Trigger.SourceCardId : Trigger.SourceCardInstanceId;
	FString Key = FString::Printf(
		TEXT("after_damage:%s:p%d:u%d:%s:%s"),
		SourceKind,
		Trigger.ControllerPlayerId,
		Trigger.SourceUnitId,
		*SourceIdentity,
		*Trigger.Definition.TriggerId);
	if (Trigger.Definition.bOncePerTurnPerOpposingUnit)
	{
		Key += FString::Printf(
			TEXT(":opposing:u%d"),
			ResolveOpposingBattleUnitId(Trigger.SourceUnitId, Context));
	}
	return Key;
}

FWBAfterDamageTriggerResolutionResult WBAfterDamageTrigger::Resolve(
	FWBGameStateData& State,
	const FWBAfterDamageTriggerCollection& Collection)
{
	FWBAfterDamageTriggerResolutionResult Result;
	if (!Collection.bOk)
	{
		Result.Reason = Collection.Reason;
		return Result;
	}
	if (State.HasPendingAttack()
		&& State.PendingAttack.Stage != EWBAttackContinuationStage::AfterDamage)
	{
		Result.Reason = TEXT("after_damage_resolution_stage_invalid");
		return Result;
	}

	for (const FWBAfterDamageTriggerInstance& Trigger : Collection.Triggers)
	{
		if (!Trigger.Definition.bMandatory)
		{
			Result.Reason = TEXT("optional_after_damage_trigger_unsupported");
			return Result;
		}
		if (Trigger.Definition.Payloads.IsEmpty())
		{
			Result.Reason = TEXT("after_damage_trigger_payloads_missing");
			return Result;
		}

		const bool bUsesTurnKey = Trigger.Definition.bOncePerTurn
			|| Trigger.Definition.bOncePerTurnPerOpposingUnit;
		const FString UsageKey = BuildUsageKey(Trigger, Collection.Context);
		if (bUsesTurnKey
			&& State.HasActivationUsageKeyThisTurn(
				Trigger.ControllerPlayerId, UsageKey))
		{
			continue;
		}

		const int32 TargetUnitId = ResolveTargetUnitId(Trigger, Collection.Context);
		if (Trigger.Definition.TargetRole
			== EWBAfterDamageTargetRole::OpposingBattleUnit
			&& TargetUnitId == INDEX_NONE)
		{
			Result.Reason = TEXT("after_damage_opposing_battle_unit_unavailable");
			return Result;
		}

		FWBTraceEvent Collected;
		Collected.Kind = FName(TEXT("after_damage_trigger_collected"));
		Collected.ActionId = Trigger.StableTriggerId;
		Collected.PlayerId = Trigger.ControllerPlayerId;
		Collected.SourceUnitId = Trigger.SourceUnitId;
		Collected.TargetUnitId = TargetUnitId;
		Collected.HitUnitId = Collection.Context.HitUnitId;
		Collected.DamageRecipientUnitId =
			Collection.Context.FinalDamageRecipientUnitId;
		Collected.CardId = Trigger.SourceCardId;
		Collected.CardInstanceId = Trigger.SourceCardInstanceId;
		Collected.EquipOrder = Trigger.EquipOrder;
		Collected.ActualHPDamageAmount = Collection.Context.AppliedHPDamage;
		Collected.AttackContinuationId =
			Collection.Context.AttackContinuationId;
		Collected.AttackContinuationStage = FName(TEXT("after_damage"));
		Collected.bCounterAttack = Collection.Context.bCounterAttack;
		Collected.bOk = true;
		Result.TraceEvents.Add(MoveTemp(Collected));

		FWBEffectRequest Request;
		Request.Source.PlayerId = Trigger.ControllerPlayerId;
		const FWBUnitState* LiveSource = State.GetUnitById(Trigger.SourceUnitId);
		Request.Source.SourceUnitId = LiveSource != nullptr
			&& LiveSource->IsUnitOnBoard() && !LiveSource->bDefeated
			? Trigger.SourceUnitId : INDEX_NONE;
		Request.Source.SourceCardId = Trigger.SourceCardId;
		Request.Source.SourceCardInstanceId = Trigger.SourceCardInstanceId;
		Request.Source.SourceEffectId = Trigger.Definition.TriggerId;
		Request.Target.TargetUnitId = TargetUnitId;
		Request.Payloads = Trigger.Definition.Payloads;

		const FWBEffectRequestResult Effect =
			WBEffectRunner::ApplyEffectRequest(State, Request);
		if (!Effect.bOk)
		{
			Result.Reason = Effect.Reason;
			return Result;
		}
		Result.TraceEvents.Append(Effect.TraceEvents);
		if (bUsesTurnKey)
		{
			State.MarkActivationUsageKeyForTest(
				Trigger.ControllerPlayerId, UsageKey);
		}

		FWBTraceEvent Resolved;
		Resolved.Kind = FName(TEXT("after_damage_trigger_resolved"));
		Resolved.ActionId = Trigger.StableTriggerId;
		Resolved.PlayerId = Trigger.ControllerPlayerId;
		Resolved.SourceUnitId = Trigger.SourceUnitId;
		Resolved.TargetUnitId = TargetUnitId;
		Resolved.HitUnitId = Collection.Context.HitUnitId;
		Resolved.DamageRecipientUnitId =
			Collection.Context.FinalDamageRecipientUnitId;
		Resolved.CardId = Trigger.SourceCardId;
		Resolved.CardInstanceId = Trigger.SourceCardInstanceId;
		Resolved.EquipOrder = Trigger.EquipOrder;
		Resolved.ActualHPDamageAmount = Collection.Context.AppliedHPDamage;
		Resolved.AttackContinuationId =
			Collection.Context.AttackContinuationId;
		Resolved.AttackContinuationStage = FName(TEXT("after_damage"));
		Resolved.bCounterAttack = Collection.Context.bCounterAttack;
		Resolved.bOk = true;
		Result.TraceEvents.Add(MoveTemp(Resolved));
		++Result.ResolvedTriggerCount;

		if (State.bGameOver)
		{
			break;
		}
	}

	if (State.HasPendingAttack()
		&& State.PendingAttack.Stage == EWBAttackContinuationStage::AfterDamage)
	{
		State.PendingAttack.Stage = EWBAttackContinuationStage::PostHit;
	}
	Result.bOk = true;
	return Result;
}
