#include "WBUnitStatDelta.h"

namespace
{
FWBUnitStatDeltaResult FailStatDelta(const FString& Reason)
{
	FWBUnitStatDeltaResult Result;
	Result.Reason = Reason;
	return Result;
}

bool CanAddInt32(const int32 Value, const int32 Delta)
{
	const int64 Result = static_cast<int64>(Value) + static_cast<int64>(Delta);
	return Result >= MIN_int32 && Result <= MAX_int32;
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
	if (!CanAddInt32(Target->ATK, Request.ATKDelta)
		|| !CanAddInt32(Target->MaxHP, Request.MaxHPDelta)
		|| !CanAddInt32(Target->HP, Request.CurrentHPDelta))
	{
		return FailStatDelta(TEXT("unit_stat_delta_overflow"));
	}

	const int32 NewATK = Target->ATK + Request.ATKDelta;
	const int32 NewMaxHP = Target->MaxHP + Request.MaxHPDelta;
	if (NewATK < 0 || NewMaxHP < 1)
	{
		return FailStatDelta(TEXT("unit_stat_delta_result_invalid"));
	}
	const int32 NewHP = FMath::Clamp(
		Target->HP + Request.CurrentHPDelta,
		0,
		NewMaxHP);

	FWBGameStateData WorkingState = State;
	FWBUnitState* MutableTarget = WorkingState.GetMutableUnitById(
		Request.TargetUnitId);
	if (MutableTarget == nullptr)
	{
		return FailStatDelta(TEXT("unit_stat_delta_target_unavailable"));
	}
	const int32 PreviousATK = MutableTarget->ATK;
	const int32 PreviousMaxHP = MutableTarget->MaxHP;
	const int32 PreviousHP = MutableTarget->HP;
	MutableTarget->MaxHP = NewMaxHP;
	MutableTarget->ATK = NewATK;
	MutableTarget->HP = NewHP;

	FWBTraceEvent Applied;
	Applied.Kind = FName(TEXT("unit_stat_delta_applied"));
	Applied.ActionId = Request.TransactionId;
	Applied.PlayerId = MutableTarget->OwnerId;
	Applied.SourceUnitId = Request.SourceUnitId;
	Applied.TargetUnitId = Request.TargetUnitId;
	Applied.PreviousATK = PreviousATK;
	Applied.NewATK = NewATK;
	Applied.PreviousMaxHP = PreviousMaxHP;
	Applied.NewMaxHP = NewMaxHP;
	Applied.PreviousHP = PreviousHP;
	Applied.NewHP = NewHP;
	Applied.bOk = true;

	FWBUnitStatDeltaResult Result;
	Result.bOk = true;
	Result.TraceEvents.Add(MoveTemp(Applied));
	State = MoveTemp(WorkingState);
	return Result;
}
