#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBCSNInheritanceWandLocation : uint8
{
	EquippedToSource,
	ControllerDiscard,
	DetachedSourceSnapshot
};

struct WANDBOUNDCORE_API FWBCSNInheritanceSourceData
{
	int32 SourceUnitId = INDEX_NONE;
	int32 SourceCurrentRL = 0;
	TArray<FWBEquippedCardEntry> EquippedWands;
};

struct WANDBOUNDCORE_API FWBCSNInheritanceMutationRequest
{
	int32 ControllerPlayerId = INDEX_NONE;
	int32 SourceUnitId = INDEX_NONE;
	int32 TargetUnitId = INDEX_NONE;
	int32 SourceCurrentRL = 0;
	TArray<FWBEquippedCardEntry> EquippedWandSnapshot;
	EWBCSNInheritanceWandLocation ExpectedWandLocation =
		EWBCSNInheritanceWandLocation::EquippedToSource;
	FString TransactionId;
};

struct WANDBOUNDCORE_API FWBCSNInheritanceMutationResult
{
	bool bOk = false;
	FString Reason;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBCSNInheritance
{
public:
	static FWBCSNInheritanceMutationResult Apply(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBCSNInheritanceMutationRequest& Request);
};
