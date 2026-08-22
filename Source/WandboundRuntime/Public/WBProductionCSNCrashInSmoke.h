#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionCSNCrashInSmokeResult
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

class WANDBOUNDRUNTIME_API WBProductionCSNCrashInSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static bool IsUndertowRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FString GetUndertowReceiptPath();
	static FWBProductionCSNCrashInSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
	static FWBProductionCSNCrashInSmokeResult RunUndertow(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
	static FWBProductionCSNCrashInSmokeResult RunUndertowNegatedForTest(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);

private:
	static FWBProductionCSNCrashInSmokeResult RunScenario(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest,
		bool bUndertow,
		bool bNegateCrashIn);
};
