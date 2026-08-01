#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBProductionTerminalReplaySmoke.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString TerminalFixturePath(const FString& Name)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/TerminalFixture"),
		Name);
}

FWBProductionRuntimeBootstrapRequest MakeTerminalRequest()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = TerminalFixturePath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = TerminalFixturePath(TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	return Request;
}

struct FTerminalReplayFixture
{
	bool bOk = false;
	FString Reason;
	FWBProductionRuntimeBootstrapRequest Request;
	FWBProductionRuntimeBootstrapResult Bootstrap;
	FWBProductionMatchReplayArchive Archive;
	FWBProductionMatchReplayReceipt Receipt;
	FWBProductionMatchReplayRunResult Replay;
	FString Serialized;
	FString ReceiptJson;
};

FTerminalReplayFixture BuildTerminalFixture()
{
	FTerminalReplayFixture Fixture;
	Fixture.Request = MakeTerminalRequest();
	Fixture.Bootstrap = WBProductionRuntimeBootstrap::Build(Fixture.Request);
	if (!Fixture.Bootstrap.bOk)
	{
		Fixture.Reason = Fixture.Bootstrap.Reason;
		const FWBProductionCardDatabaseLoadResult Database =
			WBProductionCardDatabase::LoadManifestSuite(
				Fixture.Request.CardBundleManifestPath);
		if (Database.Snapshot.IsValid())
		{
			Fixture.Reason += TEXT(":actual_bundle_digest=")
				+ Database.Snapshot->ContentDigest;
		}
		return Fixture;
	}
	const FWBProductionTerminalReplaySmokeResult Smoke =
		WBProductionTerminalReplaySmoke::Run(Fixture.Request);
	if (!Smoke.bOk)
	{
		Fixture.Reason = Smoke.Reason;
		return Fixture;
	}
	const FString ArchivePath =
		WBProductionMatchReplayPersistence::GetArchivePath(
			Fixture.Bootstrap.MatchSpecification.MatchId);
	if (!FFileHelper::LoadFileToString(Fixture.Serialized, *ArchivePath))
	{
		Fixture.Reason = TEXT("terminal_fixture_archive_missing");
		return Fixture;
	}
	const FWBProductionMatchReplayValidationResult Validation =
		WBProductionMatchReplay::DeserializeAndValidate(Fixture.Serialized);
	if (!Validation.bValid)
	{
		Fixture.Reason = Validation.FailureCode;
		return Fixture;
	}
	Fixture.Archive = Validation.Archive;
	Fixture.Receipt = WBProductionMatchReplay::BuildReceipt(Fixture.Archive, true);
	Fixture.ReceiptJson = WBProductionMatchReplay::SerializeReceipt(Fixture.Receipt);
	FWBProductionMatchReplayRunRequest RunRequest;
	RunRequest.SerializedArchive = Fixture.Serialized;
	RunRequest.BootstrapRequest = Fixture.Request;
	Fixture.Replay = FWBProductionMatchReplayRunner::Run(RunRequest);
	if (!Fixture.Replay.bValid)
	{
		Fixture.Reason = Fixture.Replay.FailureCode;
		return Fixture;
	}
	Fixture.bOk = true;
	return Fixture;
}

const FTerminalReplayFixture& GetTerminalFixture()
{
	static const FTerminalReplayFixture Fixture = BuildTerminalFixture();
	return Fixture;
}

bool RebuildTerminalCoordinator(WBMatchCoordinator& OutCoordinator, FString& OutReason)
{
	const FTerminalReplayFixture& Fixture = GetTerminalFixture();
	if (!Fixture.bOk)
	{
		OutReason = Fixture.Reason;
		return false;
	}
	const FWBMatchOperationResult Started = OutCoordinator.InitializeMatch(
		Fixture.Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		OutReason = Started.Reason;
		return false;
	}
	for (const FWBProductionMatchReplayActionRecord& Record : Fixture.Archive.Records)
	{
		const FWBMatchOperationResult Submitted = OutCoordinator.SubmitActionId(
			Record.ActingPlayer,
			Record.ChosenActionId);
		if (!Submitted.bOk)
		{
			OutReason = Submitted.Reason;
			return false;
		}
	}
	OutReason.Reset();
	return true;
}

