#include "WBCardDefinitionRepository.h"

namespace
{
FWBCardDefinitionRepositoryValidationResult MakeValidationFailure(
	const FWBCardDefinitionRepository& Repository,
	const TCHAR* Reason)
{
	FWBCardDefinitionRepositoryValidationResult Result;
	Result.Reason = Reason;
	Result.DefinitionCount = Repository.Definitions.Num();
	return Result;
}

FWBCardDefinitionRepositoryValidationResult MakeValidationSuccess(
	const FWBCardDefinitionRepository& Repository)
{
	FWBCardDefinitionRepositoryValidationResult Result;
	Result.bOk = true;
	Result.DefinitionCount = Repository.Definitions.Num();
	return Result;
}

TArray<FWBCardDefinition> SortDefinitionsByCardId(const TArray<FWBCardDefinition>& Definitions)
{
	TArray<FWBCardDefinition> SortedDefinitions = Definitions;
	SortedDefinitions.Sort([](const FWBCardDefinition& A, const FWBCardDefinition& B)
	{
		if (A.CardId != B.CardId)
		{
			return A.CardId < B.CardId;
		}

		return A.PublicName < B.PublicName;
	});
	return SortedDefinitions;
}

bool IsSupportedTargetRequirement(const EWBCardEffectTargetRequirement TargetRequirement)
{
	switch (TargetRequirement)
	{
	case EWBCardEffectTargetRequirement::None:
	case EWBCardEffectTargetRequirement::Unit:
	case EWBCardEffectTargetRequirement::Tile:
	case EWBCardEffectTargetRequirement::WallEdge:
		return true;
	default:
		return false;
	}
}

const TArray<FString>& GetForbiddenPublicLabelTerms()
{
	static const TArray<FString> ForbiddenTerms =
	{
		TEXT("EffectRunner"),
		TEXT("Rules"),
		TEXT("damage_effect"),
		TEXT("heal_effect"),
		TEXT("armor_effect"),
		TEXT("status_effect"),
		TEXT("effect.type"),
		TEXT("from_tile"),
		TEXT("to_tile"),
		TEXT("chosen_tile"),
		TEXT("player 0"),
		TEXT("player 1"),
		TEXT("player id"),
		TEXT("schema"),
		TEXT("hook")
	};
	return ForbiddenTerms;
}

bool HasDuplicateEffectIds(const FWBCardDefinition& Definition)
{
	TSet<FString> SeenEffectIds;
	for (const FWBCardEffectDefinition& Effect : Definition.ActivatedEffects)
	{
		if (Effect.EffectId.IsEmpty())
		{
			continue;
		}

		if (SeenEffectIds.Contains(Effect.EffectId))
		{
			return true;
		}

		SeenEffectIds.Add(Effect.EffectId);
	}
	return false;
}

bool HasDuplicateTurnStartTriggerIds(
	const FWBCardDefinition& Definition)
{
	TSet<FString> SeenTriggerIds;
	for (const FWBTurnStartTriggerDefinition& Trigger :
		Definition.TurnStartTriggers)
	{
		if (!Trigger.TriggerId.IsEmpty()
			&& SeenTriggerIds.Contains(Trigger.TriggerId))
		{
			return true;
		}
		SeenTriggerIds.Add(Trigger.TriggerId);
	}
	return false;
}

bool HasDuplicateAfterDamageTriggerIds(
	const FWBCardDefinition& Definition)
{
	TSet<FString> SeenTriggerIds;
	for (const FWBAfterDamageTriggerDefinition& Trigger :
		Definition.AfterDamageTriggers)
	{
		if (!Trigger.TriggerId.IsEmpty()
			&& SeenTriggerIds.Contains(Trigger.TriggerId))
		{
			return true;
		}
		SeenTriggerIds.Add(Trigger.TriggerId);
	}
	return false;
}

bool HasDuplicatePreDamageAttackTriggerIds(
	const FWBCardDefinition& Definition)
{
	TSet<FString> SeenTriggerIds;
	for (const FWBPreDamageAttackTriggerDefinition& Trigger :
		Definition.PreDamageAttackTriggers)
	{
		if (!Trigger.TriggerId.IsEmpty()
			&& SeenTriggerIds.Contains(Trigger.TriggerId))
		{
			return true;
		}
		SeenTriggerIds.Add(Trigger.TriggerId);
	}
	return false;
}

bool HasDuplicateAfterCSNInheritanceTriggerIds(
	const FWBCardDefinition& Definition)
{
	TSet<FString> SeenTriggerIds;
	for (const FWBAfterCSNInheritanceTriggerDefinition& Trigger :
		Definition.AfterCSNInheritanceTriggers)
	{
		if (!Trigger.TriggerId.IsEmpty()
			&& SeenTriggerIds.Contains(Trigger.TriggerId))
		{
			return true;
		}
		SeenTriggerIds.Add(Trigger.TriggerId);
	}
	return false;
}

bool HasDuplicateAfterUnitDestroyedTriggerIds(
	const FWBCardDefinition& Definition)
{
	TSet<FString> SeenTriggerIds;
	for (const FWBAfterUnitDestroyedTriggerDefinition& Trigger :
		Definition.AfterUnitDestroyedTriggers)
	{
		if (!Trigger.TriggerId.IsEmpty()
			&& SeenTriggerIds.Contains(Trigger.TriggerId))
		{
			return true;
		}
		SeenTriggerIds.Add(Trigger.TriggerId);
	}
	return false;
}

bool IsSupportedAfterDamageSourceRole(
	const EWBAfterDamageParticipantRole Role)
{
	switch (Role)
	{
	case EWBAfterDamageParticipantRole::Attacker:
	case EWBAfterDamageParticipantRole::HitUnit:
	case EWBAfterDamageParticipantRole::FinalDamageRecipient:
	case EWBAfterDamageParticipantRole::BattleParticipant:
		return true;
	default:
		return false;
	}
}

bool IsSupportedAfterDamageTargetRole(const EWBAfterDamageTargetRole Role)
{
	switch (Role)
	{
	case EWBAfterDamageTargetRole::None:
	case EWBAfterDamageTargetRole::Self:
	case EWBAfterDamageTargetRole::Attacker:
	case EWBAfterDamageTargetRole::HitUnit:
	case EWBAfterDamageTargetRole::FinalDamageRecipient:
	case EWBAfterDamageTargetRole::OpposingBattleUnit:
		return true;
	default:
		return false;
	}
}

bool HasValidKindMetadata(const FWBCardDefinition& Definition, FString& OutReason)
{
	switch (Definition.Kind)
	{
	case EWBCardDefinitionKind::Character:
	case EWBCardDefinitionKind::Hybrid:
	case EWBCardDefinitionKind::NPC:
		if (Definition.CharacterStats.HP <= 0)
		{
			OutReason = TEXT("invalid_character_stats");
			return false;
		}
		if (Definition.CharacterStats.ATK < 0
			|| Definition.CharacterStats.AR < 0
			|| Definition.CharacterStats.RL < 0)
		{
			OutReason = TEXT("invalid_character_stats");
			return false;
		}
		if (Definition.Kind == EWBCardDefinitionKind::Hybrid
			&& (Definition.HybridSummon.SacrificeCount != 1
				|| Definition.HybridSummon.SacrificeRequirement
					!= FName(TEXT("controlled_character"))
				|| Definition.HybridSummon.WandPaymentCount != 1
				|| !Definition.HybridSummon.WandPaymentSources.Contains(
					FName(TEXT("hand")))
				|| !Definition.HybridSummon.WandPaymentSources.Contains(
					FName(TEXT("sacrificed_unit")))
				|| Definition.HybridSummon.HeroDestination
					!= FName(TEXT("sacrificed_hero_tile"))
				|| Definition.HybridSummon.NonHeroDestination
					!= FName(TEXT("adjacent_to_hero"))))
		{
			OutReason = TEXT("invalid_hybrid_summon_definition");
			return false;
		}
		return true;

	case EWBCardDefinitionKind::Wand:
		if (Definition.WandStats.RR < 0)
		{
			OutReason = TEXT("invalid_wand_stats");
			return false;
		}
		return true;

	default:
		return true;
	}
}
}

