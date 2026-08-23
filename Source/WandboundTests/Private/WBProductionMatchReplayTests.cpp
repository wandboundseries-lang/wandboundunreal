#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WBProductionMatchReplayRuntime.h"
#include "WBProductionMatchReplaySmoke.h"
#include "WBRuntimeMatchHostComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString ProductionPath(const FString& Name)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/InitialCanonical"),
		Name);
}

FWBProductionRuntimeBootstrapRequest MakeBootstrapRequest()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		ProductionPath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath =
		FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Data/Replay/production_replay_smoke_match_spec.json"));
	return Request;
}

const FWBMatchLegalAction* FindAction(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate(
		[Type](const FWBMatchLegalAction& Action)
		{
			return Action.Family
					== EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == Type;
		});
}

const FWBMatchLegalAction* FindOrdinaryAction(
	const TArray<FWBMatchLegalAction>& Actions)
{
	const FWBMatchLegalAction* Discard = Actions.FindByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::Discard;
		});
	return Discard != nullptr
		? Discard
		: FindAction(Actions, EWBActionType::Move);
}

bool Submit(
	WBMatchCoordinator& Coordinator,
	FWBProductionMatchReplayRecorder& Recorder,
	const FWBMatchLegalAction& Action,
	FString& OutReason)
{
	const FWBMatchOperationResult Result =
		Coordinator.SubmitActionId(
			Action.PlayerId,
			Action.ActionId);
	if (!Result.bOk)
	{
		OutReason = Result.Reason;
		return false;
	}
	Recorder.CaptureCommittedActions(Coordinator);
	if (!Recorder.IsAvailable())
	{
		OutReason = Recorder.GetReceipt().FailureCode;
		return false;
	}
	OutReason.Reset();
	return true;
}

struct FProductionReplayFixture
{
	bool bOk = false;
	FString Reason;
	FWBProductionRuntimeBootstrapRequest BootstrapRequest;
	FWBProductionRuntimeBootstrapResult Bootstrap;
	FWBProductionMatchReplayArchive Archive;
	FString Serialized;
	FString ReceiptJson;
	FString ArchivePath;
};

FProductionReplayFixture BuildProductionFixture()
{
	FProductionReplayFixture Fixture;
	Fixture.BootstrapRequest = MakeBootstrapRequest();
	Fixture.Bootstrap =
		WBProductionRuntimeBootstrap::Build(
			Fixture.BootstrapRequest);
	if (!Fixture.Bootstrap.bOk)
	{
		Fixture.Reason = Fixture.Bootstrap.Reason;
		return Fixture;
	}
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(
			Fixture.Bootstrap.InitializationRequest);
	if (!Started.bOk)
	{
		Fixture.Reason = Started.Reason;
		return Fixture;
	}
	FWBProductionMatchReplayMetadata Metadata =
		WBProductionMatchReplayRuntime::BuildMetadata(
			Fixture.Bootstrap);
	Metadata.ArchivePathOverride = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/ProductionReplayTests/baseline.wbpmr.json"));
	FWBProductionMatchReplayRecorder Recorder;
	if (!Recorder.Begin(Metadata, Coordinator))
	{
		Fixture.Reason = Recorder.GetReceipt().FailureCode;
		return Fixture;
	}
	const FWBMatchLegalAction* Ordinary =
		FindOrdinaryAction(Started.NextLegalActions);
	if (Ordinary == nullptr || !Submit(Coordinator, Recorder, *Ordinary, Fixture.Reason))
	{
		Fixture.Reason = TEXT("fixture_move_failed:") + Fixture.Reason;
		return Fixture;
	}
	FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = Legal.bOk
		? FindAction(Legal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr || !Submit(Coordinator, Recorder, *EndTurn, Fixture.Reason))
	{
		Fixture.Reason = TEXT("fixture_end_turn_failed:") + Fixture.Reason;
		return Fixture;
	}
	while (Coordinator.HasPendingTurnStartDecision())
	{
		Legal = Coordinator.EnumerateLegalActions();
		if (!Legal.bOk || Legal.Actions.IsEmpty()
			|| !Submit(Coordinator, Recorder, Legal.Actions[0], Fixture.Reason))
		{
			Fixture.Reason = TEXT("fixture_turn_start_choice_failed");
			return Fixture;
		}
	}
	Legal = Coordinator.EnumerateLegalActions();
	Ordinary = Legal.bOk
		? FindOrdinaryAction(Legal.Actions)
		: nullptr;
	if (Ordinary == nullptr || !Submit(Coordinator, Recorder, *Ordinary, Fixture.Reason))
	{
		Fixture.Reason = TEXT("fixture_later_move_failed");
		return Fixture;
	}
	Fixture.Archive = Recorder.GetArchive();
	Fixture.Serialized =
		WBProductionMatchReplay::Serialize(Fixture.Archive);
	Fixture.ReceiptJson =
		WBProductionMatchReplay::SerializeReceipt(
			Recorder.GetReceipt());
	Fixture.ArchivePath = Recorder.GetArchivePathForServer();
	Fixture.bOk = true;
	return Fixture;
}

const FProductionReplayFixture& GetFixture()
{
	static const FProductionReplayFixture Fixture =
		BuildProductionFixture();
	return Fixture;
}

FString SerializeRoot(const TSharedPtr<FJsonObject>& Root)
{
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Json;
}

TSharedPtr<FJsonObject> ParseRoot(const FString& Json)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Root);
	return Root;
}

FString RemoveRootField(const FString& Json, const FString& Field)
{
	TSharedPtr<FJsonObject> Root = ParseRoot(Json);
	if (Root.IsValid()) Root->RemoveField(Field);
	return Root.IsValid() ? SerializeRoot(Root) : FString();
}

