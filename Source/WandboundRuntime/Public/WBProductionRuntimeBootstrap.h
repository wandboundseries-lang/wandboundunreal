#pragma once

#include "CoreMinimal.h"
#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"

struct WANDBOUNDRUNTIME_API FWBProductionRuntimeBootstrapRequest
{
	FString CardBundleManifestPath;
	FString MatchSpecificationPath;
	bool bAllowTestBundle = false;
};

struct WANDBOUNDRUNTIME_API FWBProductionRuntimeBootstrapResult
{
	bool bOk = false;
	FString Reason;
	TSharedPtr<const FWBProductionCardDatabase> Database;
	FWBMatchInitializationRequest InitializationRequest;
	TArray<FWBProductionCardDBDiagnostic> Diagnostics;
};

class WANDBOUNDRUNTIME_API WBProductionRuntimeBootstrap
{
public:
	static FWBProductionRuntimeBootstrapResult Build(
		const FWBProductionRuntimeBootstrapRequest& Request);
};
