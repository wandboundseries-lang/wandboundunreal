#pragma once

#include "CoreMinimal.h"
#include "WBRuntimeActivationDecisionSessionFacadeComponent.h"

enum class EWBRuntimeActivationDataRequestKind : uint8
{
	Unknown,
	CurrentDecisionPoint,
	PostActivation
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationDataProviderRequest
{
	EWBRuntimeActivationDataRequestKind Kind = EWBRuntimeActivationDataRequestKind::Unknown;

	int32 PlayerId = -1;

	FString DecisionPointId;
	FString SelectedActivationActionId;

	// Optional context for tests, logs, or future provider routing.
	FString RequestReason;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationDataProviderResult
{
	bool bOk = false;
	FString Reason;

	FWBRuntimeActivationDataProviderRequest Request;

	FWBRuntimeActivationDecisionSessionRefreshInput RefreshInput;
};

class WANDBOUNDRUNTIME_API IWBRuntimeActivationDataProvider
{
public:
	virtual ~IWBRuntimeActivationDataProvider() = default;

	virtual FWBRuntimeActivationDataProviderResult GetActivationDecisionData(
		const FWBRuntimeActivationDataProviderRequest& Request) const = 0;
};
