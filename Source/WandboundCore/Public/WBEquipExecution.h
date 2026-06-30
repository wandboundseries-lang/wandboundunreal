#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"

enum class EWBEquipExecutionResultCode : uint8
{
	Success,
	InvalidPlayer,
	PlayerZonesMissing,
	SourceCardMissing,
	SourceCardNotInHand,
	SourceCardIdMismatch,
	CardDefinitionNotFound,
	SourceCardNotWand,
	InvalidRR,
	TargetUnitNotFound,
	TargetUnitNotOwned,
	InsufficientRL,
	ZoneStateInvalid,
	UnsupportedEquipOperation
};

struct WANDBOUNDCORE_API FWBEquipExecutionRequest
{
	int32 PlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	int32 TargetUnitId = -1;
};

struct WANDBOUNDCORE_API FWBEquipExecutionTraceEvent
{
	FString EventType;
	int32 PlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	int32 EquippedToUnitId = -1;
	FString SlotId;
	int32 RR = 0;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;
};

struct WANDBOUNDCORE_API FWBEquipExecutionResult
{
	bool bOk = false;
	EWBEquipExecutionResultCode Code = EWBEquipExecutionResultCode::UnsupportedEquipOperation;
	FString Reason;

	int32 PlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	int32 EquippedToUnitId = -1;
	FString SlotId;
	int32 RR = 0;
	int32 RLUsedBefore = 0;
	int32 RLUsedAfter = 0;

	TArray<FWBEquipExecutionTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBEquipExecution
{
public:
	static FWBEquipExecutionResult ExecuteWandEquipFromHand(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBEquipExecutionRequest& Request);

	static bool EquipExecutionResultToJsonStringForTest(
		const FWBEquipExecutionResult& Result,
		FString& OutJson);

	static FString ResultCodeToString(EWBEquipExecutionResultCode Code);
};
