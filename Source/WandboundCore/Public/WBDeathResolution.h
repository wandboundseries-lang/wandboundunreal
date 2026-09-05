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

enum class EWBDestructionEquipmentDisposition : uint8
{
	Discard,
	DetachForContinuation
};

enum class EWBDestructionPendingAttackPolicy : uint8
{
	ClearIfParticipant,
	PreserveForRedirect
};

enum class EWBDestructionTerminalPolicy : uint8
{
	CommitImmediately,
	DeferToComposition
};

struct WANDBOUNDCORE_API FWBUnitDestructionRequest
{
	int32 TargetUnitId = INDEX_NONE;
	EWBUnitDestructionCause Cause = EWBUnitDestructionCause::Unknown;
	int32 ResolutionOrder = 0;
	EWBDestructionEquipmentDisposition EquipmentDisposition =
		EWBDestructionEquipmentDisposition::Discard;
	EWBDestructionPendingAttackPolicy PendingAttackPolicy =
		EWBDestructionPendingAttackPolicy::ClearIfParticipant;
	EWBDestructionTerminalPolicy TerminalPolicy =
		EWBDestructionTerminalPolicy::CommitImmediately;
	EWBTerminalSource TerminalSource = EWBTerminalSource::Unknown;
};

struct WANDBOUNDCORE_API FWBUnitDestructionResult
{
	bool bOk = false;
	bool bDestroyed = false;
	bool bPrevented = false;
	FString Reason;
	FWBUnitDestructionSnapshot Snapshot;
	TArray<FWBTraceEvent> TraceEvents;
};

class WANDBOUNDCORE_API WBDeathResolution
{
public:
	static FWBDeathPreventionResult EvaluateDeathPrevention(
		const FWBGameStateData& State,
		const FWBDeathResolutionCandidate& Candidate);

	static bool BuildSuccessfulDestructionSnapshot(
		const FWBGameStateData& State,
		int32 UnitId,
		EWBUnitDestructionCause Cause,
		int32 ResolutionOrder,
		FWBUnitDestructionSnapshot& OutSnapshot,
		FString& OutReason);

	static void QueueSuccessfulDestructionEvent(
		FWBGameStateData& State,
		FWBUnitDestructionSnapshot Snapshot);

	static FWBUnitDestructionResult ApplyGenuineUnitDestruction(
		FWBGameStateData& State,
		const FWBUnitDestructionRequest& Request);

	static FWBApplyActionResult CommitDeferredHeroDestruction(
		FWBGameStateData& State,
		const FWBUnitDestructionSnapshot& Snapshot,
		EWBTerminalSource TerminalSource);

	static FWBApplyActionResult ApplyZeroHPDeathResolution(
		FWBGameStateData& State,
		EWBUnitDestructionCause Cause = EWBUnitDestructionCause::Unknown);

	static FWBApplyActionResult ApplyExplicitUnitDestruction(
		FWBGameStateData& State,
		int32 UnitId);
};
