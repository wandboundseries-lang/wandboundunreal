#include "WBProductionMatchReplayRuntime.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
void AppendInt(FString& Out, const TCHAR* Key, const int64 Value)
{
	Out += Key;
	Out += TEXT("=");
	Out += FString::Printf(TEXT("%lld;"), Value);
}

void AppendString(FString& Out, const TCHAR* Key, const FString& Value)
{
	Out += Key;
	Out += TEXT("=");
	Out += FString::Printf(TEXT("%d:"), Value.Len());
	Out += Value;
	Out += TEXT(";");
}

void AppendTile(FString& Out, const TCHAR* Key, const FWBTile& Tile)
{
	AppendInt(Out, *FString::Printf(TEXT("%s.x"), Key), Tile.X);
	AppendInt(Out, *FString::Printf(TEXT("%s.y"), Key), Tile.Y);
}

FString SafeArchiveStem(const FString& OpaqueMatchId)
{
	FString Result = OpaqueMatchId.IsEmpty()
		? FString(TEXT("match"))
		: OpaqueMatchId;
	for (TCHAR& Character : Result)
	{
		if (!FChar::IsAlnum(Character)
			&& Character != TEXT('-')
			&& Character != TEXT('_'))
		{
			Character = TEXT('_');
		}
	}
	return Result.Left(96);
}

bool BuildCanonicalLegalEntries(
	const TArray<FWBMatchLegalAction>& Actions,
	TArray<FString>& OutEntries)
{
	OutEntries.Reset();
	OutEntries.Reserve(Actions.Num());
	for (const FWBMatchLegalAction& Action : Actions)
	{
		FString Family;
		if (!WBMatchCoordinator::ClassifyReplayActionFamily(
			Action,
			Family))
		{
			return false;
		}
		OutEntries.Add(FString::Printf(
			TEXT("%s|p%d|%s"),
			*Family,
			Action.PlayerId,
			*Action.ActionId));
	}
	return true;
}

FWBProductionMatchReplayRunResult Failure(
	const FString& Code,
	const int32 RecordIndex = -1)
{
	FWBProductionMatchReplayRunResult Result;
	Result.FailureCode = Code;
	Result.FailureRecordIndex = RecordIndex;
	return Result;
}
}

FString WBProductionMatchReplayPersistence::GetDefaultReplayDirectory()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Wandbound/Replays"));
}

FString WBProductionMatchReplayPersistence::GetArchivePath(
	const FString& OpaqueMatchId)
{
	return FPaths::Combine(
		GetDefaultReplayDirectory(),
		SafeArchiveStem(OpaqueMatchId)
			+ TEXT(".wbpmr.json"));
}

FWBProductionMatchReplayPersistenceResult
WBProductionMatchReplayPersistence::WriteAtomic(
	const FString& ArchivePath,
	const FString& CanonicalJson)
{
	FWBProductionMatchReplayPersistenceResult Result;
	if (ArchivePath.IsEmpty())
	{
		Result.FailureCode = TEXT("replay_archive_unavailable");
		return Result;
	}
	const FString Directory = FPaths::GetPath(ArchivePath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		Result.FailureCode = TEXT("replay_write_failed");
		return Result;
	}
	const FString TemporaryPath = ArchivePath + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(
		CanonicalJson,
		*TemporaryPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.FailureCode = TEXT("replay_write_failed");
		return Result;
	}
	if (!IFileManager::Get().Move(
		*ArchivePath,
		*TemporaryPath,
		true,
		true,
		false,
		true))
	{
		Result.FailureCode = TEXT("replay_replace_failed");
		return Result;
	}
	Result.bOk = true;
	return Result;
}

FWBProductionMatchReplayPersistenceResult
WBProductionMatchReplayPersistence::Load(
	const FString& ArchivePath,
	FString& OutCanonicalJson)
{
	FWBProductionMatchReplayPersistenceResult Result;
	OutCanonicalJson.Reset();
	if (ArchivePath.IsEmpty()
		|| !FFileHelper::LoadFileToString(
			OutCanonicalJson,
			*ArchivePath))
	{
		Result.FailureCode = TEXT("replay_archive_unavailable");
		return Result;
	}
	Result.bOk = true;
	return Result;
}

