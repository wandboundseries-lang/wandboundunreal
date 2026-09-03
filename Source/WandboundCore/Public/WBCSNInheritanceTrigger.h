#pragma once

#include "CoreMinimal.h"

#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBCSNInheritanceEventContext
{
	FWBEventIdentitySnapshot EventIdentity;
	FWBUnitParticipantSnapshot SourceSnapshot;
	FWBUnitParticipantSnapshot InheritingSnapshot;
	EWBTriggerEligibilityPolicy EligibilityPolicy =
		EWBTriggerEligibilityPolicy::Hybrid;
	int32 InheritingUnitId = INDEX_NONE;
	int32 InheritingPlayerId = INDEX_NONE;
	int32 SourceUnitId = INDEX_NONE;
	int32 SourceCurrentRL = 0;
	int32 InheritedWandCount = 0;
	FString TransactionId;
};

struct WANDBOUNDCORE_API FWBCSNInheritanceTriggerResult
{
	bool bOk = false;
	FString Reason;
	int32 ResolvedTriggerCount = 0;
	int32 DrawnCardCount = 0;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBCSNInheritanceTrigger
{
public:
	static FWBCSNInheritanceTriggerResult ResolveAfterSuccessfulInheritance(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBCSNInheritanceEventContext& Context);
};
