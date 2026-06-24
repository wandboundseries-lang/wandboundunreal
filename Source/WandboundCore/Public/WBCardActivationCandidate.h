#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCommand.h"
#include "WBCardDefinition.h"

struct WANDBOUNDCORE_API FWBCardActivationCandidate
{
	FString ActivationCandidateId;

	int32 PlayerId = -1;
	int32 SourceUnitId = -1;

	FString SourceCardId;
	FString SourceEffectId;

	FString PublicLabel;

	FWBEffectTargetRef Target;
	FWBCardActivationCommand Command;
};

struct WANDBOUNDCORE_API FWBCardActivationCandidateSource
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;

	FWBCardDefinition CardDefinition;
	TArray<FWBEffectTargetRef> CandidateTargets;
	FWBCardActivationSourceGateContext SourceGateContext;
	TMap<FString, FWBCardActivationSourceGateContext> EffectIdToSourceGateContext;
};

struct WANDBOUNDCORE_API FWBCardActivationCandidateGenerationResult
{
	bool bOk = false;
	FString Reason;

	TArray<FWBCardActivationCandidate> Candidates;
};
