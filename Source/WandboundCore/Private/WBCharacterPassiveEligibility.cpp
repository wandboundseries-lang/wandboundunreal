#include "WBCharacterPassiveEligibility.h"

bool WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(
	const FWBUnitState& Unit)
{
	return Unit.IsUnitOnBoard()
		&& !Unit.bDefeated
		&& !Unit.HasStatus(FName(TEXT("Stunned")))
		&& !Unit.HasStatus(FName(TEXT("Frozen")))
		&& !Unit.HasStatus(FName(TEXT("Negated")));
}
