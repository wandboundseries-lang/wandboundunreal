#include "WBRuntimeActivationExecutionBridge.h"

#include "WBEffectRunner.h"

namespace
{
FWBRuntimeActivationExecutionResult MakeActivationExecutionFailure(
	const FWBRuntimeActivationExecutionHandoffResult& Handoff,
	const FString& Reason)
{
	FWBRuntimeActivationExecutionResult Result;
	Result.bOk = false;
	Result.bExecutionAttempted = false;
	Result.bActivationResolved = false;
	Result.Reason = Reason;
	Result.SelectedActivationActionId = Handoff.SelectedActivationActionId;
	Result.Handoff = Handoff;
	return Result;
}

bool IsActivationCommandMissing(const FWBCardActivationCommand& Command)
{
	return !FWBGameStateData::IsValidPlayerId(Command.Source.PlayerId)
		&& Command.Source.SourceUnitId == -1
		&& Command.Source.SourceCardId.IsEmpty()
		&& Command.Source.SourceEffectId.IsEmpty()
		&& Command.EffectRequest.Source.PlayerId == -1
		&& Command.EffectRequest.Source.SourceUnitId == -1
		&& Command.EffectRequest.Source.SourceCardId.IsEmpty()
		&& Command.EffectRequest.Source.SourceEffectId.IsEmpty()
		&& Command.EffectRequest.Payloads.Num() == 0
		&& !Command.CostPaymentCommit.bPayCostOnSuccess
		&& !Command.UsageCommit.bMarkUsageOnSuccess
		&& Command.DebugActivationId.IsEmpty();
}
}

FWBRuntimeActivationExecutionResult
WBRuntimeActivationExecutionBridge::ExecuteResolvedActivationHandoff(
	FWBGameStateData& State,
	const FWBRuntimeActivationExecutionHandoffResult& Handoff)
{
	if (!Handoff.bHandoffOk)
	{
		return MakeActivationExecutionFailure(
			Handoff,
			Handoff.Reason.IsEmpty()
				? FString(TEXT("activation_handoff_not_ready"))
				: Handoff.Reason);
	}

	if (!Handoff.bSelectionResolved)
	{
		return MakeActivationExecutionFailure(
			Handoff,
			TEXT("activation_selection_not_resolved"));
	}

	if (Handoff.ActivationAction.ActivationActionId.IsEmpty())
	{
		return MakeActivationExecutionFailure(
			Handoff,
			TEXT("activation_action_id_missing"));
	}

	if (IsActivationCommandMissing(Handoff.ActivationAction.Command))
	{
		return MakeActivationExecutionFailure(
			Handoff,
			TEXT("activation_command_missing"));
	}

	FWBRuntimeActivationExecutionResult Result;
	Result.bExecutionAttempted = true;
	Result.SelectedActivationActionId = Handoff.SelectedActivationActionId;
	Result.Handoff = Handoff;
	Result.ActivationResult = WBEffectRunner::ApplyCardActivationCommand(
		State,
		Handoff.ActivationAction.Command);
	Result.bOk = Result.ActivationResult.bOk;
	Result.bActivationResolved = Result.ActivationResult.bOk;
	Result.Reason = Result.ActivationResult.Reason;
	return Result;
}
