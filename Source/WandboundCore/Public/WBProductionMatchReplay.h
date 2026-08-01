#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"
#include "WBReplayTrace.h"
#include "WBTurnStartSequence.h"

struct WANDBOUNDCORE_API FWBMatchCommittedActionRecord
{
	int32 RecordIndex = -1;
	int32 ActingPlayer = -1;
	FString ActionFamily;
	FString ChosenActionId;
	FString ExpectedDecisionId;
	int32 BeforeGeneration = 0;
	int32 BeforeRevision = 0;
	FString BeforeStateDigest;
	FString LegalActionSetDigest;
	int32 AfterGeneration = 0;
	int32 AfterRevision = 0;
	bool bCompleted = false;
	bool bPendingDecision = false;
	int32 PendingPlayer = -1;
	bool bTerminal = false;
	int32 TraceStart = 0;
	int32 TraceEnd = 0;
	FString TraceDigest;
	FString AfterStateDigest;
};

struct WANDBOUNDCORE_API FWBProductionMatchReplayHeader
{
	int32 SchemaVersion = 1;
	FString ReplayFormatId = TEXT("WandboundProductionMatchReplay");
	FString OpaqueMatchId;
	int32 RulesCompatibilityVersion = 1;
	FString ProductionBundleDigest;
	FString ProductionMatchSpecDigest;
	FString ActiveFormatDigest;
	FString GameStartAddendumDigest;
	int32 InitialMatchSeed = 0;
	int32 InitialCoordinatorGeneration = 0;
	int32 InitialCoordinatorRevision = 0;
	FString InitialStateDigest;
	FString InitialTraceDigest;
	FString PreviousRecordHash;
	FString HeaderHash;
};

struct WANDBOUNDCORE_API FWBProductionMatchReplayActionRecord
	: public FWBMatchCommittedActionRecord
{
	FString PreviousRecordHash;
	FString RecordHash;
};

struct WANDBOUNDCORE_API FWBProductionMatchReplayFooter
{
	bool bComplete = false;
	bool bTerminal = false;
	int32 Winner = -1;
	int32 Loser = -1;
	int32 FinalGeneration = 0;
	int32 FinalRevision = 0;
	int32 RecordCount = 0;
	FString FinalStateDigest;
	FString FinalTraceDigest;
	FString FinalRecordHash;
	FString ReplayDigest;
};

struct WANDBOUNDCORE_API FWBProductionMatchReplayArchive
{
	FWBProductionMatchReplayHeader Header;
	TArray<FWBProductionMatchReplayActionRecord> Records;
	FWBProductionMatchReplayFooter Footer;
};

struct WANDBOUNDCORE_API FWBProductionMatchReplayValidationResult
{
	bool bValid = false;
	FString FailureCode;
	int32 FailureRecordIndex = -1;
	FWBProductionMatchReplayArchive Archive;
};

struct WANDBOUNDCORE_API FWBProductionMatchReplayReceipt
{
	bool bAvailable = false;
	int32 SchemaVersion = 1;
	FString OpaqueMatchId;
	int32 RecordCount = 0;
	bool bComplete = false;
	bool bTerminal = false;
	FString FinalReplayDigest;
	FString FailureCode;
};

class WANDBOUNDCORE_API WBProductionMatchReplay
{
public:
	static constexpr int32 SchemaVersion = 1;
	static constexpr int32 RulesCompatibilityVersion = 1;

	static FString HashUtf8(const FString& Value);
	static FString BuildGameStateDigest(const FWBGameStateData& State);
	static FString BuildCoordinatorStateDigest(
		const FWBGameStateData& State,
		int32 MatchPhase,
		uint32 RandomState,
		const FWBTurnStartSequenceState& TurnStartSequence);
	static FString BuildTraceDigest(const TArray<FWBTraceEvent>& Events);
	static FString BuildLegalActionSetDigest(
		const TArray<FString>& CanonicalActionEntries);
	static FString BuildDecisionId(
		int32 Generation,
		int32 Revision,
		int32 ActingPlayer,
		int32 MatchPhase,
		const FString& LegalActionSetDigest);

	static void RebuildIntegrity(FWBProductionMatchReplayArchive& Archive);
	static FString Serialize(const FWBProductionMatchReplayArchive& Archive);
	static FWBProductionMatchReplayValidationResult DeserializeAndValidate(
		const FString& Json);
	static FWBProductionMatchReplayReceipt BuildReceipt(
		const FWBProductionMatchReplayArchive& Archive,
		bool bAvailable,
		const FString& FailureCode = FString());
	static FString SerializeReceipt(
		const FWBProductionMatchReplayReceipt& Receipt);
};
