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

struct WANDBOUNDCORE_API FWBCardActivationUsageCommit
{
	bool bMarkUsageOnSuccess = false;
	int32 PlayerId = -1;
	FString UsageKey;
};

struct WANDBOUNDCORE_API FWBCardActivationCostPaymentCommit
{
	bool bPayCostOnSuccess = false;
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	int32 RequiredRR = 0;
	FName CostKind;
};

struct WANDBOUNDCORE_API FWBCardActivationCommand
{
	FWBCardActivationSource Source;
	FWBEffectRequest EffectRequest;
	FWBCardActivationCostPaymentCommit CostPaymentCommit;
	FWBCardActivationUsageCommit UsageCommit;
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