FWBCardDefinitionRepositoryValidationResult WBCardDefinitionRepository::ValidateRepository(
	const FWBCardDefinitionRepository& Repository)
{
	if (Repository.RepositoryId.IsEmpty())
	{
		return MakeValidationFailure(Repository, TEXT("repository_id_missing"));
	}

	for (const FWBCardDefinition& Definition : Repository.Definitions)
	{
		if (Definition.CardId.IsEmpty())
		{
			return MakeValidationFailure(Repository, TEXT("card_id_missing"));
		}
	}

	FString DuplicateCardId;
	if (HasDuplicateCardIds(Repository, DuplicateCardId))
	{
		return MakeValidationFailure(Repository, TEXT("duplicate_card_id"));
	}

	const TArray<FWBCardDefinition> SortedDefinitions = SortDefinitionsByCardId(Repository.Definitions);
	for (const FWBCardDefinition& Definition : SortedDefinitions)
	{
		if (Definition.PublicName.IsEmpty())
		{
			return MakeValidationFailure(Repository, TEXT("public_name_missing"));
		}

		FString ForbiddenTerm;
		if (ContainsForbiddenPublicLabelTermForTest(Definition.PublicName, ForbiddenTerm))
		{
			return MakeValidationFailure(Repository, TEXT("public_label_contains_internal_term"));
		}

		FString KindMetadataReason;
		if (!HasValidKindMetadata(Definition, KindMetadataReason))
		{
			return MakeValidationFailure(Repository, *KindMetadataReason);
		}

		for (const FWBCardEffectDefinition& Effect : Definition.ActivatedEffects)
		{
			if (Effect.EffectId.IsEmpty())
			{
				return MakeValidationFailure(Repository, TEXT("effect_id_missing"));
			}

			if (!IsSupportedTargetRequirement(Effect.TargetRequirement))
			{
				return MakeValidationFailure(Repository, TEXT("unsupported_target_requirement"));
			}

			if (ContainsForbiddenPublicLabelTermForTest(Effect.PublicLabel, ForbiddenTerm))
			{
				return MakeValidationFailure(Repository, TEXT("public_label_contains_internal_term"));
			}
			for (const FWBGenericEffectPayload& Payload : Effect.Payloads)
			{
				if (Payload.Operation ==
					EWBGenericEffectOp::ReplacePendingAttackDefenderFromHand
					&& (Effect.TargetRequirement
							!= EWBCardEffectTargetRequirement::Unit
						|| Effect.ActivationCondition.AttackDefender
							!= EWBCardEffectAttackDefenderRequirement::OwnCurrentDefender
						|| Effect.ActivationCondition.TargetController
							!= EWBCardEffectTargetControllerRequirement::Self
						|| Payload.RequiredSourceFaction.IsEmpty()
						|| Payload.RequiredReplacementFaction.IsEmpty()
						|| Payload.RequiredReplacementKind
							!= EWBEffectReplacementCardKind::Character
						|| Payload.InheritancePolicy !=
							EWBEffectInheritancePolicy::TransferEquippedWandsAndAddSourceCurrentRL))
				{
					return MakeValidationFailure(
						Repository, TEXT("invalid_unit_replacement_definition"));
				}
				if (Payload.Operation == EWBGenericEffectOp::
					SacrificeSourceThenSummonCharacterFromDeckToSourceTile
					&& (Effect.TargetRequirement
							!= EWBCardEffectTargetRequirement::None
						|| Effect.SourceGate.RequiredZone
							!= EWBCardActivationSourceZone::Board
						|| Effect.SourceGate.Timing
							!= EWBCardActivationTimingRequirement::NormalTurnPriority
						|| !Effect.SourceGate.bRequiresSourceUnit
						|| !Effect.SourceGate.bRequiresSourceUnitOwnership
						|| !Effect.SourceGate.bOncePerTurn
						|| Effect.SourceGate.OncePerTurnKey.IsEmpty()
						|| Payload.RequiredSourceFaction.IsEmpty()
						|| Payload.RequiredReplacementFaction.IsEmpty()
						|| Payload.RequiredReplacementKind
							!= EWBEffectReplacementCardKind::Character
						|| Payload.InheritancePolicy != EWBEffectInheritancePolicy::
							TransferEquippedWandsAndAddSourceCurrentRL))
				{
					return MakeValidationFailure(
						Repository,
						TEXT("invalid_activated_deck_summon_definition"));
				}
				if (Payload.Operation == EWBGenericEffectOp::SetTerrain
					&& (Effect.TargetRequirement
							!= EWBCardEffectTargetRequirement::Tile
						|| Effect.SourceGate.RequiredZone
							!= EWBCardActivationSourceZone::Board
						|| Effect.SourceGate.Timing
							!= EWBCardActivationTimingRequirement::NormalTurnPriority
						|| !Effect.SourceGate.bRequiresSourceUnit
						|| !Effect.SourceGate.bRequiresSourceUnitOwnership
						|| !Effect.SourceGate.bOncePerTurn
						|| Payload.SetTerrainEffect.TerrainId.IsNone()
						|| Payload.SetTerrainEffect.RangeMetric
							!= EWBEffectTileRangeMetric::Manhattan
						|| Payload.SetTerrainEffect.RangeStat
							!= EWBEffectRangeStat::AR
						|| !Payload.SetTerrainEffect.bAllowOccupied
						|| Payload.SetTerrainEffect.bRequireLineOfSight))
				{
					return MakeValidationFailure(
						Repository, TEXT("invalid_set_terrain_definition"));
				}
			}
		}

		if (HasDuplicateEffectIds(Definition))
		{
			return MakeValidationFailure(Repository, TEXT("duplicate_effect_id"));
		}

		for (const FWBTurnStartTriggerDefinition& Trigger :
			Definition.TurnStartTriggers)
		{
			if (Trigger.TriggerId.IsEmpty())
			{
				return MakeValidationFailure(
					Repository,
					TEXT("turn_start_trigger_id_missing"));
			}
			if (!IsSupportedTargetRequirement(
				Trigger.TargetRequirement))
			{
				return MakeValidationFailure(
					Repository,
					TEXT("unsupported_target_requirement"));
			}
			if (Trigger.DrawCount < 0
				|| (Trigger.DrawCount == 0
					&& Trigger.Payloads.IsEmpty()))
			{
				return MakeValidationFailure(
					Repository,
					TEXT("turn_start_trigger_effect_missing"));
			}
			if (!Trigger.Payloads.IsEmpty()
				&& Trigger.TargetRequirement
					!= EWBCardEffectTargetRequirement::Unit)
			{
				return MakeValidationFailure(
					Repository,
					TEXT("turn_start_trigger_target_required"));
			}
		}
		if (HasDuplicateTurnStartTriggerIds(Definition))
		{
			return MakeValidationFailure(
				Repository,
				TEXT("duplicate_turn_start_trigger_id"));
		}

		for (const FWBAfterDamageTriggerDefinition& Trigger :
			Definition.AfterDamageTriggers)
		{
			if (Trigger.TriggerId.IsEmpty())
			{
				return MakeValidationFailure(
					Repository,
					TEXT("after_damage_trigger_id_missing"));
			}
			if (!IsSupportedAfterDamageSourceRole(Trigger.SourceRole))
			{
				return MakeValidationFailure(
					Repository,
					TEXT("after_damage_source_role_unsupported"));
			}
			if (!IsSupportedAfterDamageTargetRole(Trigger.TargetRole))
			{
				return MakeValidationFailure(
					Repository,
					TEXT("after_damage_target_role_unsupported"));
			}
			if (!Trigger.bMandatory)
			{
				return MakeValidationFailure(
					Repository,
					TEXT("optional_after_damage_trigger_unsupported"));
			}
			if (Trigger.Payloads.IsEmpty())
			{
				return MakeValidationFailure(
					Repository,
					TEXT("after_damage_trigger_payloads_missing"));
			}
		}
		if (HasDuplicateAfterDamageTriggerIds(Definition))
		{
			return MakeValidationFailure(
				Repository,
				TEXT("duplicate_after_damage_trigger_id"));
		}

		for (const FWBPreDamageAttackTriggerDefinition& Trigger :
			Definition.PreDamageAttackTriggers)
		{
			if (Trigger.TriggerId.IsEmpty())
			{
				return MakeValidationFailure(
					Repository, TEXT("pre_damage_attack_trigger_id_missing"));
			}
			if (Trigger.SourceRole
					!= EWBPreDamageAttackTriggerSourceRole::CurrentDefender
				|| Trigger.Timing
					!= EWBPreDamageAttackTriggerTiming::AfterPreHitBeforeCalculateDamage
				|| Trigger.RandomBranch
					!= EWBDeterministicRandomBranchKind::CoinFlip
				|| !Trigger.bMandatory
				|| !Trigger.bOncePerTurn
				|| Trigger.Heads.Operation
					!= EWBPendingBattleHitModifierOperation::ReflectToAttacker
				|| Trigger.Heads.Amount != 0
				|| Trigger.Tails.Operation
					!= EWBPendingBattleHitModifierOperation::AddRawDamage
				|| Trigger.Tails.Amount <= 0)
			{
				return MakeValidationFailure(
					Repository, TEXT("pre_damage_attack_trigger_unsupported"));
			}
		}
		if (HasDuplicatePreDamageAttackTriggerIds(Definition))
		{
			return MakeValidationFailure(
				Repository, TEXT("duplicate_pre_damage_attack_trigger_id"));
		}

		for (const FWBAfterCSNInheritanceTriggerDefinition& Trigger :
			Definition.AfterCSNInheritanceTriggers)
		{
			if (Trigger.TriggerId.IsEmpty())
			{
				return MakeValidationFailure(
					Repository,
					TEXT("csn_inheritance_trigger_id_missing"));
			}
			if (!Trigger.bMandatory)
			{
				return MakeValidationFailure(
					Repository,
					TEXT("optional_csn_inheritance_trigger_unsupported"));
			}
			if (Trigger.DrawCount <= 0)
			{
				return MakeValidationFailure(
					Repository,
					TEXT("csn_inheritance_trigger_effect_missing"));
			}
		}
		if (HasDuplicateAfterCSNInheritanceTriggerIds(Definition))
		{
			return MakeValidationFailure(
				Repository,
				TEXT("duplicate_csn_inheritance_trigger_id"));
		}

		for (const FWBAfterUnitDestroyedTriggerDefinition& Trigger :
			Definition.AfterUnitDestroyedTriggers)
		{
			if (Trigger.TriggerId.IsEmpty())
			{
				return MakeValidationFailure(
					Repository, TEXT("after_unit_destroyed_trigger_id_missing"));
			}
			if (!Trigger.bMandatory)
			{
				return MakeValidationFailure(
					Repository, TEXT("optional_after_unit_destroyed_trigger_unsupported"));
			}
			const bool bValidDeckSummon =
				Trigger.SourceScope == EWBAfterUnitDestroyedSourceScope::DestroyedSelf
				&& Trigger.Operation == EWBPostDestructionEffectOperation::
					SummonCharacterFromDeckToDestroyedTile
				&& Trigger.SummonCount == 1
				&& Trigger.bIgnoreSummoningConditions
				&& Trigger.bApplyCSNInheritance;
			const bool bValidObserverStatDelta =
				Trigger.SourceScope == EWBAfterUnitDestroyedSourceScope::
					ControlledFactionUnitDestroyed
				&& Trigger.Operation == EWBPostDestructionEffectOperation::
					ApplyPersistentStatDeltaToTriggerSource
				&& Trigger.Target == EWBPostDestructionTarget::TriggerSource
				&& (Trigger.StatDelta.ATKDelta != 0
					|| Trigger.StatDelta.MaxHPDelta != 0
					|| Trigger.StatDelta.CurrentHPDelta != 0);
			if (Trigger.RequiredFaction.IsEmpty()
				|| (!bValidDeckSummon && !bValidObserverStatDelta))
			{
				return MakeValidationFailure(
					Repository, TEXT("after_unit_destroyed_trigger_invalid"));
			}
		}
		if (HasDuplicateAfterUnitDestroyedTriggerIds(Definition))
		{
			return MakeValidationFailure(
				Repository, TEXT("duplicate_after_unit_destroyed_trigger_id"));
		}

		TSet<FString> ContinuousAuraIds;
		for (const FWBContinuousStatAuraDefinition& Aura :
			Definition.ContinuousStatAuras)
		{
			if (Aura.AuraId.IsEmpty()
				|| ContinuousAuraIds.Contains(Aura.AuraId))
			{
				return MakeValidationFailure(
					Repository, TEXT("continuous_stat_aura_id_invalid"));
			}
			if (Aura.TargetRelation
					!= EWBContinuousAuraTargetRelation::Enemy
				|| Aura.TargetStat != EWBContinuousStat::AR
				|| Aura.Operation != EWBContinuousStatOperation::Add
				|| Aura.Amount == 0
				|| Aura.RangeStat != EWBContinuousAuraRangeStat::AR
				|| Aura.Geometry != EWBContinuousAuraGeometry::AttackLine
				|| Aura.MinimumResult < 0)
			{
				return MakeValidationFailure(
					Repository, TEXT("continuous_stat_aura_unsupported"));
			}
			ContinuousAuraIds.Add(Aura.AuraId);
		}
	}

	return MakeValidationSuccess(Repository);
}