FWBProductionMatchReplayRunResult RunMutatedArchive(
	FWBProductionMatchReplayArchive Archive,
	const TFunctionRef<void(FWBProductionMatchReplayArchive&)> Mutate)
{
	Mutate(Archive);
	WBProductionMatchReplay::RebuildIntegrity(Archive);
	FWBProductionMatchReplayRunRequest Request;
	Request.SerializedArchive = WBProductionMatchReplay::Serialize(Archive);
	Request.BootstrapRequest = GetTerminalFixture().Request;
	return FWBProductionMatchReplayRunner::Run(Request);
}

bool TestFixtureReady(FAutomationTestBase& Test)
{
	const FTerminalReplayFixture& Fixture = GetTerminalFixture();
	return Test.TestTrue(
		*FString::Printf(TEXT("Terminal fixture ready: %s"), *Fixture.Reason),
		Fixture.bOk);
}

bool TestTerminalLock(FAutomationTestBase& Test, const FString& Field)
{
	WBMatchCoordinator Coordinator;
	FString Reason;
	if (!Test.TestTrue(TEXT("Terminal coordinator rebuilt"),
		RebuildTerminalCoordinator(Coordinator, Reason)))
	{
		Test.AddError(Reason);
		return false;
	}
	const int32 Generation = Coordinator.GetCoordinatorGeneration();
	const int32 Revision = Coordinator.GetCoordinatorRevision();
	const FString StateDigest = Coordinator.GetCurrentStateDigest();
	const FString TraceDigest = Coordinator.GetCurrentTraceDigest();
	const int32 RecordCount = Coordinator.GetCommittedActionRecords().Num();
	const FWBMatchOperationResult Rejected =
		Coordinator.SubmitActionId(0, TEXT("end_turn:p0"));
	if (Field == TEXT("legal"))
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		return Test.TestTrue(TEXT("Terminal legal query succeeds"), Legal.bOk)
			&& Test.TestEqual(TEXT("No terminal legal actions"), Legal.Actions.Num(), 0);
	}
	if (Field == TEXT("submission"))
		return Test.TestFalse(TEXT("Submission rejected"), Rejected.bOk)
			&& Test.TestEqual(TEXT("Typed existing rejection"), Rejected.Reason, FString(TEXT("game_over")));
	if (Field == TEXT("revision"))
		return Test.TestEqual(TEXT("Revision stable"), Coordinator.GetCoordinatorRevision(), Revision);
	if (Field == TEXT("generation"))
		return Test.TestEqual(TEXT("Generation stable"), Coordinator.GetCoordinatorGeneration(), Generation);
	if (Field == TEXT("trace"))
		return Test.TestEqual(TEXT("Trace stable"), Coordinator.GetCurrentTraceDigest(), TraceDigest);
	if (Field == TEXT("state"))
		return Test.TestEqual(TEXT("State stable"), Coordinator.GetCurrentStateDigest(), StateDigest);
	return Test.TestEqual(TEXT("Replay record count stable"), Coordinator.GetCommittedActionRecords().Num(), RecordCount);
}

