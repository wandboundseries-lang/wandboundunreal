#include "WBCharacterPassiveEligibility.h"

#include "WBStatusSemantics.h"

bool WBCharacterPassiveEligibility::CanUseAutomaticCharacterPassive(
	const FWBUnitState& Unit)
{
	return Unit.IsUnitOnBoard()
		&& !Unit.bDefeated
		&& WBStatusSemantics::CanUseAutomaticCharacterPassive(Unit);
}
