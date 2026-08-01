#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBPublicBoardSummary.h"
#include "WBPublicTurnSummary.h"
#include "WBReplayTrace.h"

class WBMatchCoordinator;

struct WANDBOUNDCORE_API FWBRuntimeTurnResolutionContext
{
	bool bResolveEndTurnAsFullTransition = true;
	IWBMPRollSource* MPRollSource = nullptr;
	// Preferred authority path. When supplied, all selected actions are
	// submitted to this coordinator and the raw-state compatibility path is
	// bypassed.
	WBMatchCoordinator* MatchCoordinator = nullptr;
};

struct WANDBOUNDCORE_API FWBRuntimeSelectedActionResult
{
	FWBApplyActionResult ApplyResult;
	FName SelectedActionType;
	FString SelectedActionId;
	bool bConsumedMPRoll = false;
	int32 ConsumedMPRoll = 0;
	bool bCoordinatorOwnedTransition = false;
	bool bTransitionCompleted = false;
	bool bPendingDecision = false;
	int32 PendingPlayerId = -1;
	int32 ActivePlayerId = -1;
	int32 TurnNumber = 0;
	FWBPublicTurnSummary FinalPublicTurnSummary;
	FWBPublicBoardSummary FinalPublicBoardSummary;
};

class WANDBOUNDCORE_API WBRuntimeTurnResolutionAdapter
{
public:
	static FWBApplyActionResult ApplyRuntimeSelectedAction(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& Context);

	static FWBRuntimeSelectedActionResult ApplyRuntimeSelectedActionWithResult(
		FWBGameStateData& State,
		const FWBAction& SelectedAction,
		FWBRuntimeTurnResolutionContext& Context);
};
