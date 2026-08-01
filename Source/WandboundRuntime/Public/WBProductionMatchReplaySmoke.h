#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionMatchReplaySmokeResult
{
	bool bOk = false;
	FString Reason;
};

class WANDBOUNDRUNTIME_API WBProductionMatchReplaySmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionMatchReplaySmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
