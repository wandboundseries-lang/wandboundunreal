#pragma once

#include "CoreMinimal.h"
#include "WBAction.h"
#include "WBArmorEffect.h"
#include "WBCardActivationCommand.h"
#include "WBEffectRequest.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct FWBCardDefinitionRepository;

class WANDBOUNDCORE_API WBEffectRunner
{
public:
	static FWBApplyActionResult ApplyAction(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyAction(FWBGameStateData& State, const FWBCardDefinitionRepository& Repository, const FWBAction& Action);
	static FWBApplyActionResult ApplyMove(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyMove(FWBGameStateData& State, const FWBCardDefinitionRepository& Repository, const FWBAction& Action);
	static FWBApplyActionResult ApplyNPCMove(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyNPCMove(FWBGameStateData& State, const FWBCardDefinitionRepository& Repository, const FWBAction& Action);
	static FWBApplyActionResult ApplyAttackDeclare(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyAttackDeclare(FWBGameStateData& State, const FWBCardDefinitionRepository& Repository, const FWBAction& Action);
	static FWBApplyActionResult ApplyNPCAttackDeclare(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyNPCAttackDeclare(FWBGameStateData& State, const FWBCardDefinitionRepository& Repository, const FWBAction& Action);
	static FWBApplyActionResult CalculatePendingAttackDamage(FWBGameStateData& State);
	static FWBApplyActionResult ResolvePendingAttackDamageSubstitution(FWBGameStateData& State);
	static FWBApplyActionResult ApplyCalculatedPendingAttackDamage(FWBGameStateData& State, bool bPreservePendingAttack = false);
	static FWBApplyActionResult ApplyPendingAttackDamage(FWBGameStateData& State, bool bPreservePendingAttack = false);
	static FWBApplyActionResult ApplyPendingAttackRedirect(
		FWBGameStateData& State,
		const FString& PendingAttackContinuationId,
		int32 NewTargetUnitId);
	static FWBApplyActionResult ApplyPendingAttackRedirect(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FString& PendingAttackContinuationId,
		int32 NewTargetUnitId);
	static FWBApplyActionResult ApplyPendingAttackDamageSubstitutionRegistration(
		FWBGameStateData& State,
		const FString& PendingAttackContinuationId,
		int32 SubstituteUnitId);
	static FWBApplyActionResult ApplyZeroHPDeathRemoval(
		FWBGameStateData& State,
		EWBUnitDestructionCause Cause = EWBUnitDestructionCause::Unknown);
	static FWBApplyActionResult ApplyEndTurn(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPass(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyPassResponse(FWBGameStateData& State, const FWBAction& Action);
	static FWBApplyActionResult ApplyStartOfTurnStatusTicks(FWBGameStateData& State, int32 PlayerId);
	static FWBApplyActionResult ApplyEndOfTurnStatusTicks(FWBGameStateData& State, int32 PlayerId);
	// Compatibility only. Production full turn transitions are owned by
	// WBMatchCoordinator::SubmitActionId.
	static FWBApplyActionResult ApplyDeterministicTurnTransition(FWBGameStateData& State, int32 EndingPlayerId, int32 NextPlayerExplicitMPRoll);
	static FWBApplyActionResult ApplyTurnStartMPRoll(FWBGameStateData& State, int32 PlayerId, int32 ExplicitMPRoll);
	static FWBApplyActionResult ApplyTurnStartResourceReset(FWBGameStateData& State, int32 PlayerId);
	static FWBApplyActionResult ApplyTurnStartResourceSetup(FWBGameStateData& State, int32 PlayerId, int32 ExplicitMPRoll);
	static FWBApplyActionResult ApplyArmorEffect(FWBGameStateData& State, const FWBArmorEffectRequest& Request);
	static FWBApplyActionResult ApplyStatusEffect(FWBGameStateData& State, const FWBStatusEffectRequest& Request);
	static FWBApplyActionResult ApplyDamageEffect(FWBGameStateData& State, const FWBDamageEffectRequest& Request);
	static FWBApplyActionResult ApplyHealEffect(FWBGameStateData& State, const FWBHealEffectRequest& Request);
	static FWBEffectRequestResult ApplyEffectRequest(FWBGameStateData& State, const FWBEffectRequest& Request);
	static FWBEffectRequestResult ApplyEffectRequest(
		FWBGameStateData& State,
		const FWBEffectRequest& Request,
		const FWBCardDefinitionRepository& Repository);
	static FWBCardActivationCommandResult ApplyCardActivationCommand(FWBGameStateData& State, const FWBCardActivationCommand& Command);
	static FWBCardActivationCommandResult ApplyCardActivationCommand(
		FWBGameStateData& State,
		const FWBCardActivationCommand& Command,
		const FWBCardDefinitionRepository& Repository);
};
