#include "WBUnitStatMutation.h"

namespace
{
constexpr int32 StoredStatCount = static_cast<int32>(EWBStoredUnitStat::Unsupported);

FWBApplyActionResult StatMutationFailure(const TCHAR* Reason)
{
	FWBApplyActionResult Result;
	Result.Reason = Reason;
	return Result;
}
}

FName WBUnitStatMutation::GetStatName(const EWBStoredUnitStat Stat)
{
	switch (Stat)
	{
	case EWBStoredUnitStat::ATK: return TEXT("atk");
	case EWBStoredUnitStat::AR: return TEXT("ar");
	case EWBStoredUnitStat::CurrentHP: return TEXT("current_hp");
	case EWBStoredUnitStat::MaxHP: return TEXT("max_hp");
	case EWBStoredUnitStat::CurrentArmor: return TEXT("current_armor");
	case EWBStoredUnitStat::MaxArmor: return TEXT("max_armor");
	default: return NAME_None;
	}
}

FWBApplyActionResult WBUnitStatMutation::Apply(
	FWBGameStateData& State, const FWBUnitStatMutationRequest& Request)
{
	if (Request.TransactionId.IsEmpty() || Request.Source.SourceUnitId < INDEX_NONE
		|| Request.TargetUnitId < 0 || Request.Entries.IsEmpty())
	{
		return StatMutationFailure(TEXT("unit_stat_mutation_context_invalid"));
	}
	FWBUnitState* Target = State.GetMutableUnitById(Request.TargetUnitId);
	if (Target == nullptr || !Target->IsUnitOnBoard() || Target->bDefeated)
	{
		return StatMutationFailure(TEXT("unit_stat_mutation_target_unavailable"));
	}
	const int32 Before[StoredStatCount] = {
		Target->ATK, Target->AR, Target->HP, Target->MaxHP,
		Target->GetCurrentArmor(), Target->GetMaxArmor() };
	int64 Values[StoredStatCount] = {};
	bool Present[StoredStatCount] = {};
	bool IsSet[StoredStatCount] = {};
	for (const FWBUnitStatMutationEntry& Entry : Request.Entries)
	{
		const int32 Index = static_cast<int32>(Entry.Stat);
		if (Index >= StoredStatCount)
		{
			return StatMutationFailure(TEXT("unit_stat_mutation_unsupported_stat"));
		}
		if (Entry.Operation != EWBUnitStatMutationOp::Add
			&& Entry.Operation != EWBUnitStatMutationOp::Set)
		{
			return StatMutationFailure(TEXT("unit_stat_mutation_unsupported_operation"));
		}
		const bool bSet = Entry.Operation == EWBUnitStatMutationOp::Set;
		if (Present[Index] && (IsSet[Index] || bSet))
		{
			return StatMutationFailure(TEXT("unit_stat_mutation_ambiguous_entries"));
		}
		Present[Index] = true;
		IsSet[Index] = bSet;
		// An int32-sized array of int32 entries cannot overflow an int64 sum.
		Values[Index] += static_cast<int64>(Entry.Value);
	}
	int32 After[StoredStatCount];
	for (int32 Index = 0; Index < StoredStatCount; ++Index)
	{
		const int64 Value = IsSet[Index] ? Values[Index]
			: static_cast<int64>(Before[Index]) + Values[Index];
		if (Value < MIN_int32 || Value > MAX_int32)
		{
			return StatMutationFailure(TEXT("unit_stat_mutation_overflow"));
		}
		After[Index] = static_cast<int32>(Value);
	}
	if (After[0] < 0 || After[1] < 0 || After[3] < 1
		|| (IsSet[4] && After[4] < 0) || (IsSet[5] && After[5] < 0))
	{
		return StatMutationFailure(TEXT("unit_stat_mutation_result_invalid"));
	}
	if (Present[2] && After[2] < Before[2])
	{
		return StatMutationFailure(TEXT("unit_stat_mutation_hp_reduction_unsupported"));
	}
	if (After[2] < 0)
	{
		return StatMutationFailure(TEXT("unit_stat_mutation_result_invalid"));
	}
	// Current/max pairs use the final combined maxima, never request order.
	After[2] = FMath::Min(After[2], After[3]);
	After[5] = FMath::Max(0, After[5]);
	After[4] = FMath::Clamp(After[4], 0, After[5]);
	FWBUnitState WorkingUnit = *Target;
	WorkingUnit.ATK = After[0];
	WorkingUnit.AR = After[1];
	WorkingUnit.HP = After[2];
	WorkingUnit.MaxHP = After[3];
	WorkingUnit.CurrentArmor = After[4];
	WorkingUnit.MaxArmor = After[5];
	FWBApplyActionResult Result;
	Result.bOk = true;
	for (int32 Index = 0; Index < StoredStatCount; ++Index)
	{
		if (!Present[Index] && Before[Index] == After[Index]) continue;
		FWBTraceEvent Event;
		Event.Kind = TEXT("unit_stat_mutated");
		Event.ActionId = Request.TransactionId;
		Event.PlayerId = Target->GetControllerPlayerIdForRules();
		Event.SourceUnitId = Request.Source.SourceUnitId;
		Event.TargetUnitId = Request.TargetUnitId;
		Event.StatId = GetStatName(static_cast<EWBStoredUnitStat>(Index));
		Event.PreviousStatValue = Before[Index];
		Event.NewStatValue = After[Index];
		Event.bOk = true;
		Result.TraceEvents.Add(MoveTemp(Event));
	}
	*Target = MoveTemp(WorkingUnit);
	return Result;
}
