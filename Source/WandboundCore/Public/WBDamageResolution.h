#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

enum class EWBDamageKind : uint8
{
	Unknown,
	Attack,
	Burn,
	Effect
};

struct WANDBOUNDCORE_API FWBDamageRequest
{
	EWBDamageKind DamageKind = EWBDamageKind::Unknown;
	int32 SourceUnitId = -1;
	int32 TargetUnitId = -1;
	int32 SourcePlayerId = -1;
	int32 BaseDamage = 0;
	bool bBypassArmor = false;
	FName DamageCause;
};

struct WANDBOUNDCORE_API FWBDamagePreventionResult
{
	bool bPrevented = false;
	int32 PreventedAmount = 0;
	int32 FinalDamage = 0;
	FName PreventionReason;
};

struct WANDBOUNDCORE_API FWBDamageResolutionResult
{
	bool bOk = false;
	FString Reason;
	FWBDamageRequest Request;
	FWBDamagePreventionResult Prevention;
	int32 PreviousHP = 0;
	int32 NewHP = 0;
	int32 PreviousArmor = 0;
	int32 NewArmor = 0;
	int32 ArmorAbsorbedAmount = 0;
	bool bBypassedArmor = false;
	int32 HPDamageAmount = 0;
	bool bAtOrBelowZeroHP = false;
};

class WANDBOUNDCORE_API WBDamageResolution
{
public:
	static FWBDamagePreventionResult EvaluateDamagePrevention(
		const FWBGameStateData& State,
		const FWBDamageRequest& Request);

	static FWBDamageResolutionResult ResolveDamageRequest(
		FWBGameStateData& State,
		const FWBDamageRequest& Request);
};
