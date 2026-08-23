#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBUnitStatDeltaRequest
{
	int32 SourceUnitId = INDEX_NONE;
	int32 TargetUnitId = INDEX_NONE;
	int32 ATKDelta = 0;
	int32 MaxHPDelta = 0;
	int32 CurrentHPDelta = 0;
	FString TransactionId;
};

struct WANDBOUNDCORE_API FWBUnitStatDeltaResult
{
	bool bOk = false;
	FString Reason;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBUnitStatDelta
{
public:
	static FWBUnitStatDeltaResult ApplyPersistentDelta(
		FWBGameStateData& State,
		const FWBUnitStatDeltaRequest& Request);
};
