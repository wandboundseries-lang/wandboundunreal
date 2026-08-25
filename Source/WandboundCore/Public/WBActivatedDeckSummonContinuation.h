#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCommand.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBActivatedDeckSummonContinuationResult
{
	bool bOk = false;
	FString Reason;
	bool bHandled = false;
	bool bPendingChoice = false;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBActivatedDeckSummonContinuation
{
public:
	static FWBActivatedDeckSummonContinuationResult Resolve(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBCardActivationCommand& Command,
		const FString& ActivationActionId,
		const FString& PendingEffectFrameId,
		int32 ResumePriorityPlayerId,
		int32 ResumeMatchPhase);
};
