#include "WBHealEffect.h"

namespace
{
FWBHealEffectResult MakeHealEffectFailure(const FWBHealEffectRequest& Request, const FString& Reason)
{
	FWBHealEffectResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.Request = Request;
	return Result;
}
}

FWBHealEffectResult WBHealEffect::ApplyHealEffect(
	FWBGameStateData& State,
	const FWBHealEffectRequest& Request)
{
	FWBUnitState* Target = State.GetMutableUnitById(Request.TargetUnitId);
	if (Target == nullptr)
	{
		return MakeHealEffectFailure(Request, TEXT("missing_target_unit"));
	}

	FWBHealEffectResult Result;
	Result.Request = Request;
	Result.PreviousHP = Target->HP;
	Result.NewHP = Target->HP;
	Result.EffectiveHealAmount = 0;

	if (!Target->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("target_unit_removed");
		return Result;
	}

	if (Request.Amount < 0)
	{
		Result.bOk = false;
		Result.Reason = TEXT("negative_heal_effect_amount");
		return Result;
	}

	if (Request.Amount > 0)
	{
		const int32 MaxHP = FMath::Max(Target->MaxHP, 0);
		Target->HP = FMath::Clamp(Target->HP + Request.Amount, 0, MaxHP);
	}

	Result.NewHP = Target->HP;
	Result.EffectiveHealAmount = Result.NewHP - Result.PreviousHP;
	Result.bOk = true;
	return Result;
}
