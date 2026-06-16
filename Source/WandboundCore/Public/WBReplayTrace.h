#pragma once

#include "CoreMinimal.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBTypes.h"

struct WANDBOUNDCORE_API FWBTraceEvent
{
	FName Kind;
	FString ActionId;
	int32 PlayerId = -1;
	int32 SourceUnitId = -1;
	int32 FromPlayer = -1;
	int32 ToPlayer = -1;
	int32 TurnNumber = -1;
	FWBTile FromTile;
	FWBTile ToTile;
	bool bOk = false;
	FString Reason;
};

struct WANDBOUNDCORE_API FWBApplyActionResult
{
	bool bOk = false;
	FString Reason;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBReplayTrace
{
public:
	static FString SerializeEvent(const FWBTraceEvent& Event);
	static FString SerializeEvents(const TArray<FWBTraceEvent>& Events);
};

struct WANDBOUNDCORE_API FWBReplayDecisionRecord
{
	int32 DecisionIndex = -1;
	FWBAction ChosenAction;
	FString ChosenActionId;
	TArray<FString> LegalActionIds;
	TArray<FWBTraceEvent> TraceEvents;
	FString SerializedTraceEventsJson;
};

class WANDBOUNDCORE_API FWBReplayLog
{
public:
	const FWBReplayDecisionRecord& RecordDecision(
		int32 DecisionIndex,
		const FWBAction& ChosenAction,
		const TArray<FWBAction>& LegalActions,
		const TArray<FWBTraceEvent>& TraceEvents);

	void Reset();
	int32 Num() const;
	const TArray<FWBReplayDecisionRecord>& GetDecisionRecords() const;
	FString Serialize() const;

private:
	TArray<FWBReplayDecisionRecord> DecisionRecords;
};

struct WANDBOUNDCORE_API FWBReplayVerificationResult
{
	bool bOk = false;
	FString Reason;
	int32 FailedDecisionIndex = -1;
	int32 VerifiedDecisionCount = 0;
	FWBGameStateData FinalState;
};

class WANDBOUNDCORE_API WBReplayVerifier
{
public:
	static FWBReplayVerificationResult Verify(const FWBGameStateData& InitialState, const FString& SerializedReplayLog);
};
