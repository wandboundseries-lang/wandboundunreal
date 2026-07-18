#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBDeathPreventionResult
{
	bool bPrevented = false;
	FName PreventionReason;
};

struct WANDBOUNDCORE_API FWBDeathResolutionCandidate
{
	int32 UnitId = -1;
	int32 OwnerId = -1;
	bool bIsHero = false;
};

class WANDBOUNDCORE_API WBDeathResolution
{
public:
	static FWBDeathPreventionResult EvaluateDeathPrevention(
		const FWBGameStateData& State,
		const FWBDeathResolutionCandidate& Candidate);

	static FWBApplyActionResult ApplyZeroHPDeathResolution(FWBGameStateData& State);
};
