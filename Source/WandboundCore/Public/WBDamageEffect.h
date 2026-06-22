#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBDamageEffectRequest
{
	int32 TargetUnitId = -1;
	int32 SourceUnitId = -1;
	int32 SourcePlayerId = -1;
	int32 Amount = 0;
	bool bBypassArmor = false;
	FName DamageCause;
	FName SourceReason;
};

struct WANDBOUNDCORE_API FWBDamageEffectResult
{
	bool bOk = false;
	FString Reason;
	FWBDamageEffectRequest Request;
	int32 PreviousHP = 0;
	int32 NewHP = 0;
	int32 PreviousArmor = 0;
	int32 NewArmor = 0;
	int32 ArmorAbsorbedAmount = 0;
	int32 HPDamageAmount = 0;
	bool bBypassedArmor = false;
	bool bAtOrBelowZeroHP = false;
};

class WANDBOUNDCORE_API WBDamageEffect
{
public:
	static FWBDamageEffectResult ApplyDamageEffect(
		FWBGameStateData& State,
		const FWBDamageEffectRequest& Request);
};