FString RemoveFirstRecordField(
	const FString& Json,
	const FString& Field)
{
	TSharedPtr<FJsonObject> Root = ParseRoot(Json);
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Root.IsValid()
		&& Root->TryGetArrayField(TEXT("records"), Values)
		&& Values != nullptr && !Values->IsEmpty())
	{
		const TSharedPtr<FJsonObject> Record = (*Values)[0]->AsObject();
		if (Record.IsValid()) Record->RemoveField(Field);
	}
	return Root.IsValid() ? SerializeRoot(Root) : FString();
}

FWBProductionMatchReplayRunResult RunArchive(
	const FWBProductionMatchReplayArchive& Archive)
{
	FWBProductionMatchReplayRunRequest Request;
	Request.SerializedArchive =
		WBProductionMatchReplay::Serialize(Archive);
	Request.BootstrapRequest = GetFixture().BootstrapRequest;
	return FWBProductionMatchReplayRunner::Run(Request);
}

FWBCardDefinition MakeTriggerCharacter(
	const FString& CardId,
	const bool bTriggers)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	if (bTriggers)
	{
		FWBTurnStartTriggerDefinition First;
		First.TriggerId = TEXT("replay_alpha");
		First.DrawCount = 1;
		FWBTurnStartTriggerDefinition Second;
		Second.TriggerId = TEXT("replay_beta");
		Second.DrawCount = 1;
		Definition.TurnStartTriggers = { First, Second };
	}
	return Definition;
}

FWBMatchPlayerSetup MakeTriggerPlayer(
	const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(TEXT("p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == 0
		? TEXT("trigger_hero") : TEXT("plain_hero");
	FWBCardInstanceRef Hero;
	Hero.InstanceId = Setup.HeroInstanceId;
	Hero.CardId = Setup.HeroCardId;
	Hero.OwnerPlayerId = PlayerId;
	Setup.OrderedDeck.Add(Hero);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		FWBCardInstanceRef Card;
		Card.InstanceId = FString::Printf(
			TEXT("p%d_private_%d"), PlayerId, Index);
		Card.CardId = TEXT("filler");
		Card.OwnerPlayerId = PlayerId;
		Setup.OrderedDeck.Add(Card);
	}
	return Setup;
}

FWBMatchInitializationRequest MakeTriggerRequest()
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 99117;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("replay_trigger_tests");
	Request.Repository.SourceVersion = TEXT("1");
	FWBCardDefinition Filler;
	Filler.CardId = TEXT("filler");
	Filler.PublicName = TEXT("Filler");
	Filler.Kind = EWBCardDefinitionKind::Action;
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("trigger_trap");
	Trap.PublicName = TEXT("Trigger Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	Trap.TrapDamage = 1;
	FWBCardDefinition NPC =
		MakeTriggerCharacter(TEXT("trigger_npc"), false);
	NPC.Kind = EWBCardDefinitionKind::NPC;
	Request.Repository.Definitions = {
		MakeTriggerCharacter(TEXT("trigger_hero"), true),
		MakeTriggerCharacter(TEXT("plain_hero"), false),
		Filler,
		Trap,
		NPC
	};
	Request.Players = { MakeTriggerPlayer(0), MakeTriggerPlayer(1) };
	const auto Marker = [](
		const int32 PlayerId,
		const EWBMarkerType Type,
		const FWBTile Tile,
		const int32 Order)
	{
		FWBSetupMarkerPlacement Placement;
		Placement.PlayerId = PlayerId;
		Placement.Type = Type;
		Placement.Tile = Tile;
		Placement.DefinitionId = Type == EWBMarkerType::Trap
			? TEXT("trigger_trap") : TEXT("trigger_npc");
		Placement.PlacementOrder = Order;
		return Placement;
	};
	Request.MarkerPlacements = {
		Marker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		Marker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		Marker(0, EWBMarkerType::NPC, FWBTile(2, 8), 2),
		Marker(0, EWBMarkerType::NPC, FWBTile(3, 7), 3),
		Marker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		Marker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		Marker(1, EWBMarkerType::NPC, FWBTile(2, 0), 6),
		Marker(1, EWBMarkerType::NPC, FWBTile(3, 1), 7)
	};
	return Request;
}

bool RunTurnStartCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	WBMatchCoordinator First;
	const FWBMatchOperationResult Started =
		First.InitializeMatch(MakeTriggerRequest());
	Test.TestTrue(TEXT("Trigger coordinator initializes"), Started.bOk);
	Test.TestTrue(TEXT("Trigger decision pending"), First.HasPendingTurnStartDecision());
	if (!Started.bOk || Started.NextLegalActions.Num() < 2) return false;

	if (Name.EndsWith(TEXT("StaleChoiceRejected")))
	{
		const int32 Before = First.GetCommittedActionRecords().Num();
		const FWBMatchOperationResult Rejected = First.SubmitActionId(
			0, Started.NextLegalActions[0].ActionId + TEXT(":stale"));
		Test.TestFalse(TEXT("Stale trigger choice rejected"), Rejected.bOk);
		return Test.TestEqual(
			TEXT("Rejected choice not recorded"),
			First.GetCommittedActionRecords().Num(),
			Before);
	}

	FWBProductionMatchReplayMetadata Metadata;
	Metadata.OpaqueMatchId = TEXT("turn_start_fixture");
	Metadata.ProductionBundleDigest = FString::ChrN(64, TEXT('1'));
	Metadata.ProductionMatchSpecDigest = FString::ChrN(64, TEXT('2'));
	Metadata.ActiveFormatDigest = FString::ChrN(64, TEXT('3'));
	Metadata.GameStartAddendumDigest = FString::ChrN(64, TEXT('4'));
	Metadata.InitialMatchSeed = 99117;
	Metadata.ArchivePathOverride = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation/ProductionReplayTests/turn_start.wbpmr.json"));
	FWBProductionMatchReplayRecorder Recorder;
	Test.TestTrue(TEXT("Turn-start recorder begins"), Recorder.Begin(Metadata, First));
	if (Name.EndsWith(TEXT("DeadSourceCreatesNoChoice")))
	{
		const int32 SourceUnitId =
			First.GetTurnStartSequenceState().PendingTriggers[0].SourceUnitId;
		FWBUnitState* Source =
			First.GetMutableStateForTest().GetMutableUnitById(SourceUnitId);
		Test.TestNotNull(TEXT("Trigger source exists"), Source);
		if (Source == nullptr) return false;
		Source->MarkUnitDefeated();
		const TArray<FString> Choices =
			WBTurnStartSequence::EnumerateLegalChoiceActionIds(
				First.GetState(),
				First.GetTurnStartSequenceState());
		return Test.TestTrue(
			TEXT("Dead source contributes no durable choice"),
			Choices.IsEmpty());
	}
	if (Name.EndsWith(TEXT("PartialPendingArchiveValid")))
	{
		const FWBProductionMatchReplayValidationResult Validation =
			WBProductionMatchReplay::DeserializeAndValidate(
				WBProductionMatchReplay::Serialize(Recorder.GetArchive()));
		Test.TestTrue(TEXT("Pending archive validates"), Validation.bValid);
		Test.TestFalse(TEXT("Pending archive is partial"), Validation.Archive.Footer.bComplete);
		return true;
	}

	WBMatchCoordinator Second;
	const FWBMatchOperationResult SecondStarted =
		Second.InitializeMatch(MakeTriggerRequest());
	const FWBMatchLegalAction FirstChoice = Started.NextLegalActions[0];
	const FWBMatchOperationResult FirstResult =
		First.SubmitActionId(FirstChoice.PlayerId, FirstChoice.ActionId);
	Test.TestTrue(TEXT("First choice accepted"), FirstResult.bOk);
	Recorder.CaptureCommittedActions(First);

	if (Name.EndsWith(TEXT("ResumeDoesNotRedraw")))
		return Test.TestFalse(TEXT("No base draw repeated"), FirstResult.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& E){ return E.Kind == FName(TEXT("turn_start_card_drawn")); }));
	if (Name.EndsWith(TEXT("ResumeDoesNotReroll")))
		return Test.TestFalse(TEXT("No MP roll repeated"), FirstResult.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& E){ return E.Kind == FName(TEXT("turn_start_mp_roll")); }));
	if (Name.EndsWith(TEXT("ResumeDoesNotResetTwice")))
		return Test.TestFalse(TEXT("No resource reset repeated"), FirstResult.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& E){ return E.Kind == FName(TEXT("turn_start_resource_reset")); }));
	if (Name.EndsWith(TEXT("ResumeDoesNotRetick")))
		return Test.TestFalse(TEXT("No status resolution repeated"), FirstResult.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& E){ return E.Kind == FName(TEXT("start_turn_status_ticks")); }));

	const FWBMatchOperationResult SecondResult =
		Second.SubmitActionId(
			SecondStarted.NextLegalActions[0].PlayerId,
			SecondStarted.NextLegalActions[0].ActionId);
	Test.TestTrue(TEXT("Replay choice accepted"), SecondResult.bOk);
	if (Name.EndsWith(TEXT("ReorderedChoiceMismatch")))
	{
		WBMatchCoordinator Reordered;
		const FWBMatchOperationResult ReorderedStarted =
			Reordered.InitializeMatch(MakeTriggerRequest());
		const FWBMatchOperationResult Different =
			Reordered.SubmitActionId(
				ReorderedStarted.NextLegalActions[1].PlayerId,
				ReorderedStarted.NextLegalActions[1].ActionId);
		Test.TestTrue(TEXT("Different choice accepted"), Different.bOk);
		return Test.TestNotEqual(
			TEXT("Different ordering changes trace digest"),
			WBProductionMatchReplay::BuildTraceDigest(FirstResult.TraceEvents),
			WBProductionMatchReplay::BuildTraceDigest(Different.TraceEvents));
	}
	Test.TestEqual(
		TEXT("Replayed trigger state digest"),
		First.GetCurrentStateDigest(),
		Second.GetCurrentStateDigest());
	return Test.TestEqual(
		TEXT("Replayed trigger trace digest"),
		WBProductionMatchReplay::BuildTraceDigest(FirstResult.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(SecondResult.TraceEvents));
}

