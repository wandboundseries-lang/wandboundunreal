#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBEffectRequest.h"
#include "WBReplayTrace.h"

class WANDBOUNDCORE_API WBUnitReplacementEffect
{
public:
	static FWBApplyActionResult ApplyPendingAttackDefenderReplacement(
		FWBGameStateData& State,
		const FWBEffectRequest& Request,
		const FWBGenericEffectPayload& Payload,
		const FWBCardDefinitionRepository& Repository);
};