bool FWBProductionMatchReplayRecorder::Begin(
	const FWBProductionMatchReplayMetadata& Metadata,
	const WBMatchCoordinator& Coordinator)
{
	bFinalized = false;
	Archive = FWBProductionMatchReplayArchive();
	Archive.Header.SchemaVersion =
		WBProductionMatchReplay::SchemaVersion;
	Archive.Header.OpaqueMatchId = Metadata.OpaqueMatchId;
	Archive.Header.RulesCompatibilityVersion =
		WBProductionMatchReplay::RulesCompatibilityVersion;
	Archive.Header.ProductionBundleDigest =
		Metadata.ProductionBundleDigest;
	Archive.Header.ProductionMatchSpecDigest =
		Metadata.ProductionMatchSpecDigest;
	Archive.Header.ActiveFormatDigest =
		Metadata.ActiveFormatDigest;
	Archive.Header.GameStartAddendumDigest =
		Metadata.GameStartAddendumDigest;
	Archive.Header.InitialMatchSeed =
		Metadata.InitialMatchSeed;
	Archive.Header.InitialCoordinatorGeneration =
		Coordinator.GetCoordinatorGeneration();
	Archive.Header.InitialCoordinatorRevision =
		Coordinator.GetCoordinatorRevision();
	Archive.Header.InitialStateDigest =
		Coordinator.GetInitialStateDigest();
	Archive.Header.InitialTraceDigest =
		Coordinator.GetInitialTraceDigest();
	ArchivePath = Metadata.ArchivePathOverride.IsEmpty()
		? WBProductionMatchReplayPersistence::GetArchivePath(
			Metadata.OpaqueMatchId)
		: Metadata.ArchivePathOverride;
	bAvailable = Coordinator.IsInitialized()
		&& !Metadata.OpaqueMatchId.IsEmpty()
		&& !Metadata.ProductionBundleDigest.IsEmpty()
		&& !Metadata.ProductionMatchSpecDigest.IsEmpty();
	if (!bAvailable)
	{
		Fail(TEXT("replay_recording_disabled"));
		return false;
	}
	RefreshFooter(Coordinator, Coordinator.GetState().bGameOver);
	return Persist();
}

void FWBProductionMatchReplayRecorder::CaptureCommittedActions(
	const WBMatchCoordinator& Coordinator)
{
	if (!bAvailable || bFinalized)
	{
		return;
	}
	const TArray<FWBMatchCommittedActionRecord>& Committed =
		Coordinator.GetCommittedActionRecords();
	for (int32 Index = Archive.Records.Num();
		Index < Committed.Num(); ++Index)
	{
		FWBProductionMatchReplayActionRecord Record;
		static_cast<FWBMatchCommittedActionRecord&>(Record) =
			Committed[Index];
		Archive.Records.Add(MoveTemp(Record));
	}
	const bool bTerminal = Coordinator.GetState().bGameOver;
	RefreshFooter(Coordinator, bTerminal);
	if (Persist() && bTerminal)
	{
		bFinalized = true;
	}
}

bool FWBProductionMatchReplayRecorder::MarkComplete(
	const WBMatchCoordinator& Coordinator)
{
	if (!bAvailable)
	{
		return false;
	}
	if (bFinalized)
	{
		return true;
	}
	CaptureCommittedActions(Coordinator);
	if (!bAvailable)
	{
		return false;
	}
	if (bFinalized)
	{
		return true;
	}
	if (!Coordinator.GetState().bGameOver)
	{
		return false;
	}
	RefreshFooter(Coordinator, true);
	if (!Persist())
	{
		return false;
	}
	bFinalized = true;
	return true;
}

bool FWBProductionMatchReplayRecorder::IsAvailable() const
{
	return bAvailable;
}

const FWBProductionMatchReplayArchive&
FWBProductionMatchReplayRecorder::GetArchive() const
{
	return Archive;
}

