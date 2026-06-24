#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCandidate.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBCardActivationAffordabilityRequest
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;

	FString SourceCardId;
	FString SourceEffectId;

	int32 RequiredRR = 0;
	FName CostKind;
};

struct WANDBOUNDCORE_API FWBCardActivationAffordabilityResult
{
	bool bOk = false;
	FString Reason;

	int32 PlayerId = -1;
	int32 SourceUnitId = -1;

	int32 RequiredRR = 0;
	int32 CurrentRL = 0;
	int32 RLUsed = 0;
	int32 AvailableRL = 0;

	bool bAffordable = false;
	FName CostKind;
};

class WANDBOUNDCORE_API IWBCardActivationAffordabilityProvider
{
public:
	virtual ~IWBCardActivationAffordabilityProvider() = default;

	virtual FWBCardActivationAffordabilityResult QueryAffordability(
		const FWBGameStateData& State,
		const FWBCardActivationAffordabilityRequest& Request) const = 0;
};

class WANDBOUNDCORE_API FWBFixedCardActivationAffordabilityProvider final : public IWBCardActivationAffordabilityProvider
{
public:
	FWBCardActivationAffordabilityResult FixedResult;

	virtual FWBCardActivationAffordabilityResult QueryAffordability(
		const FWBGameStateData& State,
		const FWBCardActivationAffordabilityRequest& Request) const override;
};

class WANDBOUNDCORE_API WBCardActivationAffordability
{
public:
	static FWBCardActivationAffordabilityResult QueryFromUnitRL(
		const FWBGameStateData& State,
		const FWBCardActivationAffordabilityRequest& Request);

	static void ApplyResultToSourceGateContext(
		const FWBCardActivationAffordabilityResult& Result,
		FWBCardActivationSourceGateContext& Context);

	static FWBCardActivationCandidateSource BuildCandidateSourceWithAffordability(
		const FWBGameStateData& State,
		const FWBCardActivationCandidateSource& Source,
		const IWBCardActivationAffordabilityProvider& Provider);
};
