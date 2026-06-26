#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBRuntimeActivationDataProvider.h"
#include "WBRuntimeActivationDecisionSessionFacadeComponent.h"

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationProviderRefreshResult
{
	bool bOk = false;
	FString Reason;

	FWBRuntimeActivationDataProviderResult ProviderResult;
	FWBRuntimeActivationDecisionSessionRefreshResult SessionRefreshResult;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationProviderExecutionResult
{
	bool bOk = false;
	FString Reason;

	FWBRuntimeActivationDataProviderResult PostActivationProviderResult;
	FWBRuntimeActivationDecisionSessionExecutionResult SessionExecutionResult;
};

class WANDBOUNDRUNTIME_API WBRuntimeActivationDataProviderAdapter
{
public:
	static FWBRuntimeActivationProviderRefreshResult RefreshSessionFromProvider(
		UWBRuntimeActivationDecisionSessionFacadeComponent* SessionFacade,
		const IWBRuntimeActivationDataProvider& Provider,
		const FWBRuntimeActivationDataProviderRequest& Request);

	static FWBRuntimeActivationProviderExecutionResult ExecuteActivationAndRefreshFromProvider(
		FWBGameStateData& State,
		UWBRuntimeActivationDecisionSessionFacadeComponent* SessionFacade,
		const FString& SelectedActivationActionId,
		const IWBRuntimeActivationDataProvider& Provider,
		const FWBRuntimeActivationDataProviderRequest& PostActivationRequest,
		const FWBRuntimePostActivationRefreshOptions& Options = FWBRuntimePostActivationRefreshOptions());
};
