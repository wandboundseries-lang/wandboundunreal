#pragma once

#include "CoreMinimal.h"
#include "WBCardDefinitionRepository.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"

enum class EWBTurnStartSequencePhase : uint8
{
	NotStarted,
	Draw,
	MPRoll,
	ResourceReset,
	StatusResolution,
	EffectCollection,
	EffectResolution,
	Complete,
	Terminal
};

struct WANDBOUNDCORE_API FWBTurnStartTriggerInstance
{
	FString StableTriggerId;
	int32 ControllerPlayerId = -1;
	int32 SourceUnitId = -1;
	FString SourceCardId;
	FWBTurnStartTriggerDefinition Definition;
};

struct WANDBOUNDCORE_API FWBTurnStartSequenceState
{
	EWBTurnStartSequencePhase Phase =
		EWBTurnStartSequencePhase::NotStarted;
	int32 ActivePlayerId = -1;
	int32 TurnNumber = -1;
	int32 MPRoll = -1;
	bool bDrawSkipped = false;
	bool bDrawCompleted = false;
	bool bMPGenerated = false;
	bool bResourcesReset = false;
	bool bStatusesResolved = false;
	bool bEffectsResolved = false;
	bool bCompleted = false;
	TArray<FWBTurnStartTriggerInstance> PendingTriggers;
};

struct WANDBOUNDCORE_API FWBTurnStartSequenceResult
{
	bool bOk = false;
	bool bCompleted = false;
	bool bChoiceRequired = false;
	bool bTerminal = false;
	FString Reason;
	TArray<FString> LegalChoiceActionIds;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBTurnStartSequence
{
public:
	static FWBTurnStartSequenceResult Begin(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		int32 ActivePlayerId,
		int32 ExplicitMPRoll,
		FWBTurnStartSequenceState& InOutSequence);

	static FWBTurnStartSequenceResult SubmitChoice(
		FWBGameStateData& State,
		const FWBCardDefinitionRepository& Repository,
		const FString& ActionId,
		FWBTurnStartSequenceState& InOutSequence);

	static TArray<FString> EnumerateLegalChoiceActionIds(
		const FWBGameStateData& State,
		const FWBTurnStartSequenceState& Sequence);
};
