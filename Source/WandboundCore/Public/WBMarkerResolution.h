#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBSetupMarkerPlacement
{
	int32 PlayerId = -1;
	EWBMarkerType Type = EWBMarkerType::Unknown;
	FWBTile Tile;
	FString DefinitionId;
	int32 PlacementOrder = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBMarkerResolutionResult
{
	bool bOk = false;
	FString Reason;
	bool bMarkerFound = false;
	bool bMarkerConsumed = false;
	bool bTrapDamageApplied = false;
	bool bNPCSpawnScheduled = false;
	bool bUnitDefeated = false;
	int32 MarkerId = -1;
	int32 EnteringUnitId = -1;
	int32 PendingSpawnId = -1;
	TArray<FWBTraceEvent> TraceEvents;
};

struct WANDBOUNDCORE_API FWBNPCPhaseResult
{
	bool bOk = false;
	FString Reason;
	int32 SpawnedCount = 0;
	int32 BlockedCount = 0;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBMarkerResolution
{
public:
	static FWBMarkerResolutionResult ApplyCanonicalSetup(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const TArray<FWBSetupMarkerPlacement>& Placements);

	static FWBMarkerResolutionResult ResolveMarkerAtUnitTile(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 EnteringUnitId);

	static FWBNPCPhaseResult ProcessPendingNPCSpawns(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 PhaseOwnerPlayerId);

	static bool ValidateAuthoritativeState(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		FString& OutReason);

	static FName MarkerTypeToName(EWBMarkerType Type);
};
