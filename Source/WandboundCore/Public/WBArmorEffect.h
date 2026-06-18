#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

enum class EWBArmorEffectOp : uint8
{
	Unknown,
	AddCurrentArmor,
	ReduceCurrentArmor,
	SetCurrentArmor,
	AddMaxArmor,
	ReduceMaxArmor,
	SetMaxArmor,
	RestoreArmorToMax
};

struct WANDBOUNDCORE_API FWBArmorEffectRequest
{
	EWBArmorEffectOp Operation = EWBArmorEffectOp::Unknown;
	int32 TargetUnitId = -1;
	int32 Amount = 0;
	FName SourceReason;
};

struct WANDBOUNDCORE_API FWBArmorEffectResult
{
	bool bOk = false;
	FString Reason;
	FWBArmorEffectRequest Request;
	int32 PreviousCurrentArmor = 0;
	int32 NewCurrentArmor = 0;
	int32 PreviousMaxArmor = 0;
	int32 NewMaxArmor = 0;
};

class WANDBOUNDCORE_API WBArmorEffect
{
public:
	static FWBArmorEffectResult ApplyArmorEffect(FWBGameStateData& State, const FWBArmorEffectRequest& Request);
	static FName GetOperationName(EWBArmorEffectOp Operation);
};
