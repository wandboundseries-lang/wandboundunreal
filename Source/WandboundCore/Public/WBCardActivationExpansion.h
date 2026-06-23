#pragma once

#include "CoreMinimal.h"
#include "WBCardActivationCommand.h"
#include "WBCardDefinition.h"

struct WANDBOUNDCORE_API FWBCardActivationExpansionRequest
{
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	FWBCardDefinition CardDefinition;
	FString EffectId;
	FWBEffectTargetRef Target;
	FString DebugActivationId;
};

struct WANDBOUNDCORE_API FWBCardActivationExpansionResult
{
	bool bOk = false;
	FString Reason;
	FWBCardActivationExpansionRequest Request;
	FWBCardActivationCommand Command;
};

class WANDBOUNDCORE_API WBCardActivationExpansion
{
public:
	static FWBCardActivationExpansionResult BuildActivationCommand(const FWBCardActivationExpansionRequest& Request);

	static void BuildActivationCommandCandidates(
		const FWBCardDefinition& CardDefinition,
		int32 PlayerId,
		int32 SourceUnitId,
		const TArray<FWBEffectTargetRef>& CandidateTargets,
		TArray<FWBCardActivationCommand>& OutCommands);
};