bool TestReceiptPrivacy(FAutomationTestBase& Test, const FString& ForbiddenField)
{
	if (!TestFixtureReady(Test)) return false;
	const FString& Json = GetTerminalFixture().ReceiptJson;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!Test.TestTrue(TEXT("Receipt parses"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}
	if (ForbiddenField == TEXT("field_count"))
		return Test.TestEqual(TEXT("Receipt remains eight fields"), Root->Values.Num(), 8);
	return Test.TestFalse(*FString::Printf(TEXT("Receipt omits %s"), *ForbiddenField),
		Root->Values.Contains(ForbiddenField));
}

bool RunTerminalAssertion(
	FAutomationTestBase& Test,
	const FString& Case)
{
	if (Case.StartsWith(TEXT("lock:")))
		return TestTerminalLock(Test, Case.RightChop(5));
	if (Case.StartsWith(TEXT("privacy:")))
		return TestReceiptPrivacy(Test, Case.RightChop(8));
	if (!TestFixtureReady(Test)) return false;
	const FTerminalReplayFixture& Fixture = GetTerminalFixture();
	const FWBProductionMatchReplayFooter& Footer = Fixture.Archive.Footer;
	const FWBProductionMatchReplayActionRecord& Final = Fixture.Archive.Records.Last();

	if (Case == TEXT("terminal"))
		return Test.TestTrue(TEXT("Match terminal"), Footer.bTerminal && Footer.bComplete);
	if (Case == TEXT("winner"))
		return Test.TestEqual(TEXT("Winner"), Footer.Winner, 0);
	if (Case == TEXT("loser"))
		return Test.TestEqual(TEXT("Loser"), Footer.Loser, 1);
	if (Case == TEXT("reason"))
		return Test.TestEqual(TEXT("Typed reason"), Footer.TerminalReason,
			FName(TEXT("hero_defeated_without_replacement")));
	if (Case == TEXT("source"))
		return Test.TestEqual(TEXT("Attack source"), Footer.TerminalSource, FName(TEXT("attack")));
	if (Case == TEXT("trace_order"))
	{
		WBMatchCoordinator Coordinator;
		FString Reason;
		if (!Test.TestTrue(TEXT("Terminal coordinator rebuilt"),
			RebuildTerminalCoordinator(Coordinator, Reason)))
		{
			Test.AddError(Reason);
			return false;
		}
		int32 HeroDefeated = INDEX_NONE;
		int32 TerminalCommitted = INDEX_NONE;
		int32 GameOver = INDEX_NONE;
		for (int32 Index = 0; Index < Coordinator.GetTraceLog().Num(); ++Index)
		{
			const FName Kind = Coordinator.GetTraceLog()[Index].Kind;
			if (Kind == FName(TEXT("hero_defeated"))) HeroDefeated = Index;
			if (Kind == FName(TEXT("terminal_state_committed"))) TerminalCommitted = Index;
			if (Kind == FName(TEXT("game_over"))) GameOver = Index;
		}
		return Test.TestTrue(TEXT("Hero defeat traced"), HeroDefeated != INDEX_NONE)
			&& Test.TestTrue(TEXT("Terminal commit follows defeat"), TerminalCommitted > HeroDefeated)
			&& Test.TestTrue(TEXT("Game over follows terminal commit"), GameOver > TerminalCommitted)
			&& Test.TestEqual(TEXT("Terminal trace index authoritative"),
				Coordinator.GetState().TerminalOutcome.TraceIndex,
				TerminalCommitted);
	}
	if (Case == TEXT("final_record"))
		return Test.TestTrue(TEXT("Final accepted action terminal"), Final.bTerminal && Final.bCompleted && !Final.bPendingDecision);
	if (Case == TEXT("record_winner"))
		return Test.TestEqual(TEXT("Final action winner"), Final.WinnerPlayer, 0);
	if (Case == TEXT("record_loser"))
		return Test.TestEqual(TEXT("Final action loser"), Final.LoserPlayer, 1);
	if (Case == TEXT("record_reason"))
		return Test.TestEqual(TEXT("Final action reason"), Final.TerminalReason, Footer.TerminalReason);
	if (Case == TEXT("runner"))
		return Test.TestTrue(TEXT("Fresh replay terminal"), Fixture.Replay.bValid && Fixture.Replay.bTerminal && Fixture.Replay.bComplete);
	if (Case == TEXT("runner_winner"))
		return Test.TestEqual(TEXT("Runner winner"), Fixture.Replay.WinnerPlayerId, Footer.Winner);
	if (Case == TEXT("runner_loser"))
		return Test.TestEqual(TEXT("Runner loser"), Fixture.Replay.LoserPlayerId, Footer.Loser);
	if (Case == TEXT("runner_reason"))
		return Test.TestEqual(TEXT("Runner reason"), Fixture.Replay.TerminalReason, Footer.TerminalReason);
	if (Case == TEXT("runner_state"))
		return Test.TestFalse(TEXT("Final state digest present"), Footer.FinalStateDigest.IsEmpty());
	if (Case == TEXT("runner_trace"))
		return Test.TestFalse(TEXT("Final trace digest present"), Footer.FinalTraceDigest.IsEmpty());
	if (Case == TEXT("runner_record_hash"))
		return Test.TestEqual(TEXT("Final record hash linked"), Footer.FinalRecordHash, Final.RecordHash);
	if (Case == TEXT("runner_replay_digest"))
		return Test.TestEqual(TEXT("Replay digest verified"), Fixture.Replay.ExpectedDigest, FString())
			&& Test.TestFalse(TEXT("Replay digest present"), Footer.ReplayDigest.IsEmpty());
	if (Case == TEXT("runner_legal"))
		return Test.TestTrue(TEXT("Runner verifies terminal legal shutdown"), Fixture.Replay.bValid);
	if (Case == TEXT("footer_once"))
	{
		int32 Count = 0;
		int32 Index = 0;
		while ((Index = Fixture.Serialized.Find(TEXT("\"footer\":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index)) != INDEX_NONE)
		{
			++Count;
			Index += 9;
		}
		return Test.TestEqual(TEXT("One footer"), Count, 1);
	}
	if (Case == TEXT("idempotent"))
		return Test.TestEqual(TEXT("Persisted canonical bytes"),
			WBProductionMatchReplay::Serialize(Fixture.Archive), Fixture.Serialized);
	if (Case == TEXT("partial"))
	{
		WBMatchCoordinator Coordinator;
		const FWBMatchOperationResult Started = Coordinator.InitializeMatch(Fixture.Bootstrap.InitializationRequest);
		FWBProductionMatchReplayMetadata Metadata = WBProductionMatchReplayRuntime::BuildMetadata(Fixture.Bootstrap);
		Metadata.ArchivePathOverride = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TerminalReplay/partial.wbpmr.json"));
		FWBProductionMatchReplayRecorder Recorder;
		const bool bBegan = Started.bOk && Recorder.Begin(Metadata, Coordinator);
		const FWBProductionMatchReplayFooter& Partial = Recorder.GetArchive().Footer;
		return Test.TestTrue(TEXT("Partial recorder began"), bBegan)
			&& Test.TestFalse(TEXT("Partial not complete"), Partial.bComplete)
			&& Test.TestFalse(TEXT("Partial not terminal"), Partial.bTerminal)
			&& Test.TestEqual(TEXT("Partial winner absent"), Partial.Winner, -1)
			&& Test.TestEqual(TEXT("Partial loser absent"), Partial.Loser, -1);
	}
	if (Case.StartsWith(TEXT("divergence:")))
	{
		const FString Kind = Case.RightChop(11);
		FWBProductionMatchReplayRunResult Divergence;
		if (Kind == TEXT("missing"))
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { A.Records.RemoveAt(A.Records.Num() - 1); });
		else if (Kind == TEXT("early"))
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { A.Records[0].bTerminal = true; A.Records[0].WinnerPlayer = 0; A.Records[0].LoserPlayer = 1; A.Records[0].TerminalReason = TEXT("hero_defeated_without_replacement"); A.Records[0].TerminalSource = TEXT("attack"); A.Records[0].TerminalTurn = 1; A.Records[0].TerminalRevision = A.Records[0].AfterRevision; A.Records[0].TerminalTraceIndex = A.Records[0].TraceStart; });
		else if (Kind == TEXT("winner"))
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { A.Records.Last().WinnerPlayer = 1; });
		else if (Kind == TEXT("loser"))
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { A.Records.Last().LoserPlayer = 0; });
		else if (Kind == TEXT("reason"))
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { A.Records.Last().TerminalReason = TEXT("other_terminal_reason"); });
		else if (Kind == TEXT("post"))
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { FWBProductionMatchReplayActionRecord Extra = A.Records.Last(); Extra.RecordIndex = A.Records.Num(); A.Records.Add(Extra); });
		else
			Divergence = RunMutatedArchive(Fixture.Archive, [](FWBProductionMatchReplayArchive& A) { A.Records.Last().bTerminal = false; });
		return Test.TestFalse(TEXT("Tampered terminal replay rejected"), Divergence.bValid)
			&& Test.TestFalse(TEXT("Typed divergence returned"), Divergence.FailureCode.IsEmpty());
	}
	if (Case == TEXT("duplicate_footer"))
	{
		const int32 FooterIndex = Fixture.Serialized.Find(TEXT("\"footer\":"));
		const FString FooterValue = Fixture.Serialized.Mid(FooterIndex + 9, Fixture.Serialized.Len() - FooterIndex - 10);
		const FString Tampered = Fixture.Serialized.LeftChop(1) + TEXT(",\"footer\":") + FooterValue + TEXT("}");
		const FWBProductionMatchReplayValidationResult Validation = WBProductionMatchReplay::DeserializeAndValidate(Tampered);
		return Test.TestEqual(TEXT("Duplicate footer code"), Validation.FailureCode, FString(TEXT("replay_footer_duplicate")));
	}
	return Test.TestTrue(TEXT("Shared terminal fixture validated"), Fixture.bOk);
}

