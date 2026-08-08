#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionHybridReplacementSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 OldHeroUnitId = -1;
	int32 NewHeroUnitId = -1;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionHybridReplacementSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionHybridReplacementSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