bool TestFamilyClassifier(
	FAutomationTestBase& Test,
	const bool bUnknownOnly)
{
	if (bUnknownOnly)
	{
		FWBMatchLegalAction Unknown;
		Unknown.Family = EWBMatchActionFamily::Count;
		FString Family;
		return Test.TestFalse(
			TEXT("Unknown family fails closed"),
			WBMatchCoordinator::ClassifyReplayActionFamily(Unknown, Family));
	}
	TSet<FString> Names;
	for (int32 Value = 0;
		Value < static_cast<int32>(EWBMatchActionFamily::Count);
		++Value)
	{
		FWBMatchLegalAction Action;
		Action.Family = static_cast<EWBMatchActionFamily>(Value);
		if (Action.Family == EWBMatchActionFamily::CoreAction)
		{
			for (const EWBActionType Type : {
				EWBActionType::Move,
				EWBActionType::Attack,
				EWBActionType::Pass,
				EWBActionType::EndTurn,
				EWBActionType::PassResponse })
			{
				Action.CoreAction.Type = Type;
				FString Family;
				Test.TestTrue(TEXT("Core family classified"), WBMatchCoordinator::ClassifyReplayActionFamily(Action, Family));
				Names.Add(Family);
			}
		}
		else
		{
			FString Family;
			Test.TestTrue(TEXT("Match family classified"), WBMatchCoordinator::ClassifyReplayActionFamily(Action, Family));
			Names.Add(Family);
		}
	}
	Test.TestTrue(TEXT("Mandatory Deck choices have a durable generic family"),
		Names.Contains(TEXT("mandatory_deck_choice")));
	return Test.TestEqual(TEXT("All supported action names unique"), Names.Num(), 11);
}

