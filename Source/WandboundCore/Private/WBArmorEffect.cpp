#include "WBArmorEffect.h"

namespace
{
FWBArmorEffectResult MakeArmorEffectFailure(const FWBArmorEffectRequest& Request, const FString& Reason)
{
	FWBArmorEffectResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.Request = Request;
	return Result;
}
}

FName WBArmorEffect::GetOperationName(const EWBArmorEffectOp Operation)
{
	switch (Operation)
	{
	case EWBArmorEffectOp::AddCurrentArmor:
		return FName(TEXT("add_current_armor"));
	case EWBArmorEffectOp::ReduceCurrentArmor:
		return FName(TEXT("reduce_current_armor"));
	case EWBArmorEffectOp::SetCurrentArmor:
		return FName(TEXT("set_current_armor"));
	case EWBArmorEffectOp::AddMaxArmor:
		return FName(TEXT("add_max_armor"));
	case EWBArmorEffectOp::ReduceMaxArmor:
		return FName(TEXT("reduce_max_armor"));
	case EWBArmorEffectOp::SetMaxArmor:
		return FName(TEXT("set_max_armor"));
	case EWBArmorEffectOp::RestoreArmorToMax:
		return FName(TEXT("restore_armor_to_max"));
	default:
		return FName(TEXT("unknown"));
	}
}

FWBArmorEffectResult WBArmorEffect::ApplyArmorEffect(
	FWBGameStateData& State,
	const FWBArmorEffectRequest& Request)
{
	FWBUnitState* Target = State.GetMutableUnitById(Request.TargetUnitId);
	if (Target == nullptr)
	{
		return MakeArmorEffectFailure(Request, TEXT("missing_target_unit"));
	}

	FWBArmorEffectResult Result;
	Result.Request = Request;
	Result.PreviousCurrentArmor = Target->GetCurrentArmor();
	Result.NewCurrentArmor = Result.PreviousCurrentArmor;
	Result.PreviousMaxArmor = Target->GetMaxArmor();
	Result.NewMaxArmor = Result.PreviousMaxArmor;

	if (!Target->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("target_unit_removed");
		return Result;
	}

	if (Request.Operation == EWBArmorEffectOp::Unknown)
	{
		Result.bOk = false;
		Result.Reason = TEXT("unknown_armor_effect_operation");
		return Result;
	}

	if (Request.Amount < 0)
	{
		Result.bOk = false;
		Result.Reason = TEXT("negative_armor_effect_amount");
		return Result;
	}

	switch (Request.Operation)
	{
	case EWBArmorEffectOp::AddCurrentArmor:
		Result.NewCurrentArmor = Result.PreviousMaxArmor > 0
			? FMath::Min(Result.PreviousMaxArmor, Result.PreviousCurrentArmor + Request.Amount)
			: 0;
		break;
	case EWBArmorEffectOp::ReduceCurrentArmor:
		Result.NewCurrentArmor = FMath::Max(Result.PreviousCurrentArmor - Request.Amount, 0);
		break;
	case EWBArmorEffectOp::SetCurrentArmor:
		Result.NewCurrentArmor = FMath::Clamp(Request.Amount, 0, Result.PreviousMaxArmor);
		break;
	case EWBArmorEffectOp::AddMaxArmor:
		Result.NewMaxArmor = Result.PreviousMaxArmor + Request.Amount;
		Result.NewCurrentArmor = FMath::Min(Result.PreviousCurrentArmor, Result.NewMaxArmor);
		break;
	case EWBArmorEffectOp::ReduceMaxArmor:
		Result.NewMaxArmor = FMath::Max(Result.PreviousMaxArmor - Request.Amount, 0);
		Result.NewCurrentArmor = FMath::Min(Result.PreviousCurrentArmor, Result.NewMaxArmor);
		break;
	case EWBArmorEffectOp::SetMaxArmor:
		Result.NewMaxArmor = FMath::Max(Request.Amount, 0);
		Result.NewCurrentArmor = FMath::Min(Result.PreviousCurrentArmor, Result.NewMaxArmor);
		break;
	case EWBArmorEffectOp::RestoreArmorToMax:
		Result.NewCurrentArmor = Result.PreviousMaxArmor;
		break;
	default:
		Result.bOk = false;
		Result.Reason = TEXT("unknown_armor_effect_operation");
		return Result;
	}

	Target->SetArmorForTest(Result.NewCurrentArmor, Result.NewMaxArmor);
	Result.NewCurrentArmor = Target->GetCurrentArmor();
	Result.NewMaxArmor = Target->GetMaxArmor();
	Result.bOk = true;
	return Result;
}
