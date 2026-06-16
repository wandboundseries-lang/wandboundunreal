#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

class WANDBOUNDCORE_API WBEffectRunner
{
public:
	static FWBApplyActionResult ApplyAction(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyMove(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyEndTurn(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPass(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPassResponse(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyStartOfTurnStatusTicks(FWBGameStateData& State, int32 PlayerId);
	static FWBApplyActionResult ApplyEndOfTurnStatusTicks(FWBGameStateData& State, int32 PlayerId);
	static FWBApplyActionResult ApplyDeterministicTurnTransition(FWBGameStateData& State, int32 EndingPlayerId, int32 NextPlayerExplicitMPRoll);
	static FWBApplyActionResult ApplyTurnStartResourceSetup(FWBGameStateData& State, int32 PlayerId, int32 ExplicitMPRoll);
};
