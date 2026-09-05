#include "WBUnitStatDelta.h"
#include "WBUnitStatMutation.h"

namespace
{
FWBUnitStatDeltaResult FailStatDelta(const FString& Reason)
{
	FWBUnitStatDeltaResult Result;
	Result.Reason = Reason;
	return Result;
}

}

FWBUnitStatDeltaResult WBUnitStatDelta::ApplyPersistentDelta(
	FWBGameStateData& State,
	const FWBUnitStatDeltaRequest& Request)
{
	if (Request.SourceUnitId < 0 || Request.TargetUnitId < 0
		|| Request.TransactionId.IsEmpty())
	{
		return FailStatDelta(TEXT("unit_stat_delta_context_invalid"));
	}
	const FWBUnitState* Target = State.GetUnitById(Request.TargetUnitId);
	if (Target == nullptr || !Target->IsUnitOnBoard() || Target->bDefeated)
	{
		return FailStatDelta(TEXT("unit_stat_delta_target_unavailable"));
	}
	const int32 PreviousATK = Target->ATK;
	const int32 PreviousMaxHP = Target->MaxHP;
	const int32 PreviousHP = Target->HP;
	FWBUnitStatMutationRequest Mutation;
	Mutation.TransactionId = Request.TransactionId;
	Mutation.Source.SourceUnitId = Request.SourceUnitId;
	Mutation.TargetUnitId = Request.TargetUnitId;
	Mutation.Entries = {
		{ EWBStoredUnitStat::ATK, EWBUnitStatMutationOp::Add, Request.ATKDelta },
		{ EWBStoredUnitStat::MaxHP, EWBUnitStatMutationOp::Add, Request.MaxHPDelta },
		{ EWBStoredUnitStat::CurrentHP, EWBUnitStatMutationOp::Add, Request.CurrentHPDelta } };
	const FWBApplyActionResult MutationResult = WBUnitStatMutation::Apply(State, Mutation);
	if (!MutationResult.bOk)
	{
		return FailStatDelta(MutationResult.Reason == TEXT("unit_stat_mutation_overflow")
			? TEXT("unit_stat_delta_overflow") : TEXT("unit_stat_delta_result_invalid"));
	}

	FWBTraceEvent Applied;
	Applied.Kind = FName(TEXT("unit_stat_delta_applied"));
	Applied.ActionId = Request.TransactionId;
	Applied.PlayerId = Target->GetControllerPlayerIdForRules();
	Applied.SourceUnitId = Request.SourceUnitId;
	Applied.TargetUnitId = Request.TargetUnitId;
	Applied.PreviousATK = PreviousATK;
	Applied.NewATK = Target->ATK;
	Applied.PreviousMaxHP = PreviousMaxHP;
	Applied.NewMaxHP = Target->MaxHP;
	Applied.PreviousHP = PreviousHP;
	Applied.NewHP = Target->HP;
	Applied.bOk = true;

	FWBUnitStatDeltaResult Result;
	Result.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Applied));
	return Result;
}