const FWBProductionMatchReplayReceipt&
FWBProductionMatchReplayRecorder::GetReceipt() const
{
	return Receipt;
}

const FString&
FWBProductionMatchReplayRecorder::GetArchivePathForServer() const
{
	return ArchivePath;
}

void FWBProductionMatchReplayRecorder::RefreshFooter(
	const WBMatchCoordinator& Coordinator,
	const bool bComplete)
{
	Archive.Footer.bTerminal = Coordinator.GetState().bGameOver;
	Archive.Footer.bComplete = bComplete
		&& Archive.Footer.bTerminal;
	Archive.Footer.Winner = Archive.Footer.bTerminal
		? Coordinator.GetState().WinnerPlayerId
		: -1;
	Archive.Footer.Loser = Archive.Footer.Winner >= 0
		? Coordinator.GetState().TerminalOutcome.LoserPlayerId
		: -1;
	Archive.Footer.TerminalReason = Archive.Footer.bTerminal
		? WBTerminalOutcomeNames::ReasonToName(
			Coordinator.GetState().TerminalOutcome.Reason)
		: FName();
	Archive.Footer.TerminalSource = Archive.Footer.bTerminal
		? WBTerminalOutcomeNames::SourceToName(
			Coordinator.GetState().TerminalOutcome.Source)
		: FName();
	Archive.Footer.TerminalTurn = Archive.Footer.bTerminal
		? Coordinator.GetState().TerminalOutcome.TurnNumber
		: -1;
	Archive.Footer.TerminalGeneration = Archive.Footer.bTerminal
		? Coordinator.GetCoordinatorGeneration()
		: -1;
	Archive.Footer.TerminalRevision = Archive.Footer.bTerminal
		? Coordinator.GetState().TerminalOutcome.CoordinatorRevision
		: -1;
	Archive.Footer.TerminalTraceIndex = Archive.Footer.bTerminal
		? Coordinator.GetState().TerminalOutcome.TraceIndex
		: -1;
	Archive.Footer.FinalGeneration =
		Coordinator.GetCoordinatorGeneration();
	Archive.Footer.FinalRevision =
		Coordinator.GetCoordinatorRevision();
	Archive.Footer.FinalStateDigest =
		Coordinator.GetCurrentStateDigest();
	Archive.Footer.FinalTraceDigest =
		Coordinator.GetCurrentTraceDigest();
	WBProductionMatchReplay::RebuildIntegrity(Archive);
	Receipt = WBProductionMatchReplay::BuildReceipt(
		Archive,
		bAvailable);
}

bool FWBProductionMatchReplayRecorder::Persist()
{
	if (!bAvailable)
	{
		return false;
	}
	const FWBProductionMatchReplayPersistenceResult Result =
		WBProductionMatchReplayPersistence::WriteAtomic(
			ArchivePath,
			WBProductionMatchReplay::Serialize(Archive));
	if (!Result.bOk)
	{
		Fail(Result.FailureCode);
		return false;
	}
	Receipt = WBProductionMatchReplay::BuildReceipt(
		Archive,
		true);
	return true;
}

void FWBProductionMatchReplayRecorder::Fail(
	const FString& FailureCode)
{
	bAvailable = false;
	Receipt = WBProductionMatchReplay::BuildReceipt(
		Archive,
		false,
		FailureCode.IsEmpty()
			? FString(TEXT("replay_archive_unavailable"))
			: FailureCode);
}

