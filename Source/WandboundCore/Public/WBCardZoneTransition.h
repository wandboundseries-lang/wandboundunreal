#pragma once

#include "CoreMinimal.h"
#include "WBCardZoneMutation.h"
#include "WBEventSnapshot.h"
#include "WBReplayTrace.h"

enum class EWBCardZoneTransitionCause : uint8
{
	Unknown,
	Draw,
	Effect,
	Cost,
	Rule,
	Setup
};

struct WANDBOUNDCORE_API FWBCardZoneTransitionContext
{
	EWBCardZoneTransitionCause Cause =
		EWBCardZoneTransitionCause::Unknown;
	FString SourceActionId;
	FString ContinuationId;
	EWBDeclarationProvenance ActionDeclaration =
		EWBDeclarationProvenance::Automatic;
	EWBDeclarationProvenance TargetDeclaration =
		EWBDeclarationProvenance::Automatic;
	int32 ResolutionOrder = 0;
};

struct WANDBOUNDCORE_API FWBCardZoneTransitionSnapshot
{
	FWBEventIdentitySnapshot EventIdentity;
	FString CardInstanceId;
	FString CardId;
	int32 OwnerPlayerId = INDEX_NONE;
	EWBCardZone SourceZone = EWBCardZone::Unknown;
	EWBCardZone DestinationZone = EWBCardZone::Unknown;
	EWBCardZoneTransitionCause Cause =
		EWBCardZoneTransitionCause::Unknown;
	int32 SourceZoneCountAfter = 0;
	int32 DestinationZoneCountAfter = 0;
	int32 ResolutionOrder = 0;

	bool IsValid() const;
};

class WANDBOUNDCORE_API WBCardZoneTransition
{
public:
	static bool BuildCommittedSnapshot(
		const FWBGameStateData& State,
		const FWBCardZoneMutationResult& Mutation,
		const FWBCardZoneTransitionContext& Context,
		FWBCardZoneTransitionSnapshot& OutSnapshot,
		FString& OutReason);

	static FWBTraceEvent MakeRedactedTrace(
		const FWBCardZoneTransitionSnapshot& Snapshot);

	static void AppendRedactedTraces(
		const TArray<FWBCardZoneTransitionSnapshot>& Snapshots,
		TArray<FWBTraceEvent>& OutTraceEvents);

	static FName CauseToName(EWBCardZoneTransitionCause Cause);
};
