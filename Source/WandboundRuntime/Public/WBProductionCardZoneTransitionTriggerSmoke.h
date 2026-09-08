#pragma once

#include "CoreMinimal.h"

struct WANDBOUNDRUNTIME_API FWBProductionCardZoneTransitionTriggerSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 TriggerCount = 0;
	int32 TransitionCount = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionCardZoneTransitionTriggerSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionCardZoneTransitionTriggerSmokeResult Run();
};
