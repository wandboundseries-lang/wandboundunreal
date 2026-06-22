#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBHealEffectRequest
{
	int32 TargetUnitId = -1;
	int32 SourceUnitId = -1;
	int32 SourcePlayerId = -1;
	int32 Amount = 0;
	FName SourceReason;
};

struct WANDBOUNDCORE_API FWBHealEffectResult
{
	bool bOk = false;
	FString Reason;
	FWBHealEffectRequest Request;
	int32 PreviousHP = 0;
	int32 NewHP = 0;
	int32 EffectiveHealAmount = 0;
};

class WANDBOUNDCORE_API WBHealEffect
{
public:
	static FWBHealEffectResult ApplyHealEffect(
		FWBGameStateData& State,
		const FWBHealEffectRequest& Request);
};