FString WBProductionMatchReplayRuntime::BuildMatchSpecificationDigest(
	const FWBProductionMatchSpecification& Specification)
{
	FString Canonical;
	AppendInt(Canonical, TEXT("schema_version"), Specification.SchemaVersion);
	AppendString(Canonical, TEXT("match_id"), Specification.MatchId);
	AppendInt(Canonical, TEXT("seed"), Specification.Seed);
	AppendInt(Canonical, TEXT("first_player"), Specification.FirstPlayerId);
	AppendInt(Canonical, TEXT("initial_draw_count"), Specification.InitialDrawCount);
	AppendString(Canonical, TEXT("bundle_digest"), Specification.DefinitionBundleDigest);
	AppendString(Canonical, TEXT("format_id"), Specification.ActiveFormatId);
	AppendInt(Canonical, TEXT("format_version"), Specification.ActiveFormatVersion);
	AppendString(Canonical, TEXT("format_digest"), Specification.ActiveFormatDigest);
	AppendString(Canonical, TEXT("addendum_id"), Specification.GameStartAddendumId);
	AppendInt(Canonical, TEXT("addendum_version"), Specification.GameStartAddendumVersion);
	AppendString(Canonical, TEXT("addendum_digest"), Specification.GameStartAddendumDigest);
	TArray<FWBProductionPlayerMatchSpecification> Players =
		Specification.Players;
	Players.Sort([](
		const FWBProductionPlayerMatchSpecification& A,
		const FWBProductionPlayerMatchSpecification& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	AppendInt(Canonical, TEXT("player_count"), Players.Num());
	for (const FWBProductionPlayerMatchSpecification& Player : Players)
	{
		AppendInt(Canonical, TEXT("player.id"), Player.PlayerId);
		AppendString(Canonical, TEXT("player.hero"), Player.HeroDefinitionId);
		AppendTile(Canonical, TEXT("player.spawn"), Player.HeroSpawnTile);
		AppendInt(Canonical, TEXT("player.deck_count"), Player.OrderedDeckDefinitionIds.Num());
		for (const FString& CardId : Player.OrderedDeckDefinitionIds)
		{
			AppendString(Canonical, TEXT("player.deck_card"), CardId);
		}
		AppendInt(Canonical, TEXT("player.trap_count"), Player.SetupTrapDefinitionIds.Num());
		for (const FString& CardId : Player.SetupTrapDefinitionIds)
		{
			AppendString(Canonical, TEXT("player.trap"), CardId);
		}
		AppendInt(Canonical, TEXT("player.npc_count"), Player.SetupNPCDefinitionIds.Num());
		for (const FString& CardId : Player.SetupNPCDefinitionIds)
		{
			AppendString(Canonical, TEXT("player.npc"), CardId);
		}
	}
	TArray<FWBSetupMarkerPlacement> Markers =
		Specification.MarkerPlacements;
	Markers.Sort([](
		const FWBSetupMarkerPlacement& A,
		const FWBSetupMarkerPlacement& B)
	{
		if (A.PlacementOrder != B.PlacementOrder)
			return A.PlacementOrder < B.PlacementOrder;
		if (A.PlayerId != B.PlayerId)
			return A.PlayerId < B.PlayerId;
		if (A.Tile.Y != B.Tile.Y) return A.Tile.Y < B.Tile.Y;
		return A.Tile.X < B.Tile.X;
	});
	AppendInt(Canonical, TEXT("marker_count"), Markers.Num());
	for (const FWBSetupMarkerPlacement& Marker : Markers)
	{
		AppendInt(Canonical, TEXT("marker.player"), Marker.PlayerId);
		AppendInt(Canonical, TEXT("marker.type"), static_cast<int32>(Marker.Type));
		AppendTile(Canonical, TEXT("marker.tile"), Marker.Tile);
		AppendString(Canonical, TEXT("marker.definition"), Marker.DefinitionId);
		AppendInt(Canonical, TEXT("marker.order"), Marker.PlacementOrder);
	}
	return WBProductionMatchReplay::HashUtf8(Canonical);
}

FWBProductionMatchReplayMetadata
WBProductionMatchReplayRuntime::BuildMetadata(
	const FWBProductionRuntimeBootstrapResult& Bootstrap)
{
	FWBProductionMatchReplayMetadata Metadata;
	Metadata.OpaqueMatchId =
		Bootstrap.MatchSpecification.MatchId;
	Metadata.ProductionBundleDigest = Bootstrap.Database.IsValid()
		? Bootstrap.Database->ContentDigest
		: FString();
	Metadata.ProductionMatchSpecDigest =
		BuildMatchSpecificationDigest(
			Bootstrap.MatchSpecification);
	Metadata.ActiveFormatDigest =
		Bootstrap.ActiveFormat.Digest;
	Metadata.GameStartAddendumDigest =
		Bootstrap.GameStartAddendum.Digest;
	Metadata.InitialMatchSeed =
		Bootstrap.InitializationRequest.Seed;
	return Metadata;
}

FWBProductionMatchReplayRunResult FWBProductionMatchReplayRunner::Run(
	const FWBProductionMatchReplayRunRequest& Request)
{
	const FWBProductionMatchReplayValidationResult Validation =
		WBProductionMatchReplay::DeserializeAndValidate(
			Request.SerializedArchive);
	if (!Validation.bValid)
	{
		return Failure(
			Validation.FailureCode,
			Validation.FailureRecordIndex);
	}
	const FWBProductionMatchReplayArchive& Archive =
		Validation.Archive;
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		WBProductionRuntimeBootstrap::Build(
			Request.BootstrapRequest);
	if (!Bootstrap.bOk)
	{
		return Failure(TEXT("replay_archive_unavailable"));
	}
	const FWBProductionMatchReplayMetadata Metadata =
		WBProductionMatchReplayRuntime::BuildMetadata(Bootstrap);
	if (Archive.Header.ProductionBundleDigest
		!= Metadata.ProductionBundleDigest)
	{
		return Failure(TEXT("replay_bundle_digest_mismatch"));
	}
	if (Archive.Header.ProductionMatchSpecDigest
		!= Metadata.ProductionMatchSpecDigest)
	{
		return Failure(TEXT("replay_match_spec_digest_mismatch"));
	}
	if (Archive.Header.ActiveFormatDigest
		!= Metadata.ActiveFormatDigest)
	{
		return Failure(TEXT("replay_format_digest_mismatch"));
	}
	if (Archive.Header.GameStartAddendumDigest
		!= Metadata.GameStartAddendumDigest)
	{
		return Failure(TEXT("replay_addendum_digest_mismatch"));
	}
	if (Archive.Header.InitialMatchSeed
		!= Bootstrap.InitializationRequest.Seed)
	{
		return Failure(TEXT("replay_match_spec_digest_mismatch"));
	}

	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(
			Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		return Failure(TEXT("replay_submission_rejected"));
	}
	if (Coordinator.GetCoordinatorGeneration()
		!= Archive.Header.InitialCoordinatorGeneration
		|| Coordinator.GetCoordinatorRevision()
		!= Archive.Header.InitialCoordinatorRevision)
	{
		FWBProductionMatchReplayRunResult Result =
			Failure(TEXT("replay_revision_mismatch"));
		Result.ExpectedGeneration =
			Archive.Header.InitialCoordinatorGeneration;
		Result.ActualGeneration =
			Coordinator.GetCoordinatorGeneration();
		Result.ExpectedRevision =
			Archive.Header.InitialCoordinatorRevision;
		Result.ActualRevision =
			Coordinator.GetCoordinatorRevision();
		return Result;
	}
	if (Coordinator.GetInitialStateDigest()
		!= Archive.Header.InitialStateDigest)
	{
		FWBProductionMatchReplayRunResult Result =
			Failure(TEXT("replay_initial_state_mismatch"));
		Result.ExpectedDigest = Archive.Header.InitialStateDigest;
		Result.ActualDigest = Coordinator.GetInitialStateDigest();
		return Result;
	}
	if (Coordinator.GetInitialTraceDigest()
		!= Archive.Header.InitialTraceDigest)
	{
		FWBProductionMatchReplayRunResult Result =
			Failure(TEXT("replay_initial_trace_mismatch"));
		Result.ExpectedDigest = Archive.Header.InitialTraceDigest;
		Result.ActualDigest = Coordinator.GetInitialTraceDigest();
		return Result;
	}

	FWBProductionMatchReplayRunResult Result;
	for (int32 Index = 0; Index < Archive.Records.Num(); ++Index)
	{
		const FWBProductionMatchReplayActionRecord& Record =
			Archive.Records[Index];
		Result.FailureRecordIndex = Index;
		Result.ExpectedActionId = Record.ChosenActionId;
		if (Coordinator.GetState().bGameOver)
		{
			Result.FailureCode = TEXT("replay_post_terminal_record");
			return Result;
		}
		if (Coordinator.GetCoordinatorGeneration()
			!= Record.BeforeGeneration)
		{
			Result.FailureCode = TEXT("replay_generation_mismatch");
			Result.ExpectedGeneration = Record.BeforeGeneration;
			Result.ActualGeneration = Coordinator.GetCoordinatorGeneration();
			return Result;
		}
		if (Coordinator.GetCoordinatorRevision()
			!= Record.BeforeRevision)
		{
			Result.FailureCode = TEXT("replay_revision_mismatch");
			Result.ExpectedRevision = Record.BeforeRevision;
			Result.ActualRevision = Coordinator.GetCoordinatorRevision();
			return Result;
		}
		if (Coordinator.GetCurrentStateDigest()
			!= Record.BeforeStateDigest)
		{
			Result.FailureCode = TEXT("replay_state_digest_mismatch");
			Result.ExpectedDigest = Record.BeforeStateDigest;
			Result.ActualDigest = Coordinator.GetCurrentStateDigest();
			return Result;
		}
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		if (!Legal.bOk)
		{
			Result.FailureCode = TEXT("replay_submission_rejected");
			return Result;
		}
		TArray<FString> LegalEntries;
		if (!BuildCanonicalLegalEntries(Legal.Actions, LegalEntries))
		{
			Result.FailureCode = TEXT("replay_action_family_mismatch");
			return Result;
		}
		const FString LegalDigest =
			WBProductionMatchReplay::BuildLegalActionSetDigest(
				LegalEntries);
		if (LegalDigest != Record.LegalActionSetDigest)
		{
			Result.FailureCode = TEXT("replay_wrong_decision");
			Result.ExpectedDigest = Record.LegalActionSetDigest;
			Result.ActualDigest = LegalDigest;
			return Result;
		}
		const FString DecisionId =
			WBProductionMatchReplay::BuildDecisionId(
				Coordinator.GetCoordinatorGeneration(),
				Coordinator.GetCoordinatorRevision(),
				Coordinator.GetState().PriorityPlayer,
				static_cast<int32>(Coordinator.GetMatchPhase()),
				LegalDigest);
		if (DecisionId != Record.ExpectedDecisionId)
		{
			Result.FailureCode = TEXT("replay_wrong_decision");
			return Result;
		}
		const FWBMatchLegalAction* Selected = nullptr;
		int32 MatchCount = 0;
		for (const FWBMatchLegalAction& Action : Legal.Actions)
		{
			if (Action.ActionId == Record.ChosenActionId)
			{
				Selected = &Action;
				++MatchCount;
			}
		}
		if (Selected == nullptr || MatchCount != 1)
		{
			Result.FailureCode = TEXT("replay_action_not_legal");
			return Result;
		}
		if (Selected->PlayerId != Record.ActingPlayer)
		{
			Result.FailureCode = TEXT("replay_wrong_acting_player");
			return Result;
		}
		FString Family;
		if (!WBMatchCoordinator::ClassifyReplayActionFamily(
			*Selected,
			Family)
			|| Family != Record.ActionFamily)
		{
			Result.FailureCode = TEXT("replay_action_family_mismatch");
			return Result;
		}
		const FWBMatchOperationResult Submitted =
			Coordinator.SubmitActionId(
				Record.ActingPlayer,
				Record.ChosenActionId);
		if (!Submitted.bOk)
		{
			Result.FailureCode = TEXT("replay_submission_rejected");
			return Result;
		}
		if (Submitted.CoordinatorGeneration
			!= Record.AfterGeneration)
		{
			Result.FailureCode = TEXT("replay_generation_mismatch");
			Result.ExpectedGeneration = Record.AfterGeneration;
			Result.ActualGeneration = Submitted.CoordinatorGeneration;
			return Result;
		}
		if (Submitted.CoordinatorRevision
			!= Record.AfterRevision)
		{
			Result.FailureCode = TEXT("replay_revision_mismatch");
			Result.ExpectedRevision = Record.AfterRevision;
			Result.ActualRevision = Submitted.CoordinatorRevision;
			return Result;
		}
		if (Submitted.bCompleted != Record.bCompleted
			|| Submitted.bPendingDecision != Record.bPendingDecision
			|| Submitted.PendingPlayerId != Record.PendingPlayer)
		{
			Result.FailureCode = TEXT("replay_pending_state_mismatch");
			return Result;
		}
		if (Submitted.TraceBeginIndex != Record.TraceStart
			|| Submitted.TraceEndIndex != Record.TraceEnd)
		{
			Result.FailureCode = TEXT("replay_trace_range_mismatch");
			return Result;
		}
		const FString TraceDigest =
			WBProductionMatchReplay::BuildTraceDigest(
				Submitted.TraceEvents);
		if (TraceDigest != Record.TraceDigest)
		{
			Result.FailureCode = TEXT("replay_trace_digest_mismatch");
			Result.ExpectedDigest = Record.TraceDigest;
			Result.ActualDigest = TraceDigest;
			return Result;
		}
		const FString StateDigest = Coordinator.GetCurrentStateDigest();
		if (StateDigest != Record.AfterStateDigest)
		{
			Result.FailureCode = TEXT("replay_state_digest_mismatch");
			Result.ExpectedDigest = Record.AfterStateDigest;
			Result.ActualDigest = StateDigest;
			return Result;
		}
		if (Submitted.bTerminal && !Record.bTerminal)
		{
			Result.FailureCode = TEXT("replay_terminal_unexpected");
			return Result;
		}
		if (!Submitted.bTerminal && Record.bTerminal)
		{
			Result.FailureCode = TEXT("replay_terminal_expected");
			return Result;
		}
		if (Record.bTerminal)
		{
			if (Submitted.WinnerPlayerId != Record.WinnerPlayer)
			{
				Result.FailureCode = TEXT("replay_winner_mismatch");
				return Result;
			}
			if (Submitted.LoserPlayerId != Record.LoserPlayer)
			{
				Result.FailureCode = TEXT("replay_loser_mismatch");
				return Result;
			}
			if (Submitted.TerminalReason != Record.TerminalReason)
			{
				Result.FailureCode = TEXT("replay_terminal_reason_mismatch");
				return Result;
			}
			if (Submitted.TerminalSource != Record.TerminalSource)
			{
				Result.FailureCode = TEXT("replay_terminal_source_mismatch");
				return Result;
			}
			if (Submitted.TerminalTurnNumber != Record.TerminalTurn)
			{
				Result.FailureCode = TEXT("replay_terminal_turn_mismatch");
				return Result;
			}
			if (Submitted.TerminalRevision != Record.TerminalRevision
				|| Submitted.TerminalTraceIndex != Record.TerminalTraceIndex)
			{
				Result.FailureCode = TEXT("replay_terminal_metadata_mismatch");
				return Result;
			}
			if (Index + 1 != Archive.Records.Num())
			{
				Result.FailureCode = TEXT("replay_post_terminal_record");
				return Result;
			}
		}
		++Result.RecordsVerified;
	}

	if (Archive.Footer.bComplete && !Archive.Footer.bTerminal)
	{
		Result.FailureCode = TEXT("replay_footer_terminal_mismatch");
		return Result;
	}
	if (Archive.Footer.bTerminal != Coordinator.GetState().bGameOver)
	{
		Result.FailureCode = Archive.Footer.bTerminal
			? FString(TEXT("replay_terminal_expected"))
			: FString(TEXT("replay_terminal_unexpected"));
		return Result;
	}
	if (Archive.Footer.bComplete && !Coordinator.GetState().bGameOver)
	{
		Result.FailureCode = TEXT("replay_terminal_expected");
		return Result;
	}
	if (Coordinator.GetState().bGameOver)
	{
		const FWBTerminalOutcome& Outcome = Coordinator.GetState().TerminalOutcome;
		if (Archive.Footer.Winner != Outcome.WinnerPlayerId)
		{
			Result.FailureCode = TEXT("replay_winner_mismatch");
			return Result;
		}
		if (Archive.Footer.Loser != Outcome.LoserPlayerId)
		{
			Result.FailureCode = TEXT("replay_loser_mismatch");
			return Result;
		}
		if (Archive.Footer.TerminalReason
			!= WBTerminalOutcomeNames::ReasonToName(Outcome.Reason))
		{
			Result.FailureCode = TEXT("replay_terminal_reason_mismatch");
			return Result;
		}
		if (Archive.Footer.TerminalSource
			!= WBTerminalOutcomeNames::SourceToName(Outcome.Source))
		{
			Result.FailureCode = TEXT("replay_terminal_source_mismatch");
			return Result;
		}
		if (Archive.Footer.TerminalTurn != Outcome.TurnNumber)
		{
			Result.FailureCode = TEXT("replay_terminal_turn_mismatch");
			return Result;
		}
		if (Archive.Footer.TerminalGeneration
			!= Coordinator.GetCoordinatorGeneration()
			|| Archive.Footer.TerminalRevision
				!= Outcome.CoordinatorRevision
			|| Archive.Footer.TerminalTraceIndex != Outcome.TraceIndex)
		{
			Result.FailureCode = TEXT("replay_terminal_metadata_mismatch");
			return Result;
		}
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		if (!Legal.bOk || !Legal.Actions.IsEmpty())
		{
			Result.FailureCode = TEXT("replay_legal_actions_after_terminal");
			return Result;
		}
	}

	const bool bFooterMatches =
		Archive.Footer.FinalGeneration
			== Coordinator.GetCoordinatorGeneration()
		&& Archive.Footer.FinalRevision
			== Coordinator.GetCoordinatorRevision()
		&& Archive.Footer.FinalStateDigest
			== Coordinator.GetCurrentStateDigest()
		&& Archive.Footer.FinalTraceDigest
			== Coordinator.GetCurrentTraceDigest()
		&& Archive.Footer.bTerminal
			== Coordinator.GetState().bGameOver;
	if (!bFooterMatches)
	{
		Result.FailureCode = TEXT("replay_footer_mismatch");
		return Result;
	}
	Result.bValid = true;
	Result.bComplete = Archive.Footer.bComplete;
	Result.bTerminal = Archive.Footer.bTerminal;
	Result.WinnerPlayerId = Archive.Footer.Winner;
	Result.LoserPlayerId = Archive.Footer.Loser;
	Result.TerminalReason = Archive.Footer.TerminalReason;
	Result.TerminalSource = Archive.Footer.TerminalSource;
	Result.TerminalTurn = Archive.Footer.TerminalTurn;
	Result.TerminalGeneration = Archive.Footer.TerminalGeneration;
	Result.TerminalRevision = Archive.Footer.TerminalRevision;
	Result.TerminalTraceIndex = Archive.Footer.TerminalTraceIndex;
	Result.FinalGeneration = Coordinator.GetCoordinatorGeneration();
	Result.FinalRevision = Coordinator.GetCoordinatorRevision();
	Result.FinalStateDigest = Coordinator.GetCurrentStateDigest();
	Result.FinalTraceDigest = Coordinator.GetCurrentTraceDigest();
	for (const FWBPlayerStateData& Player : Coordinator.GetState().Players)
	{
		Result.FinalHeroUnitIds.Add(Player.HeroUnitId);
		const auto* Zones = Coordinator.GetState().GetCardZoneState().PlayerZones.FindByPredicate(
			[&Player](const auto& Candidate)
			{
				return Candidate.PlayerId == Player.PlayerId;
			});
		Result.FinalDiscardCounts.Add(Zones != nullptr ? Zones->Discard.Num() : -1);
	}
	Result.FinalEquippedCardCount =
		Coordinator.GetState().GetCardZoneState().EquippedCards.Num();
	Result.FailureRecordIndex = -1;
	Result.FailureCode.Reset();
	return Result;
}
