#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationLegalAction.h"

class WANDBOUNDCORE_API WBCardActivationLegalActionGenerator
{
public:
	static FWBCardActivationLegalActionGenerationResult GenerateFromCandidates(
		const TArray<FWBCardActivationCandidate>& Candidates);

	static FString MakeActivationActionId(
		const FWBCardActivationCandidate& Candidate);
};