FWBCardDefinitionRepositoryValidationResult WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
	const FString& RepositoryId,
	const FString& SourceVersion,
	const TArray<FWBCardDefinition>& Definitions,
	FWBCardDefinitionRepository& OutRepository)
{
	FWBCardDefinitionRepository CandidateRepository;
	CandidateRepository.RepositoryId = RepositoryId;
	CandidateRepository.SourceVersion = SourceVersion;
	CandidateRepository.Definitions = Definitions;

	const FWBCardDefinitionRepositoryValidationResult Result = ValidateRepository(CandidateRepository);
	if (Result.bOk)
	{
		OutRepository = CandidateRepository;
	}
	else
	{
		OutRepository = FWBCardDefinitionRepository();
	}
	return Result;
}

bool WBCardDefinitionRepository::ContainsCardId(
	const FWBCardDefinitionRepository& Repository,
	const FString& CardId)
{
	return FindCardById(Repository, CardId).bFound;
}

FWBCardDefinitionRepositoryLookupResult WBCardDefinitionRepository::FindCardById(
	const FWBCardDefinitionRepository& Repository,
	const FString& CardId)
{
	FWBCardDefinitionRepositoryLookupResult Result;
	if (CardId.IsEmpty())
	{
		Result.Reason = TEXT("card_id_missing");
		return Result;
	}

	const TArray<FWBCardDefinition> SortedDefinitions = SortDefinitionsByCardId(Repository.Definitions);
	for (const FWBCardDefinition& Definition : SortedDefinitions)
	{
		if (Definition.CardId == CardId)
		{
			Result.bFound = true;
			Result.Definition = Definition;
			return Result;
		}
	}

	Result.Reason = TEXT("card_definition_not_found");
	return Result;
}

