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
	bool bPausedForAttack = false;
	bool bCompleted = false;
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

	static FWBNPCPhaseResolutionResult BeginPhase(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 PhaseOwnerPlayerId);

	static FWBNPCPhaseResolutionResult AdvanceUntilAttackOrComplete(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		uint32& InOutRandomState);

	static int32 RollD6(uint32& InOutRandomState);
};
