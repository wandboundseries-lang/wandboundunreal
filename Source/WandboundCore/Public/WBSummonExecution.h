#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBTypes.h"

enum class EWBSummonExecutionResultCode : uint8
{
	Success,
	GameStateMissing,
	RepositoryMissing,
	InvalidPlayer,
	PlayerZonesMissing,
	SourceCardMissing,
	SourceCardNotInHand,
	SourceCardIdMismatch,
	CardDefinitionNotFound,
	SourceCardNotCharacter,
	InvalidCharacterStats,
	HeroNotFound,
	UnitCapReached,
	TargetTileOutOfBounds,
	TargetTileNotAdjacentToHero,
	TargetTileOccupied,
	MarkerTriggerDeferred,
	TargetTileNotAllowed,
	UnitIdAllocationFailed,
	ZoneStateInvalid,
	UnsupportedSummonOperation
};

struct WANDBOUNDCORE_API FWBSummonExecutionRequest
{
	int32 PlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	FWBTile TargetTile;
};

struct WANDBOUNDCORE_API FWBSummonExecutionTraceEvent
{
	FString EventType;
	int32 PlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	int32 CreatedUnitId = -1;
	FWBTile TargetTile;
};

struct WANDBOUNDCORE_API FWBSummonExecutionResult
{
	bool bOk = false;
	EWBSummonExecutionResultCode Code = EWBSummonExecutionResultCode::UnsupportedSummonOperation;
	FString Reason;

	int32 PlayerId = -1;
	FString SourceInstanceId;
	FString SourceCardId;
	int32 CreatedUnitId = -1;
	FWBTile TargetTile;

	TArray<FWBSummonExecutionTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBSummonExecution
{
public:
	static FWBSummonExecutionResult ExecuteCharacterSummonFromHand(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBSummonExecutionRequest& Request);

	static bool SummonExecutionResultToJsonStringForTest(
		const FWBSummonExecutionResult& Result,
		FString& OutJson);

	static FString ResultCodeToString(EWBSummonExecutionResultCode Code);
};
