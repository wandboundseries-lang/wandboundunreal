#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBInitialSetupPhase : uint8
{
	FormatValidation,
	FirstPlayerSelection,
	MarkerPlacement,
	HeroAtomicSpawn,
	HeroTriggerCollection,
	FirstPlayerHeroTriggerResolution,
	SecondPlayerHeroTriggerResolution,
	OpeningHandDraw,
	FirstTurnReady
};

struct WANDBOUNDCORE_API FWBInitialHeroPlacement
{
	int32 PlayerId = -1;
	FString HeroInstanceId;
	FString HeroCardId;
	FWBTile SpawnTile;
};

struct WANDBOUNDCORE_API FWBInitialHeroSetupRequest
{
	int32 FirstPlayerId = -1;
	TArray<FWBInitialHeroPlacement> Placements;
	TMap<int32, TArray<FString>> TriggerOrderChoices;
};

struct WANDBOUNDCORE_API FWBInitialHeroSetupResult
{
	bool bOk = false;
	FString Reason;
	bool bSpawnBatchCommitted = false;
	bool bTriggersResolved = false;
	EWBInitialSetupPhase FinalPhase = EWBInitialSetupPhase::HeroAtomicSpawn;
	TArray<FString> CollectedTriggerIds;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBInitialHeroSetup
{
public:
	static FWBInitialHeroSetupResult Apply(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FWBInitialHeroSetupRequest& Request);

	static bool CanSubmitManualReact(
		const FWBGameStateData& State,
		FString& OutReason);
};
