#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBPreDamageAttackTriggerResult
{
	bool bOk = false;
	FString Reason;
	int32 ResolvedTriggerCount = 0;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBPreDamageAttackTrigger
{
public:
	static FWBPreDamageAttackTriggerResult Resolve(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		uint32& InOutRandomState);

	static FString BuildUsageKey(
		int32 SourceUnitId,
		const FString& TriggerId,
		int32 TurnNumber);
};
