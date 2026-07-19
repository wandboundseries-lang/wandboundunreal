#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBNPCPhaseResolutionResult
{
	bool bOk = false;
	FString Reason;
	int32 SpawnedCount = 0;
	int32 BlockedSpawnCount = 0;
	int32 EligibleNPCCount = 0;
	int32 CompletedNPCCount = 0;
	TArray<int32> MPRolls;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBNPCPhaseResolution
{
public:
	static FWBNPCPhaseResolutionResult ResolvePhase(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		uint32& InOutRandomState,
		int32 PhaseOwnerPlayerId);

	static int32 RollD6(uint32& InOutRandomState);
};
