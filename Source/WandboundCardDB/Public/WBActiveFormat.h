#pragma once

#include "CoreMinimal.h"
#include "WBProductionCardDatabase.h"

struct WANDBOUNDCARDDB_API FWBActiveFormatV1
{
	int32 SchemaVersion = 0;
	FString FormatId;
	int32 FormatVersion = 0;
	FString Authority;
	FString ApprovalDate;
	FString Digest;
	TArray<FString> SourceProvenance;
	int32 MinimumMainDeckSize = 0;
	int32 MaximumMainDeckSize = 0;
	bool bUniqueMainDeckDefinitions = false;
	bool bRequiresNonHybridCharacter = false;
	TArray<FString> LegalMainDeckCategories;
	TArray<FString> ExcludedMainDeckCategories;
	int32 SetupTrapCount = 0;
	int32 SetupNPCCount = 0;
	bool bSetupDefinitionsMayRepeat = false;
	int32 OpeningHandCount = 0;
	bool bMirroredDecksAllowed = false;
	bool bSeededShuffleRequired = false;
	bool bSeededFirstPlayerCoinFlipRequired = false;
	FString FirstPlayerPolicy;
	bool bFirstPlayerTurnOneRestriction = false;
	bool bNeutralRowSummonsAllowed = false;
	bool bOpponentHalfSummonsAllowed = true;
	bool bProtectedRegionBoundaryCrossingAllowed = true;
	bool bNeutralNPCAttacksAllowed = false;
	bool bOpponentControlledAttacksAllowed = true;
	FString GameStartAddendumId;
	int32 GameStartAddendumVersion = 0;
};

struct WANDBOUNDCARDDB_API FWBActiveFormatLoadResult
{
	bool bOk = false;
	FString Reason;
	FString SourcePath;
	FWBActiveFormatV1 Format;
};

struct WANDBOUNDCARDDB_API FWBActiveFormatPlayerInput
{
	int32 PlayerId = -1;
	TArray<FString> MainDeckDefinitionIds;
	FString HeroDefinitionId;
	TArray<FString> SetupTrapDefinitionIds;
	TArray<FString> SetupNPCDefinitionIds;
};

struct WANDBOUNDCARDDB_API FWBActiveFormatValidationResult
{
	bool bOk = false;
	FString Reason;
	TArray<FString> Diagnostics;
};

class WANDBOUNDCARDDB_API WBActiveFormat
{
public:
	static FWBActiveFormatLoadResult Load(const FString& Path);
	static FWBActiveFormatLoadResult ParseForTest(
		const FString& Json,
		const FString& SourcePath);

	static FWBActiveFormatValidationResult ValidateStoredMainDeck(
		const FWBActiveFormatV1& Format,
		const FWBProductionCardDatabase& Database,
		const TArray<FString>& MainDeckDefinitionIds);

	static FWBActiveFormatValidationResult ValidatePlayerForLaunch(
		const FWBActiveFormatV1& Format,
		const FWBProductionCardDatabase& Database,
		const FWBActiveFormatPlayerInput& Player);

	static FWBActiveFormatValidationResult ValidateMatchForLaunch(
		const FWBActiveFormatV1& Format,
		const FWBProductionCardDatabase& Database,
		const TArray<FWBActiveFormatPlayerInput>& Players);

	static FString ComputeDigest(const FWBActiveFormatV1& Format);
};
