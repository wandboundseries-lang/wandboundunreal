#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionTerminalReplaySmokeResult
{
	bool bOk = false;
	FString Reason;
};

class WANDBOUNDRUNTIME_API WBProductionTerminalReplaySmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionTerminalReplaySmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
