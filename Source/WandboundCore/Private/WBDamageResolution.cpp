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

FWBDamageResolutionResult WBDamageResolution::CalculateDamageRequest(
	const FWBGameStateData& State,
	const FWBDamageRequest& Request)
{
	FWBDamageResolutionResult Result;
	Result.Request = Request;
	Result.Prevention = EvaluateDamagePrevention(State, Request);

	const FWBUnitState* Target = State.GetUnitById(Request.TargetUnitId);
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
	Result.PreviousArmor = Target->GetCurrentArmor();
	Result.NewArmor = Result.PreviousArmor;
	Result.bBypassedArmor = Request.bBypassArmor;

	const int32 DamageAfterPrevention = FMath::Max(Result.Prevention.FinalDamage, 0);
	if (Request.bBypassArmor)
	{
		Result.ArmorAbsorbedAmount = 0;
		Result.HPDamageAmount = DamageAfterPrevention;
	}
	else
	{
		Result.ArmorAbsorbedAmount = FMath::Min(Result.PreviousArmor, DamageAfterPrevention);
		Result.NewArmor = FMath::Max(Result.PreviousArmor - Result.ArmorAbsorbedAmount, 0);
		Result.HPDamageAmount = FMath::Max(DamageAfterPrevention - Result.ArmorAbsorbedAmount, 0);
	}

	Result.NewHP = FMath::Max(Result.PreviousHP - Result.HPDamageAmount, 0);
	Result.bAtOrBelowZeroHP = Result.NewHP <= 0;
	Result.bOk = true;
	return Result;
}

FWBDamageResolutionResult WBDamageResolution::ApplyCalculatedDamage(
	FWBGameStateData& State,
	const FWBDamageResolutionResult& Calculation,
	const int32 HPDamageRecipientUnitId)
{
	FWBDamageResolutionResult Result = Calculation;
	if (!Calculation.bOk)
	{
		Result.bOk = false;
		Result.Reason = TEXT("invalid_damage_calculation");
		return Result;
	}

	FWBUnitState* CalculationTarget =
		State.GetMutableUnitById(Calculation.Request.TargetUnitId);
	if (CalculationTarget == nullptr || !CalculationTarget->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("damage_target_removed");
		return Result;
	}
	if (CalculationTarget->HP != Calculation.PreviousHP
		|| CalculationTarget->GetCurrentArmor() != Calculation.PreviousArmor)
	{
		Result.bOk = false;
		Result.Reason = TEXT("damage_calculation_stale");
		return Result;
	}

	const int32 RecipientUnitId = HPDamageRecipientUnitId == INDEX_NONE
		? Calculation.Request.TargetUnitId
		: HPDamageRecipientUnitId;
	FWBUnitState* Recipient = State.GetMutableUnitById(RecipientUnitId);
	if (Recipient == nullptr || !Recipient->IsUnitOnBoard())
	{
		Result.bOk = false;
		Result.Reason = TEXT("damage_recipient_removed");
		return Result;
	}

	CalculationTarget->CurrentArmor = Calculation.NewArmor;
	Result.Request.TargetUnitId = RecipientUnitId;
	Result.PreviousHP = Recipient->HP;
	Recipient->HP = FMath::Max(Recipient->HP - Calculation.HPDamageAmount, 0);
	Result.NewHP = Recipient->HP;
	Result.bAtOrBelowZeroHP = Result.NewHP <= 0;
	Result.bOk = true;
	Result.Reason.Reset();
	return Result;
}

FWBDamageResolutionResult WBDamageResolution::ResolveDamageRequest(
	FWBGameStateData& State,
	const FWBDamageRequest& Request)
{
	const FWBDamageResolutionResult Calculation =
		CalculateDamageRequest(State, Request);
	if (!Calculation.bOk)
	{
		return Calculation;
	}
	return ApplyCalculatedDamage(State, Calculation);
}
