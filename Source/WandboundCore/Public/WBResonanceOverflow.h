#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"

enum class EWBResonanceOverflowResultCode : uint8
{
	Success,
	InvalidPlayer,
	TargetUnitNotFound,
	TargetUnitNotOwned,
	InvalidRLState,
	ZoneStateInvalid,
	CardDefinitionNotFound,
	EquippedCardNotWand,
	InvalidRR,
	InsufficientOverflowCandidates,
	LifecycleMoveFailed,
	UnsupportedOverflowOperation
};

struct WANDBOUNDCORE_API FWBResonanceOverflowRemoval
{
	int32 PlayerId = -1;
	int32 UnitId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	FString SlotId;
	int32 EquipOrder = INDEX_NONE;
	int32 RR = 0;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;
};

struct WANDBOUNDCORE_API FWBResonanceOverflowPlan
{
	bool bOk = false;
	bool bHasOverflow = false;
	EWBResonanceOverflowResultCode Code = EWBResonanceOverflowResultCode::UnsupportedOverflowOperation;
	FString Reason;

	int32 PlayerId = -1;
	int32 UnitId = -1;
	int32 RLTotal = 0;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;
	int32 OverflowAmount = 0;

	TArray<FWBResonanceOverflowRemoval> Removals;
};

struct WANDBOUNDCORE_API FWBResonanceOverflowTraceEvent
{
	FString EventType;
	int32 PlayerId = -1;
	int32 UnitId = -1;
	int32 RLTotal = 0;
	int32 OverflowAmount = 0;
	FString SourceInstanceId;
	FString SourceCardId;
	FString SlotId;
	int32 EquipOrder = INDEX_NONE;
	int32 RR = 0;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;
};

struct WANDBOUNDCORE_API FWBResonanceOverflowResult
{
	bool bOk = false;
	bool bResolvedOverflow = false;
	EWBResonanceOverflowResultCode Code = EWBResonanceOverflowResultCode::UnsupportedOverflowOperation;
	FString Reason;

	FWBResonanceOverflowPlan Plan;
	TArray<FWBResonanceOverflowTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBResonanceOverflow
{
public:
	static FWBResonanceOverflowPlan BuildOverflowPlanForUnit(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 UnitId);

	static FWBResonanceOverflowResult ResolveOverflowForUnit(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 UnitId);

	static bool OverflowResultToJsonStringForTest(
		const FWBResonanceOverflowResult& Result,
		FString& OutJson);

	static FString ResultCodeToString(EWBResonanceOverflowResultCode Code);
};
