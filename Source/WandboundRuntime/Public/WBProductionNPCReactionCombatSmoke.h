#pragma once

#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionNPCReactionCombatSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 RecordsVerified = 0;
	FString ReactionActionId;
	FString PlayerAttackActionId;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionNPCReactionCombatSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionNPCReactionCombatSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
