#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

enum class EWBCardActivationSourceZone : uint8
{
	Unknown,
	Fixture,
	Hand,
	Board,
	Equipped,
	Discard,
	Deck
};

enum class EWBCardActivationTimingRequirement : uint8
{
	Unknown,
	Any,
	NormalTurnPriority,
	ResponseWindow
};

struct WANDBOUNDCORE_API FWBCardActivationCostGateDefinition
{
	bool bRequiresExternalAffordability = false;
	int32 RequiredRR = 0;

	// Optional fixture metadata only. This pass does not pay costs.
	FName CostKind;
};

struct WANDBOUNDCORE_API FWBCardActivationCostGateContext
{
	bool bHasExternalAffordability = false;
	bool bExternallyAffordable = false;
	int32 SuppliedRequiredRR = 0;
	int32 SuppliedAvailableRL = 0;
	FName CostKind;
};

struct WANDBOUNDCORE_API FWBCardActivationSourceGateDefinition
{
	EWBCardActivationSourceZone RequiredZone = EWBCardActivationSourceZone::Fixture;
	EWBCardActivationTimingRequirement Timing = EWBCardActivationTimingRequirement::Any;

	bool bRequiresSourceUnit = false;
	bool bRequiresSourceUnitOwnership = true;
	bool bBlockedByStunned = true;
	bool bBlockedByFrozen = false;

	bool bRequiresCostsSatisfiedExternally = false;
	FWBCardActivationCostGateDefinition CostGate;
	bool bOncePerTurn = false;

	FString OncePerTurnKey;

	// Fixture parsing marks this so candidate generation can preserve legacy default filtering.
	bool bHasExplicitSourceGate = false;
};

struct WANDBOUNDCORE_API FWBCardActivationSourceGateContext
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;

	EWBCardActivationSourceZone SourceZone = EWBCardActivationSourceZone::Fixture;
	bool bCostsSatisfiedExternally = true;
	FWBCardActivationCostGateContext CostContext;

	FString ActivationUsageKey;

	// Fixture parsing marks this so explicit contexts can exercise the configurable source gate policy.
	bool bHasExplicitSourceGateContext = false;
};

struct WANDBOUNDCORE_API FWBCardActivationSourceGateResult
{
	bool bOk = false;
	FString Reason;
};

class WANDBOUNDCORE_API WBCardActivationSourceGate
{
public:
	static FWBCardActivationSourceGateResult Evaluate(
		const FWBGameStateData& State,
		const FWBCardActivationSourceGateDefinition& Gate,
		const FWBCardActivationSourceGateContext& Context);

	static FString BuildDefaultUsageKey(
		int32 PlayerId,
		int32 SourceUnitId,
		const FString& CardId,
		const FString& EffectId);

	static bool MarkUsageIfAllowedForTest(
		FWBGameStateData& State,
		int32 PlayerId,
		const FString& UsageKey,
		FString& OutReason);
};
