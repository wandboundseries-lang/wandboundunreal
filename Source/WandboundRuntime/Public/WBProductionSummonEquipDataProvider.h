#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBTypes.h"

struct FWBGameStateData;

struct WANDBOUNDRUNTIME_API FWBProductionSummonOption
{
	FString SourceInstanceId;
	FString SourceCardId;
	FString PublicName;

	int32 OwnerPlayerId = -1;
	TArray<FWBTile> LegalTiles;

	FWBCardCharacterStatsDefinition CharacterStats;
};

struct WANDBOUNDRUNTIME_API FWBProductionEquipOption
{
	FString SourceInstanceId;
	FString SourceCardId;
	FString PublicName;

	int32 OwnerPlayerId = -1;
	int32 RR = 0;

	TArray<int32> EligibleUnitIds;
};

struct WANDBOUNDRUNTIME_API FWBProductionSummonEquipProviderDiagnostic
{
	FString Code;
	FString CardId;
	FString InstanceId;
	int32 UnitId = -1;
};

struct WANDBOUNDRUNTIME_API FWBProductionSummonEquipDecisionData
{
	int32 ViewerPlayerId = -1;
	TArray<FWBProductionSummonOption> SummonOptions;
	TArray<FWBProductionEquipOption> EquipOptions;
	TArray<FWBProductionSummonEquipProviderDiagnostic> Diagnostics;
};

class WANDBOUNDRUNTIME_API FWBProductionSummonEquipDataProvider
{
public:
	FWBProductionSummonEquipDecisionData BuildDecisionData(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 ViewerPlayerId) const;
};
