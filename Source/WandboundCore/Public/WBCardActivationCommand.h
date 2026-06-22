#pragma once

#include "CoreMinimal.h"
#include "WBEffectRequest.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBCardActivationSource
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	FString SourceCardId;
	FString SourceEffectId;
};

struct WANDBOUNDCORE_API FWBCardActivationCommand
{
	FWBCardActivationSource Source;
	FWBEffectRequest EffectRequest;
	FString DebugActivationId;
};

struct WANDBOUNDCORE_API FWBCardActivationCommandResult
{
	bool bOk = false;
	FString Reason;
	FWBCardActivationCommand Command;
	FWBEffectRequestResult EffectResult;
	TArray<FWBTraceEvent> TraceEvents;
};
