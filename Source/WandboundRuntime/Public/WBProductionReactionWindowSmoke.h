#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionReactionWindowSmokeResult
{
	bool bOk = false;
	FString Reason;
	FString ReactionActionId;
	int32 RecordsVerified = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionReactionWindowSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionReactionWindowSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
