#include "WBDeathResolution.h"

FWBDeathPreventionResult WBDeathResolution::EvaluateDeathPrevention(
	const FWBGameStateData& State,
	const FWBDeathResolutionCandidate& Candidate)
{
	(void)State;
	(void)Candidate;

	FWBDeathPreventionResult Result;
	Result.bPrevented = false;
	Result.PreventionReason = NAME_None;
	return Result;
}
