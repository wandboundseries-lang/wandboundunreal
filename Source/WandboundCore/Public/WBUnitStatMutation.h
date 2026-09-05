#pragma once

#include "CoreMinimal.h"
#include "WBEventSnapshot.h"
#include "WBReplayTrace.h"

enum class EWBStoredUnitStat : uint8
{
	ATK,
	AR,
	CurrentHP,
	MaxHP,
	CurrentArmor,
	MaxArmor,
	Unsupported
};

enum class EWBUnitStatMutationOp : uint8
{
	Add,
	Set
};

struct WANDBOUNDCORE_API FWBUnitStatMutationEntry
{
	EWBStoredUnitStat Stat = EWBStoredUnitStat::Unsupported;
	EWBUnitStatMutationOp Operation = EWBUnitStatMutationOp::Add;
	int32 Value = 0;
};

struct WANDBOUNDCORE_API FWBUnitStatMutationRequest
{
	FString TransactionId;
	FWBEventSourceSnapshot Source;
	int32 TargetUnitId = INDEX_NONE;
	TArray<FWBUnitStatMutationEntry> Entries;
};

class WANDBOUNDCORE_API WBUnitStatMutation
{
public:
	// Source is captured provenance, not a live-source eligibility check.
	// Callers own trigger eligibility and declaration semantics.
	static FWBApplyActionResult Apply(
		FWBGameStateData& State, const FWBUnitStatMutationRequest& Request);
	static FName GetStatName(EWBStoredUnitStat Stat);
};
