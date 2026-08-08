#pragma once

#include "CoreMinimal.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionRuntimeBootstrap.h"

struct WANDBOUNDRUNTIME_API FWBProductionMatchReplayMetadata
{
	FString OpaqueMatchId;
	FString ProductionBundleDigest;
	FString ProductionMatchSpecDigest;
	FString ActiveFormatDigest;
	FString GameStartAddendumDigest;
	int32 InitialMatchSeed = 0;
	FString ArchivePathOverride;
};

struct WANDBOUNDRUNTIME_API FWBProductionMatchReplayPersistenceResult
{
	bool bOk = false;
	FString FailureCode;
};

class WANDBOUNDRUNTIME_API WBProductionMatchReplayPersistence
{
public:
	static FString GetDefaultReplayDirectory();
	static FString GetArchivePath(const FString& OpaqueMatchId);
	static FWBProductionMatchReplayPersistenceResult WriteAtomic(
		const FString& ArchivePath,
		const FString& CanonicalJson);
	static FWBProductionMatchReplayPersistenceResult Load(
		const FString& ArchivePath,
		FString& OutCanonicalJson);
};

class WANDBOUNDRUNTIME_API FWBProductionMatchReplayRecorder
{
public:
	bool Begin(
		const FWBProductionMatchReplayMetadata& Metadata,
		const WBMatchCoordinator& Coordinator);
	void CaptureCommittedActions(const WBMatchCoordinator& Coordinator);
	bool MarkComplete(const WBMatchCoordinator& Coordinator);

	bool IsAvailable() const;
	const FWBProductionMatchReplayArchive& GetArchive() const;
	const FWBProductionMatchReplayReceipt& GetReceipt() const;
	const FString& GetArchivePathForServer() const;

private:
	void RefreshFooter(const WBMatchCoordinator& Coordinator, bool bComplete);
	bool Persist();
	void Fail(const FString& FailureCode);

	bool bAvailable = false;
	bool bFinalized = false;
	FString ArchivePath;
	FWBProductionMatchReplayArchive Archive;
	FWBProductionMatchReplayReceipt Receipt;
};

struct WANDBOUNDRUNTIME_API FWBProductionMatchReplayRunRequest
{
	FString SerializedArchive;
	FWBProductionRuntimeBootstrapRequest BootstrapRequest;
};

struct WANDBOUNDRUNTIME_API FWBProductionMatchReplayRunResult
{
	bool bValid = false;
	bool bComplete = false;
	int32 RecordsVerified = 0;
	int32 FailureRecordIndex = -1;
	FString FailureCode;
	int32 ExpectedGeneration = 0;
	int32 ActualGeneration = 0;
	int32 ExpectedRevision = 0;
	int32 ActualRevision = 0;
	FString ExpectedActionId;
	FString ExpectedDigest;
	FString ActualDigest;
	bool bTerminal = false;
	int32 WinnerPlayerId = -1;
	int32 LoserPlayerId = -1;
	FName TerminalReason;
	FName TerminalSource;
	int32 TerminalTurn = -1;
	int32 TerminalGeneration = -1;
	int32 TerminalRevision = -1;
	int32 TerminalTraceIndex = -1;
	int32 FinalGeneration = -1;
	int32 FinalRevision = -1;
	FString FinalStateDigest;
	FString FinalTraceDigest;
	TArray<int32> FinalHeroUnitIds;
	TArray<int32> FinalDiscardCounts;
	int32 FinalEquippedCardCount = -1;
};

class WANDBOUNDRUNTIME_API WBProductionMatchReplayRuntime
{
public:
	static FString BuildMatchSpecificationDigest(
		const FWBProductionMatchSpecification& Specification);
	static FWBProductionMatchReplayMetadata BuildMetadata(
		const FWBProductionRuntimeBootstrapResult& Bootstrap);
};

class WANDBOUNDRUNTIME_API FWBProductionMatchReplayRunner
{
public:
	static FWBProductionMatchReplayRunResult Run(
		const FWBProductionMatchReplayRunRequest& Request);
};
