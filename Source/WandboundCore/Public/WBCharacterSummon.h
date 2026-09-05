#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBCharacterSummonConditionPolicy : uint8
{
	Normal,
	IgnoreSummoningConditions
};

struct WANDBOUNDCORE_API FWBCharacterSummonRequest
{
	int32 OwnerPlayerId = INDEX_NONE;
	int32 ControllerPlayerId = INDEX_NONE;
	EWBCardZone SourceZone = EWBCardZone::Unknown;
	FString CardInstanceId;
	FString ExpectedCardId;
	FString RequiredFaction;
	FWBTile TargetTile;
	EWBCharacterSummonConditionPolicy ConditionPolicy =
		EWBCharacterSummonConditionPolicy::Normal;
	FName TraceKind = FName(TEXT("effect_summon_completed"));
	FString TransactionId;
	int32 SourceUnitId = INDEX_NONE;
	bool bIncludeSelectedInstanceInTrace = false;
};

struct WANDBOUNDCORE_API FWBCharacterSummonResult
{
	bool bOk = false;
	FString Reason;
	int32 CreatedUnitId = INDEX_NONE;
	FString CardInstanceId;
	FString CardId;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBCharacterSummon
{
public:
	static FWBCharacterSummonResult SummonExactCharacter(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBCharacterSummonRequest& Request);
};
