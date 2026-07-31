#pragma once

#include "CoreMinimal.h"
#include "WBMatchCoordinator.h"
#include "WBActiveFormat.h"
#include "WBGameStartAddendum.h"
#include "WBProductionCardDatabase.h"

struct WANDBOUNDCARDDB_API FWBProductionPlayerMatchSpecification
{
	int32 PlayerId = -1;
	FString HeroDefinitionId;
	TArray<FString> OrderedDeckDefinitionIds;
	TArray<FString> SetupTrapDefinitionIds;
	TArray<FString> SetupNPCDefinitionIds;
	FWBTile HeroSpawnTile;
};

struct WANDBOUNDCARDDB_API FWBProductionMatchSpecification
{
	int32 SchemaVersion = 0;
	FString MatchId;
	int32 Seed = 0;
	int32 FirstPlayerId = -1;
	int32 InitialDrawCount = 0;
	FString DefinitionBundleDigest;
	FString ActiveFormatId;
	int32 ActiveFormatVersion = 0;
	FString ActiveFormatDigest;
	FString GameStartAddendumId;
	int32 GameStartAddendumVersion = 0;
	FString GameStartAddendumDigest;
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

	static FWBProductionMatchSpecificationLoadResult LoadAndBuildRequestV2(
		const FString& MatchSpecPath,
		const FWBProductionCardDatabase& Database,
		const FWBActiveFormatV1& Format,
		const FWBGameStartAddendumV1& Addendum);

	static FWBProductionMatchSpecificationLoadResult ParseAndBuildRequestV2ForTest(
		const FString& Json,
		const FString& MatchSpecPath,
		const FWBProductionCardDatabase& Database,
		const FWBActiveFormatV1& Format,
		const FWBGameStartAddendumV1& Addendum);
};
