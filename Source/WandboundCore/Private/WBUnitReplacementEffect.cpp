#include "WBUnitReplacementEffect.h"

#include "WBCharacterSummon.h"
#include "WBPrivateCardChoice.h"
#include "WBSummonDestructionComposition.h"

namespace
{
FWBApplyActionResult MakeUnitReplacementFailure(const FString& Reason)
{
	FWBApplyActionResult Result;
	Result.Reason = Reason;
	return Result;
}
}
FWBApplyActionResult WBUnitReplacementEffect::ApplyPendingAttackDefenderReplacement(
	FWBGameStateData& State,
	const FWBEffectRequest& Request,
	const FWBGenericEffectPayload& Payload,
	const FWBCardDefinitionRepository& Repository)
{
	if (!State.HasPendingAttack()
		|| State.PendingAttack.Stage != EWBAttackContinuationStage::PreHit)
	{
		return MakeUnitReplacementFailure(TEXT("pending_attack_not_pre_hit"));
	}
	if (Payload.PendingAttackContinuationId.IsEmpty()
		|| Payload.PendingAttackContinuationId
			!= State.PendingAttack.ContinuationId)
	{
		return MakeUnitReplacementFailure(TEXT("pending_attack_target_mismatch"));
	}
	if (Request.Target.TargetUnitId != State.PendingAttack.DefenderUnitId)
	{
		return MakeUnitReplacementFailure(
			TEXT("replacement_source_not_current_defender"));
	}
	if (!FWBGameStateData::IsValidPlayerId(Request.Source.PlayerId))
	{
		return MakeUnitReplacementFailure(TEXT("invalid_effect_source_player"));
	}
	if (Request.AuxiliaryCardSelection.Zone
			!= EWBEffectAuxiliaryCardZone::Hand
		|| Request.AuxiliaryCardSelection.CardInstanceId.IsEmpty()
		|| Request.AuxiliaryCardSelection.CardId.IsEmpty())
	{
		return MakeUnitReplacementFailure(
			TEXT("auxiliary_hand_card_selection_missing"));
	}
	if (Payload.RequiredReplacementKind
			!= EWBEffectReplacementCardKind::Character
		|| Payload.InheritancePolicy
			!= EWBEffectInheritancePolicy::
				TransferEquippedWandsAndAddSourceCurrentRL)
	{
		return MakeUnitReplacementFailure(TEXT("unsupported_replacement_policy"));
	}

	const FWBUnitState* SourceUnit = State.GetUnitById(
		Request.Target.TargetUnitId);
	if (SourceUnit == nullptr || !SourceUnit->IsUnitOnBoard()
		|| SourceUnit->bDefeated)
	{
		return MakeUnitReplacementFailure(TEXT("replacement_source_unavailable"));
	}
	if (SourceUnit->GetControllerPlayerIdForRules()
		!= Request.Source.PlayerId)
	{
		return MakeUnitReplacementFailure(
			TEXT("replacement_source_owner_mismatch"));
	}
	const FWBCardDefinitionRepositoryLookupResult SourceDefinition =
		WBCardDefinitionRepository::FindCardById(
			Repository, SourceUnit->CardId);
	if (!SourceDefinition.bFound
		|| (!Payload.RequiredSourceFaction.IsEmpty()
			&& !SourceDefinition.Definition.PublicFactions.Contains(
				Payload.RequiredSourceFaction)))
	{
		return MakeUnitReplacementFailure(
			TEXT("replacement_source_faction_mismatch"));
	}

	FWBPrivateCardChoiceDescriptor PrivateSelection;
	PrivateSelection.ChoosingPlayerId = Request.Source.PlayerId;
	PrivateSelection.SourceZone = EWBCardZone::Hand;
	PrivateSelection.Timing = EWBPrivateCardChoiceTiming::ActivationDeclaration;
	PrivateSelection.TargetDeclaration =
		EWBDeclarationProvenance::PlayerDeclared;
	PrivateSelection.Filter.RequiredKind = EWBCardDefinitionKind::Character;
	PrivateSelection.Filter.RequiredFaction =
		Payload.RequiredReplacementFaction;
	const FWBPrivateCardChoiceSelectionResult Selection =
		WBPrivateCardChoice::ValidateSelection(
			State,
			Repository,
			PrivateSelection,
			Request.AuxiliaryCardSelection.CardInstanceId,
			false);
	if (!Selection.bOk)
	{
		return MakeUnitReplacementFailure(
			TEXT("selected_replacement_not_in_hand"));
	}
	if (Selection.Selected.CardId
		!= Request.AuxiliaryCardSelection.CardId)
	{
		return MakeUnitReplacementFailure(
			TEXT("auxiliary_hand_card_selection_mismatch"));
	}

	FWBSummonDestructionCompositionRequest Composition;
	Composition.DestructionTargetUnitId = SourceUnit->UnitId;
	Composition.DestructionCause =
		EWBUnitDestructionCause::ReplacementEffect;
	Composition.Summon.OwnerPlayerId = Request.Source.PlayerId;
	Composition.Summon.ControllerPlayerId = Request.Source.PlayerId;
	Composition.Summon.SourceZone = EWBCardZone::Hand;
	Composition.Summon.CardInstanceId =
		Request.AuxiliaryCardSelection.CardInstanceId;
	Composition.Summon.ExpectedCardId =
		Request.AuxiliaryCardSelection.CardId;
	Composition.Summon.RequiredFaction =
		Payload.RequiredReplacementFaction;
	Composition.Summon.TargetTile = FWBTile(SourceUnit->X, SourceUnit->Y);
	Composition.Summon.ConditionPolicy =
		EWBCharacterSummonConditionPolicy::IgnoreSummoningConditions;
	Composition.Summon.TraceKind =
		FName(TEXT("effect_replacement_summon"));
	Composition.Summon.SourceUnitId = SourceUnit->UnitId;
	Composition.Summon.bIncludeSelectedInstanceInTrace = true;
	Composition.InheritancePolicy =
		EWBDestructionSummonInheritancePolicy::ApplyCSNInheritance;
	Composition.PendingAttackPolicy =
		EWBDestructionSummonPendingAttackPolicy::PreserveAndRedirect;
	Composition.HeroPolicy =
		EWBDestructionSummonHeroPolicy::SummonThenCommitTerminal;
	Composition.PendingAttackContinuationId =
		Payload.PendingAttackContinuationId;
	Composition.TransactionId = Payload.PendingAttackContinuationId;

	const FWBSummonDestructionCompositionResult Applied =
		WBSummonDestructionComposition::Apply(
			State, Repository, Composition);
	if (!Applied.bOk)
	{
		if (Applied.Reason == TEXT("required_destruction_prevented"))
		{
			return MakeUnitReplacementFailure(
				TEXT("replacement_source_destruction_prevented"));
		}
		if (Applied.Reason == TEXT("source_card_not_in_hand")
			|| Applied.Reason == TEXT("source_card_missing"))
		{
			return MakeUnitReplacementFailure(
				TEXT("selected_replacement_not_in_hand"));
		}
		if (Applied.Reason == TEXT("source_card_id_mismatch"))
		{
			return MakeUnitReplacementFailure(
				TEXT("auxiliary_hand_card_selection_mismatch"));
		}
		return MakeUnitReplacementFailure(Applied.Reason);
	}

	FWBApplyActionResult Result;
	Result.bOk = true;
	Result.TraceEvents = Applied.TraceEvents;
	return Result;
}
