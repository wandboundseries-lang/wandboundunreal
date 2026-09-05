#include "WBArmorEffect.h"
#include "WBUnitStatMutation.h"

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

	FWBUnitStatMutationEntry Entry;
	Entry.Value = Request.Amount;
	switch (Request.Operation)
	{
	case EWBArmorEffectOp::AddCurrentArmor:
		Entry.Stat = EWBStoredUnitStat::CurrentArmor;
		break;
	case EWBArmorEffectOp::ReduceCurrentArmor:
		Entry.Stat = EWBStoredUnitStat::CurrentArmor;
		Entry.Value = -Request.Amount;
		break;
	case EWBArmorEffectOp::SetCurrentArmor:
		Entry.Stat = EWBStoredUnitStat::CurrentArmor;
		Entry.Operation = EWBUnitStatMutationOp::Set;
		break;
	case EWBArmorEffectOp::AddMaxArmor:
		Entry.Stat = EWBStoredUnitStat::MaxArmor;
		break;
	case EWBArmorEffectOp::ReduceMaxArmor:
		Entry.Stat = EWBStoredUnitStat::MaxArmor;
		Entry.Value = -Request.Amount;
		break;
	case EWBArmorEffectOp::SetMaxArmor:
		Entry.Stat = EWBStoredUnitStat::MaxArmor;
		Entry.Operation = EWBUnitStatMutationOp::Set;
		break;
	case EWBArmorEffectOp::RestoreArmorToMax:
		Entry.Stat = EWBStoredUnitStat::CurrentArmor;
		Entry.Operation = EWBUnitStatMutationOp::Set;
		Entry.Value = Result.PreviousMaxArmor;
		break;
	default:
		Result.bOk = false;
		Result.Reason = TEXT("unknown_armor_effect_operation");
		return Result;
	}

	FWBUnitStatMutationRequest Mutation;
	// Legacy Armor requests carry a semantic reason, not a declared source.
	// The enclosing effect owns the accepted action/trace identity.
	Mutation.TransactionId = FString::Printf(TEXT("armor:%s:u%d:%s"),
		*GetOperationName(Request.Operation).ToString(), Request.TargetUnitId,
		*Request.SourceReason.ToString());
	Mutation.TargetUnitId = Request.TargetUnitId;
	Mutation.Entries.Add(Entry);
	const FWBApplyActionResult Applied = WBUnitStatMutation::Apply(State, Mutation);
	if (!Applied.bOk)
	{
		Result.Reason = Applied.Reason;
		return Result;
	}
	Result.NewCurrentArmor = Target->GetCurrentArmor();
	Result.NewMaxArmor = Target->GetMaxArmor();
	Result.bOk = true;
	return Result;
}
