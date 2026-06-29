#include "WBResonanceLoad.h"

namespace
{
FWBResonanceLoadSummary MakeLoadFailure(
	const int32 UnitId,
	const TCHAR* Reason)
{
	FWBResonanceLoadSummary Summary;
	Summary.Reason = Reason;
	Summary.UnitId = UnitId;
	return Summary;
}
}

FWBResonanceLoadSummary WBResonanceLoad::BuildUnitLoadSummary(
	const FWBGameStateData& State,
	const int32 UnitId)
{
	const FWBUnitState* Unit = State.GetUnitById(UnitId);
	if (Unit == nullptr || !Unit->IsUnitOnBoard())
	{
		return MakeLoadFailure(UnitId, TEXT("unit_not_found"));
	}

	FWBResonanceLoadSummary Summary;
	Summary.UnitId = Unit->UnitId;
	Summary.OwnerPlayerId = Unit->OwnerId;
	Summary.CurrentRL = Unit->RLTotal;
	Summary.RLUsed = Unit->RLUsed;

	if (!FWBGameStateData::IsValidPlayerId(Unit->OwnerId))
	{
		Summary.Reason = TEXT("invalid_unit_owner");
		return Summary;
	}

	if (Unit->RLTotal < 0 || Unit->RLUsed < 0)
	{
		Summary.Reason = TEXT("invalid_rl_state");
		return Summary;
	}

	if (Unit->RLUsed > Unit->RLTotal)
	{
		Summary.Reason = TEXT("overflow_pending");
		Summary.AvailableRL = 0;
		return Summary;
	}

	Summary.bOk = true;
	Summary.Reason = TEXT("success");
	Summary.AvailableRL = Unit->RLTotal - Unit->RLUsed;
	return Summary;
}

bool WBResonanceLoad::CanPayRR(
	const FWBGameStateData& State,
	const int32 UnitId,
	const int32 RR,
	FWBResonanceLoadSummary& OutSummary)
{
	if (RR < 0)
	{
		OutSummary = MakeLoadFailure(UnitId, TEXT("invalid_rr"));
		return false;
	}

	OutSummary = BuildUnitLoadSummary(State, UnitId);
	if (!OutSummary.bOk)
	{
		return false;
	}

	return RR <= OutSummary.AvailableRL;
}

FString WBResonanceLoad::ResultReasonToString(const FString& Reason)
{
	if (Reason == TEXT("success")
		|| Reason == TEXT("unit_not_found")
		|| Reason == TEXT("invalid_unit_owner")
		|| Reason == TEXT("invalid_rr")
		|| Reason == TEXT("invalid_rl_state")
		|| Reason == TEXT("overflow_pending"))
	{
		return Reason;
	}

	return TEXT("invalid_rl_state");
}
