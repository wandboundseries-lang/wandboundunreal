#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinition.h"

struct WANDBOUNDCORE_API FWBCardDefinitionRepository
{
	FString RepositoryId;
	FString SourceVersion;
	TArray<FWBCardDefinition> Definitions;
};

struct WANDBOUNDCORE_API FWBCardDefinitionRepositoryValidationResult
{
	bool bOk = false;
	FString Reason;
	int32 DefinitionCount = 0;
};

struct WANDBOUNDCORE_API FWBCardDefinitionRepositoryLookupResult
{
	bool bFound = false;
	FString Reason;
	FWBCardDefinition Definition;
};

class WANDBOUNDCORE_API WBCardDefinitionRepository
{
public:
	static FWBCardDefinitionRepositoryValidationResult ValidateRepository(
		const FWBCardDefinitionRepository& Repository);

	static FWBCardDefinitionRepositoryValidationResult BuildRepositoryFromDefinitions(
		const FString& RepositoryId,
		const FString& SourceVersion,
		const TArray<FWBCardDefinition>& Definitions,
		FWBCardDefinitionRepository& OutRepository);

	static bool ContainsCardId(
		const FWBCardDefinitionRepository& Repository,
		const FString& CardId);

	static FWBCardDefinitionRepositoryLookupResult FindCardById(
		const FWBCardDefinitionRepository& Repository,
		const FString& CardId);

	static TArray<FString> GetCardIdsInDeterministicOrder(
		const FWBCardDefinitionRepository& Repository);

	static TArray<FWBCardDefinition> GetDefinitionsInDeterministicOrder(
		const FWBCardDefinitionRepository& Repository);

	static bool HasDuplicateCardIds(
		const FWBCardDefinitionRepository& Repository,
		FString& OutDuplicateCardId);

	static bool ContainsForbiddenPublicLabelTermForTest(
		const FString& Text,
		FString& OutForbiddenTerm);
};
