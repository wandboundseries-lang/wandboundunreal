#include "WBDamageEffect.h"

#include "WBDamageResolution.h"

namespace
{
FWBDamageEffectResult MakeDamageEffectFailure(const FWBDamageEffectRequest& Request, const FString& Reason)
{
	FWBDamageEffectResult Result;
	Result.bOk = false;
	Result.Reason = Reason;
	Result.Request = Request;
	return Result;
}
}

FWBDamageEffectResult WBDamageEffect::ApplyDamageEffect(
	FWBGameStateData& State,
	const FWBDamageEffectRequest& Request)
{
	FWBUnitState* Target = State.GetMutableUnitById(Request.TargetUnitId);
	if (Target == nullptr)
	{
		return MakeDamageEffectFailure(Request, TEXT("missing_target_unit"));
	}

	FWBDamageEffectResult Result;
	Result.Request = Request;
	Result.PreviousHP = Target->HP;
	Result.NewHP = Target->HP;
	Result.PreviousArmor = Target->GetCurrentArmor();
	Result.NewArmor = Result.PreviousArmor;
	Result.bBypassedArmor = Request.bBypassArmor;
	Result.bAtOrBelowZeroHP = Target->HP <= 0;

	if (!Target->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("target_unit_removed");
		return Result;
	}

	if (Request.Amount < 0)
	{
		Result.bOk = false;
		Result.Reason = TEXT("negative_damage_effect_amount");
		return Result;
	}

	FWBDamageRequest DamageRequest;
	DamageRequest.DamageKind = EWBDamageKind::Effect;
	DamageRequest.SourceUnitId = Request.SourceUnitId;
	DamageRequest.TargetUnitId = Request.TargetUnitId;
	DamageRequest.SourcePlayerId = Request.SourcePlayerId;
	DamageRequest.BaseDamage = Request.Amount;
	DamageRequest.bBypassArmor = Request.bBypassArmor;
	DamageRequest.DamageCause = Request.DamageCause.IsNone()
		? FName(TEXT("Effect"))
		: Request.DamageCause;

	const FWBDamageResolutionResult DamageResult = WBDamageResolution::ResolveDamageRequest(State, DamageRequest);
	if (!DamageResult.bOk)
	{
		Result.bOk = false;
		Result.Reason = DamageResult.Reason;
		return Result;
	}

	Result.PreviousHP = DamageResult.PreviousHP;
	Result.NewHP = DamageResult.NewHP;
	Result.PreviousArmor = DamageResult.PreviousArmor;
	Result.NewArmor = DamageResult.NewArmor;
	Result.ArmorAbsorbedAmount = DamageResult.ArmorAbsorbedAmount;
	Result.HPDamageAmount = DamageResult.HPDamageAmount;
	Result.bBypassedArmor = DamageResult.bBypassedArmor;
	Result.bAtOrBelowZeroHP = DamageResult.bAtOrBelowZeroHP;
	Result.bOk = true;
	return Result;
}
