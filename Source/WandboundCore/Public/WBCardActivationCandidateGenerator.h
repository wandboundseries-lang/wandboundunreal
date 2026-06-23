#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCandidate.h"
#include "WBGameStateData.h"

class WANDBOUNDCORE_API WBCardActivationCandidateGenerator
{
public:
	static FWBCardActivationCandidateGenerationResult GenerateCandidates(
		const FWBGameStateData& State,
		const TArray<FWBCardActivationCandidateSource>& Sources);

	static FString MakeActivationCandidateId(const FWBCardActivationCandidate& Candidate);
};
