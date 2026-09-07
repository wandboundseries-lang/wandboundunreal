#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionCardZoneTransitionSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 TransitionCount = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionCardZoneTransitionSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionCardZoneTransitionSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
