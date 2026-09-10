#pragma once
#include "CoreMinimal.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionBattlePhaseSmokeResult
{
	bool bOk = false;
	FString Reason;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};
class WANDBOUNDRUNTIME_API WBProductionBattlePhaseSmoke
{
public:
	static bool IsRequested();
	static FWBProductionBattlePhaseSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& Request, const FString& Scenario);
};
