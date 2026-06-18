#include "WBDamageResolution.h"

FWBDamagePreventionResult WBDamageResolution::EvaluateDamagePrevention(
	const FWBGameStateData& State,
	const FWBDamageRequest& Request)
{
	(void)State;

	FWBDamagePreventionResult Result;
	Result.bPrevented = false;
	Result.PreventedAmount = 0;
	Result.FinalDamage = FMath::Max(Request.BaseDamage, 0);
	Result.PreventionReason = NAME_None;
	return Result;
}

FWBDamageResolutionResult WBDamageResolution::ResolveDamageRequest(
	FWBGameStateData& State,
	const FWBDamageRequest& Request)
{
	FWBDamageResolutionResult Result;
	Result.Request = Request;
	Result.Prevention = EvaluateDamagePrevention(State, Request);

	FWBUnitState* Target = State.GetMutableUnitById(Request.TargetUnitId);
	if (Target == nullptr)
	{
		Result.bOk = false;
		Result.Reason = TEXT("missing_damage_target");
		return Result;
	}

	if (!Target->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("damage_target_removed");
		return Result;
	}

	Result.PreviousHP = Target->HP;
	Target->HP = FMath::Max(Result.PreviousHP - Result.Prevention.FinalDamage, 0);
	Result.NewHP = Target->HP;
	Result.bAtOrBelowZeroHP = Result.NewHP <= 0;
	Result.bOk = true;
	return Result;
}
