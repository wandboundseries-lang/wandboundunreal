#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

enum class EWBStatusEffectOp : uint8
{
	Unknown,
	ApplyStatus,
	RemoveStatus,
	SetStatusDuration,
	AddStatusDuration,
	ReduceStatusDuration,
	CleanseStatus,
	CleanseAllStatuses
};

struct WANDBOUNDCORE_API FWBStatusEffectRequest
{
	EWBStatusEffectOp Operation = EWBStatusEffectOp::Unknown;
	int32 TargetUnitId = -1;
	FName StatusId;
	int32 Duration = 0;
	FName SourceReason;
};

struct WANDBOUNDCORE_API FWBStatusEffectResult
{
	bool bOk = false;
	FString Reason;
	FWBStatusEffectRequest Request;
	bool bHadStatusBefore = false;
	bool bHasStatusAfter = false;
	int32 PreviousDuration = 0;
	int32 NewDuration = 0;
	TArray<FName> RemovedStatuses;
};

class WANDBOUNDCORE_API WBStatusEffect
{
public:
	static FWBStatusEffectResult ApplyStatusEffect(FWBGameStateData& State, const FWBStatusEffectRequest& Request);
	static FName CanonicalizeStatusId(FName StatusId);
	static bool DoesOperationRequireStatusId(EWBStatusEffectOp Operation);
	static FName GetOperationName(EWBStatusEffectOp Operation);
};
