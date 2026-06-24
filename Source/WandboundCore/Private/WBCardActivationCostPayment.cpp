#include "WBCardActivationCostPayment.h"

namespace
{
FWBCardActivationCostPaymentResult MakePaymentFailure(
	const FWBCardActivationCostPaymentRequest& Request,
	const TCHAR* Reason)
{
	FWBCardActivationCostPaymentResult Result;
	Result.Request = Request;
	Result.Reason = Reason;
	return Result;
}

}

FWBCardActivationCostPaymentResult WBCardActivationCostPayment::CanPayCost(
	const FWBGameStateData& State,
	const FWBCardActivationCostPaymentRequest& Request)
{
	if (!FWBGameStateData::IsValidPlayerId(Request.PlayerId))
	{
		return MakePaymentFailure(Request, TEXT("invalid_player"));
	}

	if (Request.RequiredRR < 0)
	{
		return MakePaymentFailure(Request, TEXT("invalid_cost_requirement"));
	}

	const FWBUnitState* SourceUnit = State.GetUnitById(Request.SourceUnitId);
	if (SourceUnit == nullptr)
	{
		return MakePaymentFailure(Request, TEXT("source_unit_missing"));
	}

	if (SourceUnit->bDefeated || !SourceUnit->IsUnitOnBoard())
	{
		return MakePaymentFailure(Request, TEXT("source_unit_removed"));
	}

	if (SourceUnit->OwnerId != Request.PlayerId)
	{
		return MakePaymentFailure(Request, TEXT("source_unit_owner_mismatch"));
	}

	FWBCardActivationCostPaymentResult Result;
	Result.bOk = true;
	Result.Request = Request;
	Result.RLTotal = SourceUnit->RLTotal;
	Result.PreviousRLUsed = SourceUnit->RLUsed;
	Result.AvailableRLBefore = FMath::Max(0, Result.RLTotal - Result.PreviousRLUsed);

	if (Request.RequiredRR > TNumericLimits<int32>::Max() - Result.PreviousRLUsed)
	{
		Result.bOk = false;
		Result.Reason = TEXT("cost_not_affordable");
		Result.NewRLUsed = Result.PreviousRLUsed;
		Result.AvailableRLAfter = Result.AvailableRLBefore;
		return Result;
	}

	Result.NewRLUsed = Result.PreviousRLUsed + Request.RequiredRR;
	Result.AvailableRLAfter = FMath::Max(0, Result.RLTotal - Result.NewRLUsed);

	if (Request.RequiredRR > Result.AvailableRLBefore)
	{
		Result.bOk = false;
		Result.Reason = TEXT("cost_not_affordable");
		Result.NewRLUsed = Result.PreviousRLUsed;
		Result.AvailableRLAfter = Result.AvailableRLBefore;
		return Result;
	}

	Result.Reason.Reset();
	return Result;
}

FWBCardActivationCostPaymentResult WBCardActivationCostPayment::PayCost(
	FWBGameStateData& State,
	const FWBCardActivationCostPaymentRequest& Request)
{
	FWBCardActivationCostPaymentResult Result = CanPayCost(State, Request);
	if (!Result.bOk)
	{
		return Result;
	}

	FWBUnitState* SourceUnit = State.GetMutableUnitById(Request.SourceUnitId);
	if (SourceUnit == nullptr)
	{
		return MakePaymentFailure(Result.Request, TEXT("source_unit_missing"));
	}

	SourceUnit->RLUsed = Result.NewRLUsed;
	return Result;
}
