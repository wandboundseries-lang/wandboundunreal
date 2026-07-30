#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionStartupResult
{
	int32 SchemaVersion = 1;
	FString StartupMode = TEXT("production");
	bool bBundleLoaded = false;
	FString BundleDigest;
	bool bMatchSpecPresent = false;
	bool bMatchInitialized = false;
	bool bPlayableDecisionReached = false;
	bool bBlocked = false;
	FString ResultCode;
	int32 Generation = 0;
	int32 Revision = 0;
};

class WANDBOUNDRUNTIME_API WBProductionStartupResult
{
public:
	static FString GetDefaultResultPath();

	static FWBProductionStartupResult FromBootstrap(
		const FWBProductionRuntimeBootstrapRequest& Request,
		const FWBProductionRuntimeBootstrapResult& Bootstrap);

	static FWBProductionStartupResult Started(
		const FString& BundleDigest,
		const bool bMatchSpecPresent,
		const int32 Generation,
		const int32 Revision,
		const bool bPlayableDecisionReached);

	static FString Serialize(const FWBProductionStartupResult& Result);

	static bool Write(
		const FWBProductionStartupResult& Result,
		const FString& ResultPath = FString());

	static int32 ExitCodeForResult(
		const FWBProductionStartupResult& Result);

	static bool IsStartupProbeRequested(
		const TCHAR* CommandLine);

	static void RequestProbeExit(
		const FWBProductionStartupResult& Result);
};