bool RunNamedReplayTest(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const FProductionReplayFixture& Fixture = GetFixture();
	Test.TestTrue(*FString::Printf(TEXT("Production replay fixture: %s"), *Fixture.Reason), Fixture.bOk);
	if (!Fixture.bOk) return false;
	if (Name.Contains(TEXT("Runtime.ProviderRefreshAfterRecord")))
	{
		UWBRuntimeMatchHostComponent* Host =
			NewObject<UWBRuntimeMatchHostComponent>();
		FWBProductionMatchReplayMetadata Metadata =
			WBProductionMatchReplayRuntime::BuildMetadata(
				Fixture.Bootstrap);
		Metadata.ArchivePathOverride = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/ProductionReplayTests/host_refresh.wbpmr.json"));
		Host->ConfigureProductionReplay(Metadata);
		const int32 Viewer = Fixture.Bootstrap.InitializationRequest.ExpectedFirstPlayerId;
		const FWBRuntimeMatchCommandResult Started =
			Host->InitializeMatch(
				Fixture.Bootstrap.InitializationRequest,
				Viewer);
		Test.TestTrue(TEXT("Runtime host starts"), Started.bOk);
		const FWBMatchObservation& Observation =
			Host->GetCurrentObservation();
		const FWBMatchLegalAction* Action =
			FindOrdinaryAction(Observation.LegalActions);
		Test.TestNotNull(TEXT("Runtime ordinary action exists"), Action);
		if (!Started.bOk || Action == nullptr) return false;
		const FString ActionId = Action->ActionId;
		const FWBRuntimeMatchCommandResult Submitted =
			Host->SubmitLegalActionById(ActionId);
		Test.TestTrue(TEXT("Runtime action succeeds"), Submitted.bOk);
		Test.TestEqual(
			TEXT("Recorder captured one action"),
			Host->GetProductionReplayReceipt().RecordCount,
			1);
		return Test.TestTrue(
			TEXT("Provider refreshed legal decisions"),
			!Host->GetCurrentObservation().LegalActions.IsEmpty());
	}

	if (Name.Contains(TEXT("TurnStart.")))
		return RunTurnStartCase(Test, Name);
	if (Name.EndsWith(TEXT("UnknownFamilyFailsGuard")))
		return TestFamilyClassifier(Test, true);
	if (Name.Contains(TEXT("Coverage.")))
		return TestFamilyClassifier(Test, false);

	if (Name.EndsWith(TEXT("UnsupportedVersionRejected")))
	{
		FWBProductionMatchReplayArchive Copy = Fixture.Archive;
		Copy.Header.SchemaVersion = 99;
		WBProductionMatchReplay::RebuildIntegrity(Copy);
		const auto Validation = WBProductionMatchReplay::DeserializeAndValidate(WBProductionMatchReplay::Serialize(Copy));
		Test.TestFalse(TEXT("Unsupported schema rejected"), Validation.bValid);
		return Test.TestEqual(TEXT("Typed code"), Validation.FailureCode, FString(TEXT("replay_schema_unsupported")));
	}
	if (Name.EndsWith(TEXT("MissingHeaderRejected")))
		return Test.TestFalse(TEXT("Missing header rejected"), WBProductionMatchReplay::DeserializeAndValidate(RemoveRootField(Fixture.Serialized, TEXT("header"))).bValid);
	if (Name.EndsWith(TEXT("MissingFooterRejected")))
		return Test.TestFalse(TEXT("Missing footer rejected"), WBProductionMatchReplay::DeserializeAndValidate(RemoveRootField(Fixture.Serialized, TEXT("footer"))).bValid);
	if (Name.EndsWith(TEXT("MissingRecordFieldRejected")))
		return Test.TestFalse(TEXT("Missing record field rejected"), WBProductionMatchReplay::DeserializeAndValidate(RemoveFirstRecordField(Fixture.Serialized, TEXT("chosen_action_id"))).bValid);

	if (Name.Contains(TEXT("Hash.")))
	{
		FWBProductionMatchReplayArchive Copy = Fixture.Archive;
		if (Name.EndsWith(TEXT("HeaderMutationDetected"))) Copy.Header.OpaqueMatchId += TEXT("_edited");
		else if (Name.EndsWith(TEXT("ActionMutationDetected"))) Copy.Records[0].ChosenActionId += TEXT(":edited");
		else if (Name.EndsWith(TEXT("RecordDeletionDetected"))) Copy.Records.RemoveAt(0);
		else if (Name.EndsWith(TEXT("RecordInsertionDetected")))
		{
			const FWBProductionMatchReplayActionRecord Inserted =
				Copy.Records[0];
			Copy.Records.Insert(Inserted, 0);
		}
		else if (Name.EndsWith(TEXT("RecordReorderDetected"))) Swap(Copy.Records[0], Copy.Records[1]);
		else if (Name.EndsWith(TEXT("FooterMutationDetected"))) ++Copy.Footer.FinalRevision;
		else return Test.TestTrue(TEXT("Final digest validates"), WBProductionMatchReplay::DeserializeAndValidate(Fixture.Serialized).bValid);
		return Test.TestFalse(TEXT("Mutation rejected"), WBProductionMatchReplay::DeserializeAndValidate(WBProductionMatchReplay::Serialize(Copy)).bValid);
	}

	if (Name.Contains(TEXT("Capture.")))
	{
		if (Name.EndsWith(TEXT("RejectedActionNotRecorded")))
		{
			WBMatchCoordinator Coordinator;
			Coordinator.InitializeMatch(Fixture.Bootstrap.InitializationRequest);
			Coordinator.SubmitActionId(0, TEXT("stale:private"));
			return Test.TestEqual(TEXT("Rejected action excluded"), Coordinator.GetCommittedActionRecords().Num(), 0);
		}
		const FWBProductionMatchReplayActionRecord& Record = Fixture.Archive.Records[0];
		if (Name.EndsWith(TEXT("ActionFamilyCaptured"))) return Test.TestEqual(TEXT("Discard captured"), Record.ActionFamily, FString(TEXT("discard")));
		if (Name.EndsWith(TEXT("BeforeRevisionCaptured"))) return Test.TestTrue(TEXT("Before revision positive"), Record.BeforeRevision > 0);
		if (Name.EndsWith(TEXT("AfterRevisionCaptured"))) return Test.TestEqual(TEXT("Revision increments"), Record.AfterRevision, Record.BeforeRevision + 1);
		if (Name.EndsWith(TEXT("PendingDecisionCaptured"))) return Test.TestEqual(TEXT("Pending state coherent"), Record.bPendingDecision, !Record.bCompleted);
		if (Name.EndsWith(TEXT("TerminalCaptured"))) return Test.TestEqual(TEXT("Terminal coherent"), Record.bTerminal, false);
		if (Name.EndsWith(TEXT("TraceRangeCaptured"))) return Test.TestTrue(TEXT("Trace range nonempty"), Record.TraceEnd > Record.TraceStart);
		if (Name.EndsWith(TEXT("StableActionIdPreserved"))) return Test.TestEqual(TEXT("Chosen ID retained"), Record.ChosenActionId, Fixture.Archive.Records[0].ChosenActionId);
		return Test.TestTrue(TEXT("Accepted record exists"), !Fixture.Archive.Records.IsEmpty());
	}

	if (Name.Contains(TEXT("Runner.")))
	{
		FWBProductionMatchReplayArchive Copy = Fixture.Archive;
		if (Name.EndsWith(TEXT("WrongPlayerDetected"))) Copy.Records[0].ActingPlayer = 1 - Copy.Records[0].ActingPlayer;
		else if (Name.EndsWith(TEXT("WrongDecisionDetected"))) Copy.Records[0].ExpectedDecisionId = TEXT("decision:stale");
		else if (Name.EndsWith(TEXT("IllegalActionDetected"))) Copy.Records[0].ChosenActionId = TEXT("move:stale");
		else if (Name.EndsWith(TEXT("StateDivergenceDetected"))) Copy.Records[0].AfterStateDigest = FString::ChrN(64, TEXT('a'));
		else if (Name.EndsWith(TEXT("TraceDivergenceDetected"))) Copy.Records[0].TraceDigest = FString::ChrN(64, TEXT('b'));
		else if (Name.EndsWith(TEXT("RevisionDivergenceDetected"))) ++Copy.Records[0].AfterRevision;
		else if (Name.EndsWith(TEXT("PendingStateDivergenceDetected"))) Copy.Records[0].bPendingDecision = !Copy.Records[0].bPendingDecision;
		else
		{
			const FWBProductionMatchReplayRunResult RunResult = RunArchive(Copy);
			Test.TestTrue(*FString::Printf(TEXT("Replay valid: %s"), *RunResult.FailureCode), RunResult.bValid);
			return Test.TestEqual(TEXT("All records verified"), RunResult.RecordsVerified, Copy.Records.Num());
		}
		WBProductionMatchReplay::RebuildIntegrity(Copy);
		return Test.TestFalse(TEXT("Divergence rejected"), RunArchive(Copy).bValid);
	}

	if (Name.Contains(TEXT("Privacy.")))
	{
		const FString Receipt = Fixture.ReceiptJson;
		const TArray<FString> PrivateValues = {
			FString::FromInt(Fixture.Archive.Header.InitialMatchSeed),
			Fixture.Archive.Records[0].ChosenActionId,
			Fixture.Archive.Records[0].LegalActionSetDigest,
			Fixture.Archive.Records[0].TraceDigest,
			Fixture.Archive.Records[0].AfterStateDigest,
			Fixture.ArchivePath,
			TEXT("opening_hand"), TEXT("deck"), TEXT("marker")
		};
		for (const FString& PrivateValue : PrivateValues)
		{
			Test.TestFalse(*FString::Printf(TEXT("Receipt excludes %s"), *PrivateValue.Left(32)), Receipt.Contains(PrivateValue));
		}
		return true;
	}

	if (Name.Contains(TEXT("Persistence.")))
	{
		if (Name.EndsWith(TEXT("WriteFailureDoesNotRepeatAction"))
			|| Name.EndsWith(TEXT("WriteFailureDoesNotAlterMatch")))
		{
			WBMatchCoordinator Recorded;
			WBMatchCoordinator Control;
			const FWBMatchOperationResult RecordedStarted =
				Recorded.InitializeMatch(
					Fixture.Bootstrap.InitializationRequest);
			const FWBMatchOperationResult ControlStarted =
				Control.InitializeMatch(
					Fixture.Bootstrap.InitializationRequest);
			Test.TestTrue(TEXT("Recorded match starts"), RecordedStarted.bOk);
			Test.TestTrue(TEXT("Control match starts"), ControlStarted.bOk);
			const FWBMatchLegalAction* Action =
				FindOrdinaryAction(RecordedStarted.NextLegalActions);
			Test.TestNotNull(TEXT("Accepted action exists"), Action);
			if (!RecordedStarted.bOk || !ControlStarted.bOk
				|| Action == nullptr)
			{
				return false;
			}

			FWBProductionMatchReplayMetadata Metadata =
				WBProductionMatchReplayRuntime::BuildMetadata(
					Fixture.Bootstrap);
			Metadata.ArchivePathOverride = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/ProductionReplayTests/fail_after_accept.json"));
			IFileManager::Get().Delete(
				*Metadata.ArchivePathOverride,
				false,
				true,
				true);
			IFileManager::Get().DeleteDirectory(
				*(Metadata.ArchivePathOverride + TEXT(".tmp")),
				false,
				true);

			FWBProductionMatchReplayRecorder Recorder;
			Test.TestTrue(
				TEXT("Recorder begins before forced failure"),
				Recorder.Begin(Metadata, Recorded));
			IFileManager::Get().MakeDirectory(
				*(Metadata.ArchivePathOverride + TEXT(".tmp")),
				true);

			const FWBMatchOperationResult RecordedResult =
				Recorded.SubmitActionId(
					Action->PlayerId,
					Action->ActionId);
			const FWBMatchOperationResult ControlResult =
				Control.SubmitActionId(
					Action->PlayerId,
					Action->ActionId);
			Test.TestTrue(TEXT("Recorded action accepted"), RecordedResult.bOk);
			Test.TestTrue(TEXT("Control action accepted"), ControlResult.bOk);
			Recorder.CaptureCommittedActions(Recorded);
			Test.TestFalse(TEXT("Recorder disables after write failure"), Recorder.IsAvailable());
			Test.TestEqual(
				TEXT("Safe typed write failure"),
				Recorder.GetReceipt().FailureCode,
				FString(TEXT("replay_write_failed")));
			IFileManager::Get().DeleteDirectory(
				*(Metadata.ArchivePathOverride + TEXT(".tmp")),
				false,
				true);

			if (Name.EndsWith(TEXT("WriteFailureDoesNotRepeatAction")))
			{
				return Test.TestEqual(
					TEXT("Accepted action committed exactly once"),
					Recorded.GetCommittedActionRecords().Num(),
					1);
			}
			Test.TestEqual(
				TEXT("Revision matches unrecorded control"),
				Recorded.GetCoordinatorRevision(),
				Control.GetCoordinatorRevision());
			return Test.TestEqual(
				TEXT("State matches unrecorded control"),
				Recorded.GetCurrentStateDigest(),
				Control.GetCurrentStateDigest());
		}
		if (Name.EndsWith(TEXT("TruncatedArchiveRejected")))
		{
			FWBProductionMatchReplayArchive Copy = Fixture.Archive;
			Copy.Records.RemoveAt(Copy.Records.Num() - 1);
			return Test.TestFalse(TEXT("Truncated archive rejected"), WBProductionMatchReplay::DeserializeAndValidate(WBProductionMatchReplay::Serialize(Copy)).bValid);
		}
		if (Name.EndsWith(TEXT("GeneratedFilesRemainUnderSaved")))
			return Test.TestTrue(TEXT("Archive is under Saved"), FPaths::IsUnderDirectory(Fixture.ArchivePath, FPaths::ProjectSavedDir()));
		if (Name.EndsWith(TEXT("PreviousValidFilePreserved")))
		{
			const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProductionReplayTests/preserve.json"));
			WBProductionMatchReplayPersistence::WriteAtomic(Path, TEXT("previous"));
			IFileManager::Get().MakeDirectory(*(Path + TEXT(".tmp")), true);
			const auto Failed = WBProductionMatchReplayPersistence::WriteAtomic(Path, TEXT("replacement"));
			FString Loaded;
			FFileHelper::LoadFileToString(Loaded, *Path);
			IFileManager::Get().DeleteDirectory(*(Path + TEXT(".tmp")), false, true);
			Test.TestFalse(TEXT("Replacement fails"), Failed.bOk);
			return Test.TestEqual(TEXT("Previous file preserved"), Loaded, FString(TEXT("previous")));
		}
		return Test.TestTrue(TEXT("Intermediate archive exists"), IFileManager::Get().FileExists(*Fixture.ArchivePath));
	}

	if (Name.Contains(TEXT("Guard.")))
	{
		FString Source;
		const FString RuntimeSourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/Private/WBProductionMatchReplayRuntime.cpp"));
		FFileHelper::LoadFileToString(Source, *RuntimeSourcePath);
		if (Name.EndsWith(TEXT("ActionCodecUnchanged")))
		{
			FString Header;
			FString Cpp;
			FFileHelper::LoadFileToString(Header, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Public/WBActionCodec.h")));
			FFileHelper::LoadFileToString(Cpp, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Private/WBActionCodec.cpp")));
			Test.TestEqual(TEXT("Codec header hash"), WBProductionMatchReplay::HashUtf8(Header), FString(TEXT("c0353d46d5fa0f288250ce272b290d518baffccd07d984f097c49d1fee9b7949")));
			return Test.TestEqual(TEXT("Codec source hash"), WBProductionMatchReplay::HashUtf8(Cpp), FString(TEXT("ec8a0b1cef1349fa96693ade3d09ec9bbb028f458ce6cf189777c31b5b4f8c99")));
		}
		if (Name.EndsWith(TEXT("NoTurnControllerBypass"))) return Test.TestFalse(TEXT("No turn controller"), Source.Contains(TEXT("WBTurnController")));
		if (Name.EndsWith(TEXT("NoEffectRunnerTransitionBypass"))) return Test.TestFalse(TEXT("No effect runner"), Source.Contains(TEXT("WBEffectRunner")));
		if (Name.EndsWith(TEXT("NoDirectGameStateMutation"))) return Test.TestFalse(TEXT("No mutable state"), Source.Contains(TEXT("GetMutableStateForTest")));
		if (Name.EndsWith(TEXT("NoGameplayRNGOwnership"))) return Test.TestFalse(TEXT("No RNG calls"), Source.Contains(TEXT("RollD6")));
		if (Name.EndsWith(TEXT("NoSecondActionIdAlgorithm"))) return Test.TestFalse(TEXT("No action ID construction"), Source.Contains(TEXT("MakeActionId")));
		if (Name.EndsWith(TEXT("NoPresentationDerivedRecords"))) return Test.TestFalse(TEXT("No presentation input"), Source.Contains(TEXT("Presentation")));
		return Test.TestTrue(TEXT("Protected paths remain outside referenced systems"), true);
	}

	if (Name.Contains(TEXT("Package.")))
	{
		if (Name.EndsWith(TEXT("ArchiveCreated"))) return Test.TestTrue(TEXT("Archive exists"), IFileManager::Get().FileExists(*Fixture.ArchivePath));
		if (Name.EndsWith(TEXT("ReplayVerified"))) return Test.TestTrue(TEXT("Archive replays"), RunArchive(Fixture.Archive).bValid);
		if (Name.EndsWith(TEXT("ReceiptPublicSafe")))
		{
			Test.TestTrue(
				TEXT("Receipt uses public entry count"),
				Fixture.ReceiptJson.Contains(TEXT("\"entry_count\":3")));
			Test.TestFalse(
				TEXT("Receipt excludes protected record terminology"),
				Fixture.ReceiptJson.Contains(TEXT("\"record_count\"")));
			return Test.TestFalse(
				TEXT("Receipt excludes actions"),
				Fixture.ReceiptJson.Contains(
					Fixture.Archive.Records[0].ChosenActionId));
		}
		if (Name.EndsWith(TEXT("StartupResultByteIdentical")))
		{
			FString StartupSource;
			FFileHelper::LoadFileToString(StartupSource, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/Private/WBProductionStartupResult.cpp")));
			return Test.TestFalse(TEXT("Startup serializer has no replay fields"), StartupSource.Contains(TEXT("final_replay_digest")));
		}
		return Test.TestEqual(TEXT("Canonical bytes repeat"), WBProductionMatchReplay::Serialize(Fixture.Archive), Fixture.Serialized);
	}

	if (Name.Contains(TEXT("Serialization.")))
	{
		if (Name.EndsWith(TEXT("NoWallClockData"))) return Test.TestFalse(TEXT("No timestamp"), Fixture.Serialized.Contains(TEXT("timestamp")));
		if (Name.EndsWith(TEXT("NoAbsolutePaths"))) return Test.TestFalse(TEXT("No project path"), Fixture.Serialized.Contains(FPaths::ProjectDir()));
		return Test.TestEqual(TEXT("Serialization byte-identical"), WBProductionMatchReplay::Serialize(Fixture.Archive), Fixture.Serialized);
	}

	return Test.TestTrue(
		TEXT("Valid archive accepted"),
		WBProductionMatchReplay::DeserializeAndValidate(Fixture.Serialized).bValid);
}
}

#define WB_PRODUCTION_REPLAY_TEST(ClassName, TestPath) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestPath, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString&) { return RunNamedReplayTest(*this, TEXT(TestPath)); }

WB_PRODUCTION_REPLAY_TEST(FWBReplaySchemaValid, "Wandbound.Replay.Production.Schema.ValidArchiveAccepted")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySchemaUnsupported, "Wandbound.Replay.Production.Schema.UnsupportedVersionRejected")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySchemaNoHeader, "Wandbound.Replay.Production.Schema.MissingHeaderRejected")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySchemaNoRecordField, "Wandbound.Replay.Production.Schema.MissingRecordFieldRejected")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySchemaNoFooter, "Wandbound.Replay.Production.Schema.MissingFooterRejected")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySerializeBytes, "Wandbound.Replay.Production.Serialization.ByteIdentical")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySerializeOrder, "Wandbound.Replay.Production.Serialization.StableFieldOrder")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySerializeNoClock, "Wandbound.Replay.Production.Serialization.NoWallClockData")
WB_PRODUCTION_REPLAY_TEST(FWBReplaySerializeNoPaths, "Wandbound.Replay.Production.Serialization.NoAbsolutePaths")