TArray<FString> WBCardDefinitionRepository::GetCardIdsInDeterministicOrder(
	const FWBCardDefinitionRepository& Repository)
{
	TArray<FString> CardIds;
	for (const FWBCardDefinition& Definition : SortDefinitionsByCardId(Repository.Definitions))
	{
		CardIds.Add(Definition.CardId);
	}
	return CardIds;
}

TArray<FWBCardDefinition> WBCardDefinitionRepository::GetDefinitionsInDeterministicOrder(
	const FWBCardDefinitionRepository& Repository)
{
	return SortDefinitionsByCardId(Repository.Definitions);
}

bool WBCardDefinitionRepository::HasDuplicateCardIds(
	const FWBCardDefinitionRepository& Repository,
	FString& OutDuplicateCardId)
{
	OutDuplicateCardId.Reset();

	TSet<FString> SeenCardIds;
	const TArray<FWBCardDefinition> SortedDefinitions = SortDefinitionsByCardId(Repository.Definitions);
	for (const FWBCardDefinition& Definition : SortedDefinitions)
	{
		if (Definition.CardId.IsEmpty())
		{
			continue;
		}

		if (SeenCardIds.Contains(Definition.CardId))
		{
			OutDuplicateCardId = Definition.CardId;
			return true;
		}

		SeenCardIds.Add(Definition.CardId);
	}

	return false;
}

bool WBCardDefinitionRepository::ContainsForbiddenPublicLabelTermForTest(
	const FString& Text,
	FString& OutForbiddenTerm)
{
	OutForbiddenTerm.Reset();
	if (Text.IsEmpty())
	{
		return false;
	}

	for (const FString& ForbiddenTerm : GetForbiddenPublicLabelTerms())
	{
		if (Text.Contains(ForbiddenTerm, ESearchCase::IgnoreCase))
		{
			OutForbiddenTerm = ForbiddenTerm;
			return true;
		}
	}

	return false;
}