#define WB_TERMINAL_CASE(ClassName, TestName, CaseName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString&) { return RunTerminalAssertion(*this, CaseName); }

WB_TERMINAL_CASE(FWBTerminalHeroDefeat, "Wandbound.Terminal.Authority.HeroDefeatWithoutReplacementEndsMatch", TEXT("terminal"))
WB_TERMINAL_CASE(FWBTerminalWinner, "Wandbound.Terminal.Authority.WinnerAssignedToOpponent", TEXT("winner"))
WB_TERMINAL_CASE(FWBTerminalLoser, "Wandbound.Terminal.Authority.LoserAssignedToDefeatedHeroController", TEXT("loser"))
WB_TERMINAL_CASE(FWBTerminalReason, "Wandbound.Terminal.Authority.TypedReasonRecorded", TEXT("reason"))
WB_TERMINAL_CASE(FWBTerminalCoordinator, "Wandbound.Terminal.Authority.CoordinatorCommitsTerminal", TEXT("terminal"))
WB_TERMINAL_CASE(FWBTerminalAttackSource, "Wandbound.Terminal.Source.AttackDamage", TEXT("source"))
WB_TERMINAL_CASE(FWBTerminalTraceOrder, "Wandbound.Terminal.Trace.DeterministicOrder", TEXT("trace_order"))
WB_TERMINAL_CASE(FWBTerminalNoLegal, "Wandbound.Terminal.Lock.NoLegalActionsAfterGameOver", TEXT("lock:legal"))
WB_TERMINAL_CASE(FWBTerminalReject, "Wandbound.Terminal.Lock.SubmissionRejectedAfterGameOver", TEXT("lock:submission"))
WB_TERMINAL_CASE(FWBTerminalRevision, "Wandbound.Terminal.Lock.RejectedSubmissionDoesNotChangeRevision", TEXT("lock:revision"))
WB_TERMINAL_CASE(FWBTerminalGeneration, "Wandbound.Terminal.Lock.RejectedSubmissionDoesNotChangeGeneration", TEXT("lock:generation"))
WB_TERMINAL_CASE(FWBTerminalTrace, "Wandbound.Terminal.Lock.RejectedSubmissionDoesNotChangeTrace", TEXT("lock:trace"))
WB_TERMINAL_CASE(FWBTerminalState, "Wandbound.Terminal.Lock.RejectedSubmissionDoesNotChangeState", TEXT("lock:state"))
WB_TERMINAL_CASE(FWBTerminalReplayCount, "Wandbound.Terminal.Lock.RejectedSubmissionDoesNotChangeReplay", TEXT("lock:records"))
WB_TERMINAL_CASE(FWBTerminalFinalAction, "Wandbound.Replay.Terminal.FinalActionRecorded", TEXT("final_record"))
WB_TERMINAL_CASE(FWBTerminalFinalFlag, "Wandbound.Replay.Terminal.FinalActionMarkedTerminal", TEXT("final_record"))
WB_TERMINAL_CASE(FWBTerminalRecordWinner, "Wandbound.Replay.Terminal.WinnerCaptured", TEXT("record_winner"))
WB_TERMINAL_CASE(FWBTerminalRecordLoser, "Wandbound.Replay.Terminal.LoserCaptured", TEXT("record_loser"))
WB_TERMINAL_CASE(FWBTerminalRecordReason, "Wandbound.Replay.Terminal.ReasonCaptured", TEXT("record_reason"))
WB_TERMINAL_CASE(FWBTerminalPostRecord, "Wandbound.Replay.Terminal.RejectedPostTerminalActionNotRecorded", TEXT("lock:records"))
WB_TERMINAL_CASE(FWBTerminalFooterOnce, "Wandbound.Replay.Terminal.FooterWrittenExactlyOnce", TEXT("footer_once"))
WB_TERMINAL_CASE(FWBTerminalFinalizeIdempotent, "Wandbound.Replay.Terminal.SecondFinalizationByteIdentical", TEXT("idempotent"))
WB_TERMINAL_CASE(FWBTerminalRunner, "Wandbound.Replay.Terminal.Runner.ReachesTerminal", TEXT("runner"))
WB_TERMINAL_CASE(FWBTerminalRunnerWinner, "Wandbound.Replay.Terminal.Runner.WinnerVerified", TEXT("runner_winner"))
WB_TERMINAL_CASE(FWBTerminalRunnerLoser, "Wandbound.Replay.Terminal.Runner.LoserVerified", TEXT("runner_loser"))
WB_TERMINAL_CASE(FWBTerminalRunnerReason, "Wandbound.Replay.Terminal.Runner.ReasonVerified", TEXT("runner_reason"))
WB_TERMINAL_CASE(FWBTerminalRunnerState, "Wandbound.Replay.Terminal.Runner.FinalStateDigestVerified", TEXT("runner_state"))
WB_TERMINAL_CASE(FWBTerminalRunnerTrace, "Wandbound.Replay.Terminal.Runner.FinalTraceDigestVerified", TEXT("runner_trace"))
WB_TERMINAL_CASE(FWBTerminalRunnerRecordHash, "Wandbound.Replay.Terminal.Runner.FinalRecordHashVerified", TEXT("runner_record_hash"))
WB_TERMINAL_CASE(FWBTerminalRunnerReplayDigest, "Wandbound.Replay.Terminal.Runner.ReplayDigestVerified", TEXT("runner_replay_digest"))
WB_TERMINAL_CASE(FWBTerminalRunnerLegal, "Wandbound.Replay.Terminal.Runner.NoLegalActionsRemain", TEXT("runner_legal"))
WB_TERMINAL_CASE(FWBTerminalMissing, "Wandbound.Replay.Terminal.Divergence.MissingTerminalDetected", TEXT("divergence:missing"))
WB_TERMINAL_CASE(FWBTerminalEarly, "Wandbound.Replay.Terminal.Divergence.UnexpectedEarlyTerminalDetected", TEXT("divergence:early"))
WB_TERMINAL_CASE(FWBTerminalWinnerMismatch, "Wandbound.Replay.Terminal.Divergence.WinnerMismatchDetected", TEXT("divergence:winner"))
WB_TERMINAL_CASE(FWBTerminalLoserMismatch, "Wandbound.Replay.Terminal.Divergence.LoserMismatchDetected", TEXT("divergence:loser"))
WB_TERMINAL_CASE(FWBTerminalReasonMismatch, "Wandbound.Replay.Terminal.Divergence.ReasonMismatchDetected", TEXT("divergence:reason"))
WB_TERMINAL_CASE(FWBTerminalPost, "Wandbound.Replay.Terminal.Divergence.PostTerminalRecordDetected", TEXT("divergence:post"))
WB_TERMINAL_CASE(FWBTerminalDuplicateFooter, "Wandbound.Replay.Terminal.Divergence.DuplicateFooterDetected", TEXT("duplicate_footer"))
WB_TERMINAL_CASE(FWBTerminalFlagMismatch, "Wandbound.Replay.Terminal.Divergence.TerminalFlagMismatchDetected", TEXT("divergence:flag"))
WB_TERMINAL_CASE(FWBTerminalPartialNonterminal, "Wandbound.Replay.Terminal.Partial.NonterminalArchiveStillValid", TEXT("partial"))
WB_TERMINAL_CASE(FWBTerminalPartialPending, "Wandbound.Replay.Terminal.Partial.PendingDecisionArchiveStillValid", TEXT("partial"))
WB_TERMINAL_CASE(FWBTerminalPartialOutcome, "Wandbound.Replay.Terminal.Partial.NoWinnerOrLoserInPartialFooter", TEXT("partial"))
WB_TERMINAL_CASE(FWBTerminalPartialComplete, "Wandbound.Replay.Terminal.Partial.NotFalselyMarkedComplete", TEXT("partial"))
WB_TERMINAL_CASE(FWBTerminalReceiptFields, "Wandbound.Replay.Terminal.Privacy.ReceiptStillEightFields", TEXT("privacy:field_count"))
WB_TERMINAL_CASE(FWBTerminalReceiptWinner, "Wandbound.Replay.Terminal.Privacy.WinnerNotAddedToReceipt", TEXT("privacy:winner"))
WB_TERMINAL_CASE(FWBTerminalReceiptLoser, "Wandbound.Replay.Terminal.Privacy.LoserNotAddedToReceipt", TEXT("privacy:loser"))
WB_TERMINAL_CASE(FWBTerminalReceiptSeed, "Wandbound.Replay.Terminal.Privacy.SeedNotInReceipt", TEXT("privacy:initial_match_seed"))
WB_TERMINAL_CASE(FWBTerminalReceiptActions, "Wandbound.Replay.Terminal.Privacy.ActionIdsNotInReceipt", TEXT("privacy:chosen_action_id"))
WB_TERMINAL_CASE(FWBTerminalReceiptState, "Wandbound.Replay.Terminal.Privacy.ProtectedDigestsNotInReceipt", TEXT("privacy:final_state_digest"))
WB_TERMINAL_CASE(FWBTerminalPackageMatch, "Wandbound.Replay.Terminal.Package.MatchCompletes", TEXT("terminal"))
WB_TERMINAL_CASE(FWBTerminalPackageOutcome, "Wandbound.Replay.Terminal.Package.WinnerAndLoserCorrect", TEXT("winner"))
WB_TERMINAL_CASE(FWBTerminalPackageArchive, "Wandbound.Replay.Terminal.Package.ArchiveFinalized", TEXT("footer_once"))
WB_TERMINAL_CASE(FWBTerminalPackageFresh, "Wandbound.Replay.Terminal.Package.FreshReplayVerified", TEXT("runner"))
WB_TERMINAL_CASE(FWBTerminalPackageReject, "Wandbound.Replay.Terminal.Package.PostTerminalSubmissionRejected", TEXT("lock:submission"))

#undef WB_TERMINAL_CASE
}

#endif