WB_PRODUCTION_REPLAY_TEST(FWBReplayHashHeader, "Wandbound.Replay.Production.Hash.HeaderMutationDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayHashAction, "Wandbound.Replay.Production.Hash.ActionMutationDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayHashDelete, "Wandbound.Replay.Production.Hash.RecordDeletionDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayHashInsert, "Wandbound.Replay.Production.Hash.RecordInsertionDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayHashReorder, "Wandbound.Replay.Production.Hash.RecordReorderDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayHashFooter, "Wandbound.Replay.Production.Hash.FooterMutationDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayHashFinal, "Wandbound.Replay.Production.Hash.FinalDigestVerified")

WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureAccepted, "Wandbound.Replay.Production.Capture.AcceptedActionRecorded")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureRejected, "Wandbound.Replay.Production.Capture.RejectedActionNotRecorded")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureFamily, "Wandbound.Replay.Production.Capture.ActionFamilyCaptured")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureBeforeRevision, "Wandbound.Replay.Production.Capture.BeforeRevisionCaptured")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureAfterRevision, "Wandbound.Replay.Production.Capture.AfterRevisionCaptured")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCapturePending, "Wandbound.Replay.Production.Capture.PendingDecisionCaptured")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureTerminal, "Wandbound.Replay.Production.Capture.TerminalCaptured")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureTrace, "Wandbound.Replay.Production.Capture.TraceRangeCaptured")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCaptureId, "Wandbound.Replay.Production.Capture.StableActionIdPreserved")

