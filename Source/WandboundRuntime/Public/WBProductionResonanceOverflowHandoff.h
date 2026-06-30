#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBProductionSummonEquipDataProvider.h"
#include "WBResonanceOverflow.h"

struct WANDBOUNDRUNTIME_API FWBProductionResonanceOverflowRequest
{
	int32 ViewerPlayerId = -1;
	int32 UnitId = -1;
	int32 ExpectedRLUsedBeforeResolution = INDEX_NONE;
};

struct WANDBOUNDRUNTIME_API FWBProductionResonanceOverflowHandoffResult
{
	bool bOk = false;
	bool bResolvedOverflow = false;
	FString Reason;

	int32 ViewerPlayerId = -1;
	int32 UnitId = -1;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;

	TArray<FWBResonanceOverflowTraceEvent> TraceEvents;
	FWBProductionSummonEquipDecisionData RefreshedSummonEquipData;
};

class WANDBOUNDRUNTIME_API FWBProductionResonanceOverflowHandoff
{
public:
	FWBProductionResonanceOverflowHandoffResult ResolveOverflowAndRefresh(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBProductionSummonEquipDecisionData& CurrentDecisionData,
		const FWBProductionResonanceOverflowRequest& Request) const;
};
