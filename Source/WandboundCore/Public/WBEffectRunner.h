#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBArmorEffect.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

class WANDBOUNDCORE_API WBEffectRunner
{
public:
	static FWBApplyActionResult ApplyAction(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyMove(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyAttackDeclare(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPendingAttackDamage(FWBGameStateData& State);
	static FWBApplyActionResult ApplyZeroHPDeathRemoval(FWBGameStateData& State);
	static FWBApplyActionResult ApplyEndTurn(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPass(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPassResponse(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyStartOfTurnStatusTicks(FWBGameStateData& State, int32 PlayerId);
	static FWBApplyActionResult ApplyEndOfTurnStatusTicks(FWBGameStateData& State, int32 PlayerId);
	static FWBApplyActionResult ApplyDeterministicTurnTransition(FWBGameStateData& State, int32 EndingPlayerId, int32 NextPlayerExplicitMPRoll);
	static FWBApplyActionResult ApplyTurnStartResourceSetup(FWBGameStateData& State, int32 PlayerId, int32 ExplicitMPRoll);
	static FWBApplyActionResult ApplyArmorEffect(FWBGameStateData& State, const FWBArmorEffectRequest& Request);
};
