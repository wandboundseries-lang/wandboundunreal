#include "WBStatusEffect.h"

namespace
{
FWBStatusEffectResult MakeStatusEffectFailure(const FWBStatusEffectRequest& Request, const FString& Reason)
{
	FWBStatusEffectResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.Request = Request;
	return Result;
}

void SortStatusNames(TArray<FName>& StatusIds)
{
	StatusIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
}

void AddUniqueCanonicalRemovedStatus(TArray<FName>& RemovedStatuses, const FName StatusId)
{
	const FName CanonicalStatusId = WBStatusEffect::CanonicalizeStatusId(StatusId);
	if (!CanonicalStatusId.IsNone())
	{
		RemovedStatuses.AddUnique(CanonicalStatusId);
	}
}

void EnsureCanonicalActiveStatus(FWBUnitState& Unit, const FName CanonicalStatusId, const int32 Duration)
{
	if (CanonicalStatusId.IsNone())
	{
		return;
	}

	Unit.RemoveStatus(CanonicalStatusId);
	Unit.AddStatus(CanonicalStatusId, Duration);
}
}

FName WBStatusEffect::CanonicalizeStatusId(const FName StatusId)
{
	if (StatusId.IsNone())
	{
		return NAME_None;
	}

	const FString LowerStatusId = StatusId.ToString().ToLower();
	if (LowerStatusId == TEXT("burn"))
	{
		return FName(TEXT("Burn"));
	}

	if (LowerStatusId == TEXT("poison"))
	{
		return FName(TEXT("Poison"));
	}

	if (LowerStatusId == TEXT("root") || LowerStatusId == TEXT("rooted"))
	{
		return FName(TEXT("Rooted"));
	}

	if (LowerStatusId == TEXT("stun") || LowerStatusId == TEXT("stunned"))
	{
		return FName(TEXT("Stunned"));
	}

	if (LowerStatusId == TEXT("frozen"))
	{
		return FName(TEXT("Frozen"));
	}

	return StatusId;
}

bool WBStatusEffect::DoesOperationRequireStatusId(const EWBStatusEffectOp Operation)
{
	switch (Operation)
	{
	case EWBStatusEffectOp::ApplyStatus:
	case EWBStatusEffectOp::RemoveStatus:
	case EWBStatusEffectOp::SetStatusDuration:
	case EWBStatusEffectOp::AddStatusDuration:
	case EWBStatusEffectOp::ReduceStatusDuration:
	case EWBStatusEffectOp::CleanseStatus:
		return true;
	default:
		return false;
	}
}

FName WBStatusEffect::GetOperationName(const EWBStatusEffectOp Operation)
{
	switch (Operation)
	{
	case EWBStatusEffectOp::ApplyStatus:
		return FName(TEXT("apply_status"));
	case EWBStatusEffectOp::RemoveStatus:
		return FName(TEXT("remove_status"));
	case EWBStatusEffectOp::SetStatusDuration:
		return FName(TEXT("set_status_duration"));
	case EWBStatusEffectOp::AddStatusDuration:
		return FName(TEXT("add_status_duration"));
	case EWBStatusEffectOp::ReduceStatusDuration:
		return FName(TEXT("reduce_status_duration"));
	case EWBStatusEffectOp::CleanseStatus:
		return FName(TEXT("cleanse_status"));
	case EWBStatusEffectOp::CleanseAllStatuses:
		return FName(TEXT("cleanse_all_statuses"));
	default:
		return FName(TEXT("unknown"));
	}
}

