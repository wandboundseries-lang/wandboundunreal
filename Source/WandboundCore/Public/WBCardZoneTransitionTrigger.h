#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneTransition.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBCardZoneTransitionTriggerSourceSnapshot
{
	FString SourceCardInstanceId;
	FString SourceCardId;
	int32 OwnerPlayerId = INDEX_NONE;
	EWBCardZone ResidenceZoneAtCollection = EWBCardZone::Unknown;
	int32 ZoneIndexAtCollection = INDEX_NONE;
	EWBCardZoneTransitionTriggerSourceScope SourceScope =
		EWBCardZoneTransitionTriggerSourceScope::Unknown;
	FString TriggerId;
	EWBTriggerEligibilityPolicy EligibilityPolicy =
		EWBTriggerEligibilityPolicy::SnapshotAtCollection;

	bool IsValid() const;
};

struct WANDBOUNDCORE_API FWBCardZoneTransitionTriggerInstance
{
	FWBCardZoneTransitionSnapshot TransitionSnapshot;
	FWBCardZoneTransitionTriggerSourceSnapshot SourceSnapshot;
	FWBCardZoneTransitionTriggerDefinition Definition;
	FString StableTriggerId;
	int32 CollectionOrder = INDEX_NONE;
};

struct WANDBOUNDCORE_API FWBCardZoneTransitionTriggerResult
{
	bool bOk = false;
	FString Reason;
	int32 ProcessedTransitionCount = 0;
	int32 ResolvedTriggerCount = 0;
	int32 DrawnCardCount = 0;
	TArray<FWBCardZoneTransitionTriggerInstance> CollectedTriggers;
	TArray<FWBCardZoneTransitionSnapshot> NestedTransitionEvents;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBCardZoneTransitionTrigger
{
public:
	static constexpr int32 DefaultResolutionGuard = 256;

	static bool MatchesFilter(
		const FWBCardZoneTransitionTriggerFilter& Filter,
		const FWBCardZoneTransitionSnapshot& Snapshot);

	static FWBCardZoneTransitionTriggerResult ResolveCommittedTransitions(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const TArray<FWBCardZoneTransitionSnapshot>& Transitions,
		int32 ResolutionGuard = DefaultResolutionGuard);
};
