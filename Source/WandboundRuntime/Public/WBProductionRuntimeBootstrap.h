#pragma once

#include "CoreMinimal.h"
#include "WBMatchCoordinator.h"
#include "WBActiveFormat.h"
#include "WBGameStartAddendum.h"
#include "WBProductionCardDatabase.h"

struct WANDBOUNDRUNTIME_API FWBProductionRuntimeBootstrapRequest
{
	FString CardBundleManifestPath;
	FString MatchSpecificationPath;
	FString ActiveFormatPath;
	FString GameStartAddendumPath;
	bool bAllowTestBundle = false;
};

struct WANDBOUNDRUNTIME_API FWBProductionRuntimeBootstrapResult
{
	bool bOk = false;
	FString Reason;
	TSharedPtr<const FWBProductionCardDatabase> Database;
	FWBActiveFormatV1 ActiveFormat;
	FWBGameStartAddendumV1 GameStartAddendum;
	FWBMatchInitializationRequest InitializationRequest;
	TArray<FWBProductionCardDBDiagnostic> Diagnostics;
};

class WANDBOUNDRUNTIME_API WBProductionRuntimeBootstrap
{
public:
	static FWBProductionRuntimeBootstrapResult Build(
		const FWBProductionRuntimeBootstrapRequest& Request);
};
