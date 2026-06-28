#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"

struct WANDBOUNDCORE_API FWBCardDefinitionFixtureLoadDiagnostic
{
	FString Code;
	FString CardId;
	FString EffectId;
	FString Path;
};

struct WANDBOUNDCORE_API FWBCardDefinitionFixtureLoadResult
{
	bool bOk = false;
	FString Reason;
	FString SourcePath;

	FWBCardDefinitionRepository Repository;
	TArray<FWBCardDefinitionFixtureLoadDiagnostic> Diagnostics;
};

class WANDBOUNDCORE_API WBCardDefinitionFixtureLoader
{
public:
	static FWBCardDefinitionFixtureLoadResult LoadRepositoryFromJsonString(
		const FString& Json,
		const FString& SourcePathForDiagnostics);

	static FWBCardDefinitionFixtureLoadResult LoadRepositoryFromFile(
		const FString& AbsolutePath);

	static bool FixtureLoadResultToJsonStringForTest(
		const FWBCardDefinitionFixtureLoadResult& Result,
		FString& OutJson);
};
