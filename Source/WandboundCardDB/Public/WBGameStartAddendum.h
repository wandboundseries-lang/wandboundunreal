#pragma once

#include "CoreMinimal.h"

struct WANDBOUNDCARDDB_API FWBGameStartAddendumV1
{
	int32 SchemaVersion = 0;
	FString AddendumId;
	int32 AddendumVersion = 0;
	FString Authority;
	FString ApprovalDate;
	FString Digest;
	TArray<FString> SupersedesRules;
	TArray<FString> SetupSequence;
	bool bAtomicHeroSpawn = false;
	bool bHeroPlacementCountsAsSummon = false;
	bool bCollectTriggersAfterBatchCommit = false;
	bool bFirstPlayerTriggerBatchFirst = false;
	bool bPlayerSelectedTriggerOrder = false;
	bool bStableReplayDecisionIds = false;
	bool bManualReactsAllowedDuringSetup = true;
	bool bPriorityPassingAllowedDuringSetup = true;
	bool bRequiredChoicesContinue = false;
	bool bSetupEffectDrawsBeforeOpeningHand = false;
	int32 OpeningHandCount = 0;
	bool bHeroSpawnTilesReserved = false;
	int32 BoardSize = 0;
	int32 NeutralRow = -1;
	bool bFirstPlayerTurnOneOnly = false;
	bool bNeutralRowSummonsAllowed = false;
	bool bOpponentHalfSummonsAllowed = true;
	bool bProtectedBoundaryCrossingAllowed = true;
	bool bAllRelocationOperationsCovered = false;
	bool bNeutralNPCAttacksAllowed = false;
	bool bOpponentControlledAttacksAllowed = true;
	FString SetupTriggerDrawReason;
	FString OpeningHandDrawReason;
	TArray<FString> Provenance;
};

struct WANDBOUNDCARDDB_API FWBGameStartAddendumLoadResult
{
	bool bOk = false;
	FString Reason;
	FString SourcePath;
	FWBGameStartAddendumV1 Addendum;
};

class WANDBOUNDCARDDB_API WBGameStartAddendum
{
public:
	static FWBGameStartAddendumLoadResult Load(const FString& Path);
	static FWBGameStartAddendumLoadResult ParseForTest(
		const FString& Json,
		const FString& SourcePath);
	static FString ComputeDigest(
		const FWBGameStartAddendumV1& Addendum);
};
