#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBEffectiveUnitStatResult
{
	bool bOk = false;
	FString Reason;
	int32 StoredValue = 0;
	int32 EffectiveValue = 0;
	int32 AppliedModifierCount = 0;
};

class WANDBOUNDCORE_API WBUnitStatQuery
{
public:
	static FWBEffectiveUnitStatResult GetEffectiveAR(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 UnitId);

	// Deliberately excludes range-dependent enemy AR auras, preventing mutual
	// aura sources from recursively redefining one another's radius.
	static int32 GetAuraRangeAR(
		const FWBGameStateData& State,
		int32 UnitId);
};
