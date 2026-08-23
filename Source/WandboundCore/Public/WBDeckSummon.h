#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBDeckSummonRequest
{
	int32 PlayerId = INDEX_NONE;
	FString SelectedCardInstanceId;
	FString RequiredFaction;
	FWBTile TargetTile;
	FWBUnitDestructionSnapshot InheritanceSource;
	FString TransactionId;
};

struct WANDBOUNDCORE_API FWBDeckSummonResult
{
	bool bOk = false;
	FString Reason;
	int32 CreatedUnitId = INDEX_NONE;
	FString SummonedCardId;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBDeckSummon
{
public:
	static FWBDeckSummonResult SummonExactCharacterToTile(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBDeckSummonRequest& Request);
};
