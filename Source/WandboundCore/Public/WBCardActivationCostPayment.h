#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBCardActivationCostPaymentRequest
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	int32 RequiredRR = 0;
	FName CostKind;
};

struct WANDBOUNDCORE_API FWBCardActivationCostPaymentResult
{
	bool bOk = false;
	FString Reason;

	FWBCardActivationCostPaymentRequest Request;

	int32 PreviousRLUsed = 0;
	int32 NewRLUsed = 0;
	int32 RLTotal = 0;
	int32 AvailableRLBefore = 0;
	int32 AvailableRLAfter = 0;
};

class WANDBOUNDCORE_API WBCardActivationCostPayment
{
public:
	static FWBCardActivationCostPaymentResult CanPayCost(
		const FWBGameStateData& State,
		const FWBCardActivationCostPaymentRequest& Request);

	static FWBCardActivationCostPaymentResult PayCost(
		FWBGameStateData& State,
		const FWBCardActivationCostPaymentRequest& Request);
};
