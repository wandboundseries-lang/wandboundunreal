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

bool HasValidKindMetadata(const FWBCardDefinition& Definition, FString& OutReason)
{
	switch (Definition.Kind)
	{
	case EWBCardDefinitionKind::Character:
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
		}

		if (HasDuplicateEffectIds(Definition))
		{
			return MakeValidationFailure(Repository, TEXT("duplicate_effect_id"));
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
