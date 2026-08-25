#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBMandatoryDeckChoiceResult
{
	bool bOk = false;
	FString Reason;
	bool bPendingChoice = false;
	bool bSummoned = false;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBMandatoryDeckChoice
{
public:
	static FString BuildActionId(
		const FWBPendingMandatoryDeckChoiceState& Choice,
		const FString& CardInstanceId);

	static TArray<FString> EnumerateLegalActionIds(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository);

	static FWBMandatoryDeckChoiceResult Submit(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FString& ActionId);
};