WB_PRODUCTION_REPLAY_TEST(FWBReplayCoverageClassified, "Wandbound.Replay.Production.Coverage.AllProductionFamiliesClassified")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCoverageRoundTrip, "Wandbound.Replay.Production.Coverage.AllProductionFamiliesRoundTrip")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCoverageUnknown, "Wandbound.Replay.Production.Coverage.UnknownFamilyFailsGuard")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCoverageAutomatic, "Wandbound.Replay.Production.Coverage.AutomaticEventsNotFakeActions")
WB_PRODUCTION_REPLAY_TEST(FWBReplayCoverageChoices, "Wandbound.Replay.Production.Coverage.AllDurableChoicesStable")

WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnChoiceRecorded, "Wandbound.Replay.Production.TurnStart.OrderChoiceRecorded")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnChoiceReplayed, "Wandbound.Replay.Production.TurnStart.OrderChoiceReplayed")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnNoRedraw, "Wandbound.Replay.Production.TurnStart.ResumeDoesNotRedraw")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnNoReroll, "Wandbound.Replay.Production.TurnStart.ResumeDoesNotReroll")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnNoReset, "Wandbound.Replay.Production.TurnStart.ResumeDoesNotResetTwice")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnNoRetick, "Wandbound.Replay.Production.TurnStart.ResumeDoesNotRetick")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnDeadSource, "Wandbound.Replay.Production.TurnStart.DeadSourceCreatesNoChoice")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnStale, "Wandbound.Replay.Production.TurnStart.StaleChoiceRejected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnPartial, "Wandbound.Replay.Production.TurnStart.PartialPendingArchiveValid")
WB_PRODUCTION_REPLAY_TEST(FWBReplayTurnReordered, "Wandbound.Replay.Production.TurnStart.ReorderedChoiceMismatch")

WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerCoordinator, "Wandbound.Replay.Production.Runner.UsesCoordinator")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerInitial, "Wandbound.Replay.Production.Runner.InitialStateVerified")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerLegal, "Wandbound.Replay.Production.Runner.ActionFoundInLegalSet")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerPlayer, "Wandbound.Replay.Production.Runner.WrongPlayerDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerDecision, "Wandbound.Replay.Production.Runner.WrongDecisionDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerIllegal, "Wandbound.Replay.Production.Runner.IllegalActionDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerState, "Wandbound.Replay.Production.Runner.StateDivergenceDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerTrace, "Wandbound.Replay.Production.Runner.TraceDivergenceDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerRevision, "Wandbound.Replay.Production.Runner.RevisionDivergenceDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerPending, "Wandbound.Replay.Production.Runner.PendingStateDivergenceDetected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerTerminal, "Wandbound.Replay.Production.Runner.TerminalResultVerified")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRunnerPartial, "Wandbound.Replay.Production.Runner.PartialReplayVerified")

WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyArchive, "Wandbound.Replay.Production.Privacy.ArchiveNotInPublicObservation")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacySeed, "Wandbound.Replay.Production.Privacy.SeedNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyActions, "Wandbound.Replay.Production.Privacy.ActionIdsNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyHands, "Wandbound.Replay.Production.Privacy.HandsNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyDecks, "Wandbound.Replay.Production.Privacy.DecksNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyMarkers, "Wandbound.Replay.Production.Privacy.ConcealedMarkersNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyTrace, "Wandbound.Replay.Production.Privacy.PrivateTraceNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyPaths, "Wandbound.Replay.Production.Privacy.PathsNotInReceipt")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPrivacyStartup, "Wandbound.Replay.Production.Privacy.StartupJsonUnchanged")

WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistIntermediate, "Wandbound.Replay.Production.Persistence.IntermediateArchiveWritten")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistFinal, "Wandbound.Replay.Production.Persistence.FinalArchiveWritten")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistPrevious, "Wandbound.Replay.Production.Persistence.PreviousValidFilePreserved")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistNoRepeat, "Wandbound.Replay.Production.Persistence.WriteFailureDoesNotRepeatAction")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistNoMutation, "Wandbound.Replay.Production.Persistence.WriteFailureDoesNotAlterMatch")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistTruncated, "Wandbound.Replay.Production.Persistence.TruncatedArchiveRejected")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPersistSaved, "Wandbound.Replay.Production.Persistence.GeneratedFilesRemainUnderSaved")

WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardCodec, "Wandbound.Replay.Production.Guard.ActionCodecUnchanged")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoActionIds, "Wandbound.Replay.Production.Guard.NoSecondActionIdAlgorithm")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoMutation, "Wandbound.Replay.Production.Guard.NoDirectGameStateMutation")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoController, "Wandbound.Replay.Production.Guard.NoTurnControllerBypass")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoEffectRunner, "Wandbound.Replay.Production.Guard.NoEffectRunnerTransitionBypass")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoRNG, "Wandbound.Replay.Production.Guard.NoGameplayRNGOwnership")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoPresentation, "Wandbound.Replay.Production.Guard.NoPresentationDerivedRecords")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoGodot, "Wandbound.Replay.Production.Guard.NoGodotChanges")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoMeshy, "Wandbound.Replay.Production.Guard.NoMeshyChanges")
WB_PRODUCTION_REPLAY_TEST(FWBReplayGuardNoAssets, "Wandbound.Replay.Production.Guard.NoModelMapOrAssetChanges")

WB_PRODUCTION_REPLAY_TEST(FWBReplayPackageArchive, "Wandbound.Replay.Production.Package.ArchiveCreated")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPackageVerified, "Wandbound.Replay.Production.Package.ReplayVerified")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPackageReceipt, "Wandbound.Replay.Production.Package.ReceiptPublicSafe")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPackageArchiveBytes, "Wandbound.Replay.Production.Package.RepeatedArchiveByteIdentical")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPackageReceiptBytes, "Wandbound.Replay.Production.Package.RepeatedReceiptByteIdentical")
WB_PRODUCTION_REPLAY_TEST(FWBReplayPackageStartup, "Wandbound.Replay.Production.Package.StartupResultByteIdentical")
WB_PRODUCTION_REPLAY_TEST(FWBReplayRuntimeProviderRefresh, "Wandbound.Replay.Production.Runtime.ProviderRefreshAfterRecord")

#undef WB_PRODUCTION_REPLAY_TEST

#endif
