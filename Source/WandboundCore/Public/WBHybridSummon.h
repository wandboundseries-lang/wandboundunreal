#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBHybridWandPaymentSource : uint8
{
	None,
	Hand,
	SacrificedUnit
};

enum class EWBHybridSummonResultCode : uint8
{
	Success,
	HybridDefinitionInvalid,
	HybridNotInHand,
	HybridWrongPlayer,
	HybridSacrificeRequired,
	HybridSacrificeInvalid,
	HybridHeroSacrificeInvalid,
	HybridWandPaymentRequired,
	HybridWandPaymentInvalid,
	HybridDestinationInvalid,
	HybridDestinationOccupied,
	HybridUnitCapExceeded,
	HybridReplacementNotSupported,
	HybridPlanStale,
	HybridZoneStateInvalid,
	HybridUnitIdAllocationFailed
};

struct WANDBOUNDCORE_API FWBHybridSummonPlan
{
	int32 ActingPlayerId = -1;
	FString HybridCardInstanceId;
	FString HybridDefinitionId;
	int32 SacrificedUnitId = -1;
	EWBHybridWandPaymentSource WandPaymentSource =
		EWBHybridWandPaymentSource::None;
	FString WandPaymentCardInstanceId;
	int32 WandPaymentUnitId = -1;
	FWBTile DestinationTile = FWBTile(-1, -1);
	bool bBecomesReplacementHero = false;
	int32 BeforeGeneration = -1;
	int32 BeforeRevision = -1;
};

struct WANDBOUNDCORE_API FWBHybridSummonPlanResult
{
	bool bOk = false;
	EWBHybridSummonResultCode Code =
		EWBHybridSummonResultCode::HybridReplacementNotSupported;
	FString Reason;
	TArray<FWBHybridSummonPlan> Plans;
};

struct WANDBOUNDCORE_API FWBHybridSummonResult
{
	bool bOk = false;
	EWBHybridSummonResultCode Code =
		EWBHybridSummonResultCode::HybridReplacementNotSupported;
	FString Reason;
	FWBHybridSummonPlan Plan;
	int32 SacrificedUnitId = -1;
	int32 OriginalHeroUnitId = -1;
	int32 NewHybridUnitId = -1;
	// Compatibility aliases for the established Hero-replacement API.
	int32 OldHeroUnitId = -1;
	int32 NewHeroUnitId = -1;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBHybridSummon
{
public:
	static FWBHybridSummonPlanResult BuildSummonPlans(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 ActingPlayerId,
		const FString& HybridCardInstanceId,
		int32 CoordinatorGeneration,
		int32 CoordinatorRevision);

	static FWBHybridSummonPlanResult BuildHeroReplacementPlans(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 ActingPlayerId,
		const FString& HybridCardInstanceId,
		int32 CoordinatorGeneration,
		int32 CoordinatorRevision);

	static FWBHybridSummonResult ExecuteSummon(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBHybridSummonPlan& Plan,
		int32 CoordinatorGeneration,
		int32 CoordinatorRevision);

	static FWBHybridSummonResult ExecuteHeroReplacement(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBHybridSummonPlan& Plan,
		int32 CoordinatorGeneration,
		int32 CoordinatorRevision);

	static FString BuildStableActionId(const FWBHybridSummonPlan& Plan);
	static FString ResultCodeToString(EWBHybridSummonResultCode Code);
};
