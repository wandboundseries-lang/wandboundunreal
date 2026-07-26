#pragma once

#include "CoreMinimal.h"
#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"

struct WANDBOUNDCARDDB_API FWBProductionPlayerMatchSpecification
{
	int32 PlayerId = -1;
	FString HeroDefinitionId;
	TArray<FString> OrderedDeckDefinitionIds;
};

struct WANDBOUNDCARDDB_API FWBProductionMatchSpecification
{
	int32 SchemaVersion = 0;
	FString MatchId;
	int32 Seed = 0;
	int32 FirstPlayerId = -1;
	int32 InitialDrawCount = 0;
	FString DefinitionBundleDigest;
	TArray<FWBProductionPlayerMatchSpecification> Players;
	TArray<FWBSetupMarkerPlacement> MarkerPlacements;
};

struct WANDBOUNDCARDDB_API FWBProductionMatchSpecificationLoadResult
{
	bool bOk = false;
	FString Reason;
	FString MatchSpecPath;
	FWBProductionMatchSpecification Specification;
	FWBMatchInitializationRequest InitializationRequest;
	TArray<FWBProductionCardDBDiagnostic> Diagnostics;
};

class WANDBOUNDCARDDB_API WBProductionMatchSpecification
{
public:
	static FWBProductionMatchSpecificationLoadResult LoadAndBuildRequest(
		const FString& MatchSpecPath,
		const FWBProductionCardDatabase& Database);

	static FWBProductionMatchSpecificationLoadResult ParseAndBuildRequestForTest(
		const FString& Json,
		const FString& MatchSpecPath,
		const FWBProductionCardDatabase& Database);
};
