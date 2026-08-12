#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionSuspendedAttackSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 RecordsVerified = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionSuspendedAttackSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionSuspendedAttackSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
