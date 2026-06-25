#include "WBCardActivationCostPaymentVerifier.h"

bool FWBCardActivationCostPaymentVerifier::VerifyUnitRLState(
	const FWBGameStateData& State,
	const FWBExpectedActivationCostPaymentState& Expected,
	FString& OutReason)
{
	const FWBUnitState* SourceUnit = State.GetUnitById(Expected.SourceUnitId);
	if (SourceUnit == nullptr)
	{
		OutReason = FString::Printf(TEXT("missing_rl_source_unit:%d"), Expected.SourceUnitId);
		return false;
	}

	const int32 AvailableRL = FMath::Max(0, SourceUnit->RLTotal - SourceUnit->RLUsed);
	if (SourceUnit->RLTotal != Expected.ExpectedRLTotal)
	{
		OutReason = FString::Printf(
			TEXT("rl_total_expected_%d_got_%d"),
			Expected.ExpectedRLTotal,
			SourceUnit->RLTotal);
		return false;
	}

	if (SourceUnit->RLUsed != Expected.ExpectedRLUsed)
	{
		OutReason = FString::Printf(
			TEXT("rl_used_expected_%d_got_%d"),
			Expected.ExpectedRLUsed,
			SourceUnit->RLUsed);
		return false;
	}

	if (AvailableRL != Expected.ExpectedAvailableRL)
	{
		OutReason = FString::Printf(
			TEXT("available_rl_expected_%d_got_%d"),
			Expected.ExpectedAvailableRL,
			AvailableRL);
		return false;
	}

	OutReason.Reset();
	return true;
}

bool FWBCardActivationCostPaymentVerifier::VerifyCostPaidTrace(
	const TArray<FWBTraceEvent>& Traces,
	const int32 ExpectedPlayerId,
	const int32 ExpectedSourceUnitId,
	const int32 ExpectedCostAmount,
	const int32 ExpectedPreviousRLUsed,
	const int32 ExpectedNewRLUsed,
	const int32 ExpectedAvailableRLBefore,
	const int32 ExpectedAvailableRLAfter,
	FString& OutReason)
{
	const FName CostPaidKind(TEXT("card_activation_cost_paid"));
	const FWBTraceEvent* CostPaidTrace = nullptr;
	for (const FWBTraceEvent& Trace : Traces)
	{
		if (Trace.Kind == CostPaidKind)
		{
			CostPaidTrace = &Trace;
			break;
		}
	}

	if (CostPaidTrace == nullptr)
	{
		OutReason = TEXT("missing_card_activation_cost_paid_trace");
		return false;
	}

	if (CostPaidTrace->PlayerId != ExpectedPlayerId
		|| CostPaidTrace->SourceUnitId != ExpectedSourceUnitId
		|| CostPaidTrace->CostAmount != ExpectedCostAmount
		|| CostPaidTrace->PreviousRLUsed != ExpectedPreviousRLUsed
		|| CostPaidTrace->NewRLUsed != ExpectedNewRLUsed
		|| CostPaidTrace->AvailableRLBefore != ExpectedAvailableRLBefore
		|| CostPaidTrace->AvailableRLAfter != ExpectedAvailableRLAfter)
	{
		OutReason = TEXT("card_activation_cost_paid_trace_mismatch");
		return false;
	}

	OutReason.Reset();
	return true;
}

bool FWBCardActivationCostPaymentVerifier::ContainsTraceKind(
	const TArray<FWBTraceEvent>& Traces,
	const FName ExpectedKind)
{
	for (const FWBTraceEvent& Trace : Traces)
	{
		if (Trace.Kind == ExpectedKind)
		{
			return true;
		}
	}

	return false;
}

bool FWBCardActivationCostPaymentVerifier::TraceJsonExcludesForbiddenSubstrings(
	const TArray<FWBTraceEvent>& Traces,
	const TArray<FString>& ForbiddenSubstrings,
	FString& OutReason)
{
	const FString TraceJson = WBReplayTrace::SerializeEvents(Traces);
	for (const FString& ForbiddenSubstring : ForbiddenSubstrings)
	{
		if (TraceJson.Contains(ForbiddenSubstring, ESearchCase::IgnoreCase))
		{
			OutReason = FString::Printf(TEXT("forbidden_trace_substring:%s"), *ForbiddenSubstring);
			return false;
		}
	}

	OutReason.Reset();
	return true;
}
