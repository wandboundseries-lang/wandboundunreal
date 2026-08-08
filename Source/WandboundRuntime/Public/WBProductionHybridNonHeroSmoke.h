#pragma once

#include "CoreMinimal.h"
#include "WBHybridSummon.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionHybridNonHeroSmokeResult
{
	bool bOk = false;
	FString Reason;
	int32 SacrificedUnitId = -1;
	int32 OriginalHeroUnitId = -1;
	int32 NewHybridUnitId = -1;
	FWBTile Destination = FWBTile(-1, -1);
	EWBHybridWandPaymentSource PaymentSource =
		EWBHybridWandPaymentSource::None;
	FString FinalStateDigest;
	FString FinalTraceDigest;
};

class WANDBOUNDRUNTIME_API WBProductionHybridNonHeroSmoke
{
public:
	static bool IsRequested(const TCHAR* CommandLine = nullptr);
	static FString GetReceiptPath();
	static FWBProductionHybridNonHeroSmokeResult Run(
		const FWBProductionRuntimeBootstrapRequest& BootstrapRequest);
};
