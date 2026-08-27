#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionMarrowBlackcoinBouncerSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 RecordsVerified = 0;
	int32 FinalGeneration = 0;
	int32 FinalRevision = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
	FString SerializedArchive;
	FString SerializedReceipt;
};

class WANDBOUNDRUNTIME_API WBProductionMarrowBlackcoinBouncerSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionMarrowBlackcoinBouncerSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
