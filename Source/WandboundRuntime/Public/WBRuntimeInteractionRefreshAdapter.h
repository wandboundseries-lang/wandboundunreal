#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBBoardSummaryBridge.h"
#include "WBPublicBoardSummary.h"

class UWBRuntimeTurnInteractionModelComponent;
class UWBRuntimeVisualControllerComponent;

struct WANDBOUNDRUNTIME_API FWBRuntimeInteractionRefreshOptions
{
	bool bRefreshTurnInteractionModel = true;
	bool bRefreshVisualController = true;
};

struct WANDBOUNDRUNTIME_API FWBRuntimeInteractionRefreshResult
{
	bool bOk = false;
	FString Reason;

	bool bInteractionModelRefreshed = false;
	bool bVisualControllerRefreshed = false;

	int32 LegalActionCount = 0;
	int32 PresentationEntryCount = 0;

	FWBBoardViewRefreshResult VisualRefreshResult;
};

class WANDBOUNDRUNTIME_API WBRuntimeInteractionRefreshAdapter
{
public:
	static FWBRuntimeInteractionRefreshResult RefreshDecisionPoint(
		const TArray<FWBAction>& PrecomputedLegalActions,
		const FWBPublicBoardSummary& PublicBoardSummary,
		UWBRuntimeTurnInteractionModelComponent* TurnInteractionModel,
		UWBRuntimeVisualControllerComponent* VisualController,
		const FWBRuntimeInteractionRefreshOptions& Options = FWBRuntimeInteractionRefreshOptions());
};
