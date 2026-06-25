#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationLegalAction.h"
#include "WBPublicBoardSummary.h"

class UWBRuntimeActivationPresentationModelComponent;

struct WANDBOUNDRUNTIME_API FWBRuntimeActivationPresentationRefreshResult
{
	bool bOk = false;
	FString Reason;

	bool bActivationPresentationRefreshed = false;

	int32 ActivationActionCount = 0;
	int32 ActivationPresentationEntryCount = 0;
	int32 TargetPresentationEntryCount = 0;
};

class WANDBOUNDRUNTIME_API WBRuntimeActivationPresentationRefreshAdapter
{
public:
	static FWBRuntimeActivationPresentationRefreshResult RefreshActivationPresentation(
		const FWBCardActivationLegalActionSet& ActivationActionSet,
		const FWBPublicBoardSummary& PublicBoardSummary,
		UWBRuntimeActivationPresentationModelComponent* ActivationPresentationModel);
};
