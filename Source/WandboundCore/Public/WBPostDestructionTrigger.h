#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

struct WANDBOUNDCORE_API FWBPostDestructionTriggerResult
{
	bool bOk = false;
	FString Reason;
	bool bPendingChoice = false;
	bool bSummoned = false;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBPostDestructionTrigger
{
public:
	static FWBPostDestructionTriggerResult AdvanceToDecisionOrComplete(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 ResumePriorityPlayerId,
		int32 ResumeMatchPhase);

	static TArray<FString> EnumerateLegalChoiceActionIds(
		const FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 ViewerPlayerId);

	static FWBPostDestructionTriggerResult SubmitChoice(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FString& ActionId);

	static FWBPostDestructionTriggerResult ResolveSelectedChoice(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FString& SelectedCardInstanceId,
		const FString& ActionId);

	static FString BuildChoiceActionId(
		const FWBPendingMandatoryDeckChoiceState& Choice,
		const FString& CardInstanceId);
};
