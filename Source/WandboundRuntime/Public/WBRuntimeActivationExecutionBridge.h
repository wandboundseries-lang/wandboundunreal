#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBRuntimeActivationExecutionHandoff.h"

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationExecutionResult
{
	bool bOk = false;
	bool bExecutionAttempted = false;
	bool bActivationResolved = false;

	FString Reason;
	FString SelectedActivationActionId;

	FWBRuntimeActivationExecutionHandoffResult Handoff;
	FWBCardActivationCommandResult ActivationResult;
};

class WANDBOUNDRUNTIME_API WBRuntimeActivationExecutionBridge
{
public:
	static FWBRuntimeActivationExecutionResult ExecuteResolvedActivationHandoff(
		FWBGameStateData& State,
		const FWBRuntimeActivationExecutionHandoffResult& Handoff);
};
