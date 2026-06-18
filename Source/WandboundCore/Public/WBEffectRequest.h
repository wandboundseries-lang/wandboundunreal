#pragma once

#include "CoreMinimal.h"
#include "WBArmorEffect.h"
#include "WBReplayTrace.h"
#include "WBTypes.h"

enum class EWBGenericEffectOp : uint8
{
	Unknown,
	ArmorEffect
};

struct WANDBOUNDCORE_API FWBEffectSourceRef
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	FString SourceCardId;
	FString SourceEffectId;
};

struct WANDBOUNDCORE_API FWBEffectTargetRef
{
	int32 TargetUnitId = -1;
	FWBTile TargetTile;
	FWBWallEdge TargetWallEdge;
};

struct WANDBOUNDCORE_API FWBGenericEffectPayload
{
	EWBGenericEffectOp Operation = EWBGenericEffectOp::Unknown;
	FWBArmorEffectRequest ArmorEffect;
};

struct WANDBOUNDCORE_API FWBEffectRequest
{
	FWBEffectSourceRef Source;
	FWBEffectTargetRef Target;
	TArray<FWBGenericEffectPayload> Payloads;
};

struct WANDBOUNDCORE_API FWBEffectRequestResult
{
	bool bOk = false;
	FString Reason;
	FWBEffectRequest Request;
	TArray<FWBApplyActionResult> PayloadResults;
	TArray<FWBTraceEvent> TraceEvents;
};
