#pragma once

#include "CoreMinimal.h"

struct FWBUnitState;

class WANDBOUNDCORE_API WBStatusSemantics
{
public:
	static FName CanonicalizeStatusId(FName StatusId);
	static bool IsCanonicalStatusId(FName StatusId);
	static bool IsDeferredStatusId(FName StatusId);
	static bool IsCleanseableStatusId(FName StatusId);
	static TArray<FName> GetCanonicalStatusIds();

	static bool HasCanonicalStatus(const FWBUnitState& Unit, FName StatusId);
	static bool CanDeclareMove(const FWBUnitState& Unit);
	static bool CanDeclareAttack(const FWBUnitState& Unit);
	static bool CanCounterattack(const FWBUnitState& Unit);
	static bool CanUseUnitActivation(const FWBUnitState& Unit);
	static bool CanUseAutomaticCharacterPassive(const FWBUnitState& Unit);
};
