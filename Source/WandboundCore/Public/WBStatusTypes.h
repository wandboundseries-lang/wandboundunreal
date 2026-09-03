#pragma once

#include "CoreMinimal.h"

enum class EWBStatusApplicationOrigin : uint8
{
	Unknown,
	Activation,
	TriggeredResolution,
	StatusTick,
	GameRule,
	Other
};

struct WANDBOUNDCORE_API FWBStatusSourceProvenance
{
	int32 SourcePlayerId = INDEX_NONE;
	int32 SourceOwnerPlayerId = INDEX_NONE;
	int32 SourceUnitId = INDEX_NONE;
	FString SourceCardId;
	FString SourceCardInstanceId;
	FString SourceEffectId;
	EWBStatusApplicationOrigin Origin = EWBStatusApplicationOrigin::Unknown;

	bool HasDeterministicData() const
	{
		return SourcePlayerId != INDEX_NONE
			|| SourceOwnerPlayerId != INDEX_NONE
			|| SourceUnitId != INDEX_NONE
			|| !SourceCardId.IsEmpty()
			|| !SourceCardInstanceId.IsEmpty()
			|| !SourceEffectId.IsEmpty()
			|| Origin != EWBStatusApplicationOrigin::Unknown;
	}
};

struct WANDBOUNDCORE_API FWBStatusInstanceState
{
	int32 TargetUnitId = INDEX_NONE;
	FName StatusId;
	// Zero is the established Unreal representation for permanent duration.
	int32 Duration = 0;
	FWBStatusSourceProvenance Source;
};