FWBStatusEffectResult WBStatusEffect::ApplyStatusEffect(
	FWBGameStateData& State,
	const FWBStatusEffectRequest& Request)
{
	FWBStatusEffectRequest CanonicalRequest = Request;
	CanonicalRequest.StatusId = CanonicalizeStatusId(Request.StatusId);

	FWBUnitState* Target = State.GetMutableUnitById(Request.TargetUnitId);
	if (Target == nullptr)
	{
		return MakeStatusEffectFailure(CanonicalRequest, TEXT("missing_target_unit"));
	}

	FWBStatusEffectResult Result;
	Result.Request = CanonicalRequest;

	if (!Target->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("target_unit_removed");
		return Result;
	}

	if (Request.Operation == EWBStatusEffectOp::Unknown)
	{
		Result.bOk = false;
		Result.Reason = TEXT("unknown_status_effect_operation");
		return Result;
	}

	if (Request.Duration < 0)
	{
		Result.bOk = false;
		Result.Reason = TEXT("negative_status_effect_duration");
		return Result;
	}

	if (DoesOperationRequireStatusId(Request.Operation) && CanonicalRequest.StatusId.IsNone())
	{
		Result.bOk = false;
		Result.Reason = TEXT("missing_status_id");
		return Result;
	}

	const FName CanonicalStatusId = CanonicalRequest.StatusId;
	Result.bHadStatusBefore = !CanonicalStatusId.IsNone() && Target->HasStatus(CanonicalStatusId);
	Result.PreviousDuration = Result.bHadStatusBefore ? Target->GetStatusTurnsRemaining(CanonicalStatusId) : 0;
	Result.NewDuration = Result.PreviousDuration;

	switch (Request.Operation)
	{
	case EWBStatusEffectOp::ApplyStatus:
		EnsureCanonicalActiveStatus(*Target, CanonicalStatusId, Request.Duration);
		Result.NewDuration = Request.Duration > 0 ? Request.Duration : 0;
		break;
	case EWBStatusEffectOp::RemoveStatus:
	case EWBStatusEffectOp::CleanseStatus:
		if (Result.bHadStatusBefore)
		{
			Target->RemoveStatus(CanonicalStatusId);
			AddUniqueCanonicalRemovedStatus(Result.RemovedStatuses, CanonicalStatusId);
		}
		Result.NewDuration = 0;
		break;
	case EWBStatusEffectOp::SetStatusDuration:
		if (!Result.bHadStatusBefore)
		{
			Result.bOk = false;
			Result.Reason = TEXT("status_not_active");
			return Result;
		}
		EnsureCanonicalActiveStatus(*Target, CanonicalStatusId, Request.Duration);
		Result.NewDuration = Request.Duration > 0 ? Request.Duration : 0;
		break;
	case EWBStatusEffectOp::AddStatusDuration:
		if (!Result.bHadStatusBefore)
		{
			Result.bOk = false;
			Result.Reason = TEXT("status_not_active");
			return Result;
		}
		Result.NewDuration = Result.PreviousDuration > 0
			? Result.PreviousDuration + Request.Duration
			: Request.Duration;
		EnsureCanonicalActiveStatus(*Target, CanonicalStatusId, Result.NewDuration);
		break;
	case EWBStatusEffectOp::ReduceStatusDuration:
		if (!Result.bHadStatusBefore)
		{
			Result.bOk = false;
			Result.Reason = TEXT("status_not_active");
			return Result;
		}
		if (Result.PreviousDuration > 0)
		{
			Result.NewDuration = FMath::Max(Result.PreviousDuration - Request.Duration, 0);
			if (Result.NewDuration <= 0)
			{
				Target->RemoveStatus(CanonicalStatusId);
				AddUniqueCanonicalRemovedStatus(Result.RemovedStatuses, CanonicalStatusId);
				Result.NewDuration = 0;
			}
			else
			{
				EnsureCanonicalActiveStatus(*Target, CanonicalStatusId, Result.NewDuration);
			}
		}
		else
		{
			EnsureCanonicalActiveStatus(*Target, CanonicalStatusId, 0);
			Result.NewDuration = 0;
		}
		break;
	case EWBStatusEffectOp::CleanseAllStatuses:
	{
		TArray<FName> StatusIds = Target->GetSortedStatusIdsForTrace();
		for (const FName& StatusId : StatusIds)
		{
			AddUniqueCanonicalRemovedStatus(Result.RemovedStatuses, StatusId);
			Target->RemoveStatus(StatusId);
		}
		Target->StatusTurnsRemaining.Empty();
		SortStatusNames(Result.RemovedStatuses);
		Result.NewDuration = 0;
		break;
	}
	default:
		Result.bOk = false;
		Result.Reason = TEXT("unknown_status_effect_operation");
		return Result;
	}

	Result.bHasStatusAfter = !CanonicalStatusId.IsNone() && Target->HasStatus(CanonicalStatusId);
	Result.bOk = true;
	SortStatusNames(Result.RemovedStatuses);
	return Result;
}
