#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct FWBExpectedActivationCostPaymentState
{
	int32 SourceUnitId = -1;
	int32 ExpectedRLTotal = 0;
	int32 ExpectedRLUsed = 0;
	int32 ExpectedAvailableRL = 0;
};

class FWBCardActivationCostPaymentVerifier
{
public:
	static bool VerifyUnitRLState(
		const FWBGameStateData& State,
		const FWBExpectedActivationCostPaymentState& Expected,
		FString& OutReason);

	static bool VerifyCostPaidTrace(
		const TArray<FWBTraceEvent>& Traces,
		int32 ExpectedPlayerId,
		int32 ExpectedSourceUnitId,
		int32 ExpectedCostAmount,
		int32 ExpectedPreviousRLUsed,
		int32 ExpectedNewRLUsed,
		int32 ExpectedAvailableRLBefore,
		int32 ExpectedAvailableRLAfter,
		FString& OutReason);

	static bool ContainsTraceKind(
		const TArray<FWBTraceEvent>& Traces,
		FName ExpectedKind);

	static bool TraceJsonExcludesForbiddenSubstrings(
		const TArray<FWBTraceEvent>& Traces,
		const TArray<FString>& ForbiddenSubstrings,
		FString& OutReason);
};
