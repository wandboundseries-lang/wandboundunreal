#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionAfterDamageTriggerSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 RecordsVerified = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionAfterDamageTriggerSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionAfterDamageTriggerSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
