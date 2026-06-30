#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBEquipExecution.h"
#include "WBGameStateData.h"
#include "WBProductionSummonEquipDataProvider.h"

struct WANDBOUNDRUNTIME_API FWBProductionEquipExecutionRequest
{
	int32 ViewerPlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	int32 TargetUnitId = -1;
};

struct WANDBOUNDRUNTIME_API FWBProductionEquipExecutionHandoffResult
{
	bool bOk = false;
	FString Reason;

	int32 EquippedToUnitId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	FString SlotId;
	int32 RR = 0;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;

	TArray<FWBEquipExecutionTraceEvent> TraceEvents;
	FWBProductionSummonEquipDecisionData RefreshedSummonEquipData;
};

class WANDBOUNDRUNTIME_API FWBProductionEquipExecutionHandoff
{
public:
	FWBProductionEquipExecutionHandoffResult ExecuteEquipFromProviderOption(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBProductionSummonEquipDecisionData& CurrentDecisionData,
		const FWBProductionEquipExecutionRequest& Request) const;
};
