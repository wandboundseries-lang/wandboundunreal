#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBPublicBattleSummary
{
	bool bActive = false;
	int32 DeclaringPlayerId = INDEX_NONE;
	int32 OriginalAttackerUnitId = INDEX_NONE;
	int32 OriginalDefenderUnitId = INDEX_NONE;
};

// Coordinator-owned envelope; PendingAttack remains the mutable combat authority.
class WANDBOUNDCORE_API WBBattlePhase
{
public:
	static bool Validate(const FWBGameStateData& State, FString& OutReason);
	static bool Begin(FWBGameStateData& State, int32 Generation, int32 Revision,
		TArray<FWBTraceEvent>& OutEvents, FString& OutReason);
	static void End(FWBGameStateData& State, TArray<FWBTraceEvent>& OutEvents);
	static FWBPublicBattleSummary BuildPublicSummary(const FWBGameStateData& State);
};
