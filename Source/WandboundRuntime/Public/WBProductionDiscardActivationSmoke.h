#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionDiscardActivationSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 ScenariosVerified = 0;
	int32 RecordsVerified = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionDiscardActivationSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionDiscardActivationSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
