#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionCSNBodyDoubleSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 RecordsVerified = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionCSNBodyDoubleSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionCSNBodyDoubleSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
