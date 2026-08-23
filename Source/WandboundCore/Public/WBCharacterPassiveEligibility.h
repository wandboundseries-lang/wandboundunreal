#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

class WANDBOUNDCORE_API WBCharacterPassiveEligibility
{
public:
	static bool CanUseAutomaticCharacterPassive(const FWBUnitState& Unit);
};
