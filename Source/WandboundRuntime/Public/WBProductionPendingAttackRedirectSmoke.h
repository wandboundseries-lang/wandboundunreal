#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionPendingAttackRedirectSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 RecordsVerified = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionPendingAttackRedirectSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionPendingAttackRedirectSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
