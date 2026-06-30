#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBProductionSummonEquipDataProvider.h"
#include "WBSummonExecution.h"

struct WANDBOUNDRUNTIME_API FWBProductionSummonExecutionRequest
{
	int32 ViewerPlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	FWBTile TargetTile;
};

struct WANDBOUNDRUNTIME_API FWBProductionSummonExecutionHandoffResult
{
	bool bOk = false;
	FString Reason;

	int32 CreatedUnitId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	FWBTile TargetTile;

	TArray<FWBSummonExecutionTraceEvent> TraceEvents;
	FWBProductionSummonEquipDecisionData RefreshedSummonEquipData;
};

class WANDBOUNDRUNTIME_API FWBProductionSummonExecutionHandoff
{
public:
	FWBProductionSummonExecutionHandoffResult ExecuteSummonFromProviderOption(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBProductionSummonEquipDecisionData& CurrentDecisionData,
		const FWBProductionSummonExecutionRequest& Request) const;
};
