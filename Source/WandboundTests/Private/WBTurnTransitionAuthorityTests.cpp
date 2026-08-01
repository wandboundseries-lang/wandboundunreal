#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBEffectRunner.h"
#include "WBMatchCoordinator.h"
#include "WBMPRollSource.h"
#include "WBProductionRuntimeBootstrap.h"
#include "WBProductionStartupResult.h"
#include "WBReplayTrace.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBTurnController.h"

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

const FWBProductionRuntimeBootstrapResult& GetProductionBootstrap()
{
	static const FWBProductionRuntimeBootstrapResult Bootstrap = []()
	{
		FWBProductionRuntimeBootstrapRequest Request;
		Request.CardBundleManifestPath =
			ProductionPath(TEXT("root_manifest.json"));
		Request.MatchSpecificationPath =
			ProductionPath(TEXT("match_spec.json"));
		return WBProductionRuntimeBootstrap::Build(Request);
	}();
	return Bootstrap;
}

FWBCardDefinition MakeAuthorityCharacter(
	const FString& CardId,
	const EWBCardDefinitionKind Kind =
		EWBCardDefinitionKind::Character)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = 3;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	return Definition;
}

FWBCardInstanceRef MakeAuthorityCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 PlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = PlayerId;
	return Card;
}

FWBMatchPlayerSetup MakeAuthorityPlayer(
	const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(
		TEXT("p%d_hero"),
		PlayerId);
	Setup.HeroCardId =
		PlayerId == 0 ? TEXT("hero_alpha") : TEXT("hero_beta");
	Setup.OrderedDeck.Add(MakeAuthorityCard(
		Setup.HeroInstanceId,
		Setup.HeroCardId,
		PlayerId));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeAuthorityCard(
			FString::Printf(
				TEXT("p%d_filler_%d"),
				PlayerId,
				Index),
			TEXT("test_filler"),
			PlayerId));
	}
	return Setup;
}

FWBSetupMarkerPlacement MakeAuthorityMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile& Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Placement;
	Placement.PlayerId = PlayerId;
	Placement.Type = Type;
	Placement.Tile = Tile;
	Placement.DefinitionId =
		Type == EWBMarkerType::Trap
			? TEXT("basic_trap")
			: TEXT("basic_npc");
	Placement.PlacementOrder = Order;
	return Placement;
}

FWBMatchInitializationRequest MakeAuthorityRequest(
	const bool bAddPendingTurnStartChoice = false)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 424242;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId =
		TEXT("turn_authority_tests");
	Request.Repository.SourceVersion = TEXT("1");
	FWBCardDefinition HeroAlpha =
		MakeAuthorityCharacter(TEXT("hero_alpha"));
	FWBCardDefinition HeroBeta =
		MakeAuthorityCharacter(TEXT("hero_beta"));
	if (bAddPendingTurnStartChoice)
	{
		FWBTurnStartTriggerDefinition First;
		First.TriggerId = TEXT("authority_first");
		First.DrawCount = 1;
		FWBTurnStartTriggerDefinition Second;
		Second.TriggerId = TEXT("authority_second");
		Second.DrawCount = 1;
		HeroBeta.TurnStartTriggers = { First, Second };
	}
	FWBCardDefinition Filler;
	Filler.CardId = TEXT("test_filler");
	Filler.PublicName = TEXT("Filler");
	Filler.Kind = EWBCardDefinitionKind::Action;
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("basic_trap");
	Trap.PublicName = TEXT("Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	Trap.TrapDamage = 1;
	FWBCardDefinition NPC =
		MakeAuthorityCharacter(
			TEXT("basic_npc"),
			EWBCardDefinitionKind::NPC);
	Request.Repository.Definitions = {
		HeroAlpha,
		HeroBeta,
		Filler,
		Trap,
		NPC
	};
	Request.Players = {
		MakeAuthorityPlayer(0),
		MakeAuthorityPlayer(1)
	};
	Request.MarkerPlacements = {
		MakeAuthorityMarker(
			0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeAuthorityMarker(
			0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeAuthorityMarker(
			0, EWBMarkerType::NPC, FWBTile(2, 8), 2),
		MakeAuthorityMarker(
			0, EWBMarkerType::NPC, FWBTile(3, 7), 3),
		MakeAuthorityMarker(
			1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeAuthorityMarker(
			1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeAuthorityMarker(
			1, EWBMarkerType::NPC, FWBTile(2, 0), 6),
		MakeAuthorityMarker(
			1, EWBMarkerType::NPC, FWBTile(3, 1), 7)
	};
	return Request;
}

const FWBMatchLegalAction* FindEndTurn(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate(
		[](const FWBMatchLegalAction& Action)
		{
			return Action.Family
					== EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type
					== EWBActionType::EndTurn;
		});
}

bool ContainsTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.ContainsByPredicate(
		[Kind](const FWBTraceEvent& Event)
		{
			return Event.Kind == Kind;
		});
}

const FWBTraceEvent* FindTrace(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.FindByPredicate(
		[Kind](const FWBTraceEvent& Event)
		{
			return Event.Kind == Kind;
		});
}

TArray<FString> ActionIds(
	const TArray<FWBMatchLegalAction>& Actions)
{
	TArray<FString> Result;
	for (const FWBMatchLegalAction& Action : Actions)
	{
		Result.Add(Action.ActionId);
	}
	return Result;
}

bool StartCoordinator(
	FAutomationTestBase& Test,
	WBMatchCoordinator& Coordinator,
	FWBMatchOperationResult& OutStarted,
	const bool bPendingChoice = false)
{
	Test.TestTrue(
		TEXT("Production bootstrap succeeds"),
		GetProductionBootstrap().bOk);
	if (!GetProductionBootstrap().bOk)
	{
		Test.AddError(GetProductionBootstrap().Reason);
		return false;
	}

	OutStarted = Coordinator.InitializeMatch(
		MakeAuthorityRequest(bPendingChoice));
	Test.TestTrue(TEXT("Coordinator initializes"), OutStarted.bOk);
	return OutStarted.bOk;
}

bool SubmitEndTurn(
	FAutomationTestBase& Test,
	WBMatchCoordinator& Coordinator,
	const FWBMatchOperationResult& Started,
	FWBMatchOperationResult& OutTransition,
	FString& OutActionId)
{
	const FWBMatchLegalAction* EndTurn =
		FindEndTurn(Started.NextLegalActions);
	Test.TestNotNull(TEXT("EndTurn is legal"), EndTurn);
	if (EndTurn == nullptr)
	{
		return false;
	}
	OutActionId = EndTurn->ActionId;
	OutTransition = Coordinator.SubmitActionId(
		EndTurn->PlayerId,
		EndTurn->ActionId);
	Test.TestTrue(*FString::Printf(
		TEXT("EndTurn submission succeeds: %s"),
		*OutTransition.Reason),
		OutTransition.bOk);
	return OutTransition.bOk;
}

bool LoadAudit(
	FAutomationTestBase& Test,
	TSharedPtr<FJsonObject>& OutRoot)
{
	FString Json;
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Docs/Turn_Transition_Authority_Migration_Audit.json"));
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		Test.AddError(TEXT("Authority audit JSON is missing"));
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, OutRoot)
		|| !OutRoot.IsValid())
	{
		Test.AddError(TEXT("Authority audit JSON is malformed"));
		return false;
	}
	return true;
}

bool RunAuditCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	TSharedPtr<FJsonObject> Root;
	if (!LoadAudit(Test, Root))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Callers = nullptr;
	Test.TestTrue(
		TEXT("Audit has caller inventory"),
		Root->TryGetArrayField(TEXT("callers"), Callers));
	if (Callers == nullptr)
	{
		return false;
	}

	const TSet<FString> Categories = {
		TEXT("ProductionRuntime"),
		TEXT("ProductionBootstrap"),
		TEXT("AuthoritativeCore"),
		TEXT("Replay"),
		TEXT("Serialization"),
		TEXT("TestHarness"),
		TEXT("GoldenFixture"),
		TEXT("LegacyCompatibility"),
		TEXT("Unused")
	};
	const TSet<FString> Statuses = {
		TEXT("AlreadyCoordinatorOwned"),
		TEXT("MustMigrate"),
		TEXT("CompatibilityOnly"),
		TEXT("LowLevelPrimitiveOnly"),
		TEXT("TestOnly"),
		TEXT("SafeToDeprecate"),
		TEXT("UnsafeToRemove"),
		TEXT("UnusedConfirmed")
	};
	bool bAllClassified = !Callers->IsEmpty();
	bool bProductionOwned = true;
	bool bCompatibilityExplicit = false;
	for (const TSharedPtr<FJsonValue>& Value : *Callers)
	{
		const TSharedPtr<FJsonObject> Caller =
			Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Caller.IsValid())
		{
			bAllClassified = false;
			continue;
		}
		FString Category;
		FString Status;
		FString Path;
		FString Symbol;
		bAllClassified &=
			Caller->TryGetStringField(
				TEXT("caller_category"), Category)
			&& Categories.Contains(Category)
			&& Caller->TryGetStringField(
				TEXT("migration_status"), Status)
			&& Statuses.Contains(Status)
			&& Caller->TryGetStringField(
				TEXT("caller_path"), Path)
			&& !Path.IsEmpty()
			&& Caller->TryGetStringField(
				TEXT("symbol"), Symbol)
			&& !Symbol.IsEmpty()
			&& Caller->HasField(TEXT("mutates_game_state"))
			&& Caller->HasField(TEXT("owns_rng"))
			&& Caller->HasField(TEXT("advances_player"))
			&& Caller->HasField(TEXT("executes_turn_start"))
			&& Caller->HasField(TEXT("can_pause_for_decision"))
			&& Caller->HasField(TEXT("used_in_production"))
			&& Caller->HasField(TEXT("used_in_replay"))
			&& Caller->HasField(TEXT("used_in_tests"));

		bool bUsedInProduction = false;
		Caller->TryGetBoolField(
			TEXT("used_in_production"),
			bUsedInProduction);
		if (bUsedInProduction
			&& Category == TEXT("ProductionRuntime"))
		{
			bProductionOwned &=
				Status == TEXT("AlreadyCoordinatorOwned");
		}
		bCompatibilityExplicit |=
			Category == TEXT("LegacyCompatibility")
			&& (Status == TEXT("CompatibilityOnly")
				|| Status == TEXT("SafeToDeprecate"));
	}

	Test.TestTrue(TEXT("All callers classified"), bAllClassified);
	Test.TestTrue(TEXT("Production callers coordinator-owned"),
		bProductionOwned);
	Test.TestTrue(TEXT("Compatibility callers explicit"),
		bCompatibilityExplicit);
	Test.TestEqual(
		TEXT("No unknown full transition caller"),
		Root->GetIntegerField(
			TEXT("unknown_full_transition_callers")),
		0);
	return true;
}

bool RunCoordinatorCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const bool bNeedsPause =
		Name.Contains(TEXT("Resume"))
		|| Name.Contains(TEXT("Duplicate"))
		|| Name.Contains(TEXT("StaleDecision"));
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult Started;
	if (!StartCoordinator(
		Test,
		Coordinator,
		Started,
		bNeedsPause))
	{
		return false;
	}

	const int32 EndingPlayer = Coordinator.GetState().CurrentPlayer;
	const int32 EndingTurn = Coordinator.GetState().TurnNumber;
	FWBMatchOperationResult Transition;
	FString EndTurnActionId;
	if (!SubmitEndTurn(
		Test,
		Coordinator,
		Started,
		Transition,
		EndTurnActionId))
	{
		return false;
	}

	Test.TestTrue(
		TEXT("Coordinator emits end-turn status phase"),
		ContainsTrace(
			Transition.TraceEvents,
			FName(TEXT("end_turn_status_ticks"))));
	Test.TestTrue(
		TEXT("Coordinator emits EndTurn"),
		ContainsTrace(
			Transition.TraceEvents,
			FName(TEXT("end_turn"))));
	Test.TestTrue(
		TEXT("Coordinator advances exactly once"),
		Coordinator.GetState().CurrentPlayer != EndingPlayer
			&& Coordinator.GetState().TurnNumber
				== EndingTurn + 1);
	const FWBTraceEvent* MPRoll = FindTrace(
		Transition.TraceEvents,
		FName(TEXT("turn_start_mp_rolled")));
	Test.TestNotNull(TEXT("Coordinator owns MP trace"), MPRoll);
	if (MPRoll != nullptr)
	{
		Test.TestTrue(TEXT("MP roll is valid"),
			MPRoll->MPRoll >= 1 && MPRoll->MPRoll <= 6);
	}

	if (bNeedsPause)
	{
		Test.TestTrue(TEXT("Transition pauses"),
			Transition.bPendingDecision);
		Test.TestTrue(TEXT("Coordinator reports in progress"),
			Coordinator.IsTurnTransitionInProgress());
		Test.TestEqual(TEXT("Pending owner is public"),
			Coordinator.GetPendingTurnStartDecisionPlayerId(),
			Coordinator.GetState().PriorityPlayer);
		Test.TestTrue(TEXT("Only trigger decisions are exposed"),
			!Transition.NextLegalActions.IsEmpty()
			&& Transition.NextLegalActions.ContainsByPredicate(
				[](const FWBMatchLegalAction& Action)
				{
					return Action.Family
						== EWBMatchActionFamily::TurnStartTrigger;
				}));

		const int32 MPRollBefore =
			Coordinator.GetTurnStartSequenceState().MPRoll;
		const int32 MPBefore =
			Coordinator.GetState().GetPlayerById(
				Coordinator.GetState().CurrentPlayer)->RemainingMP;
		const int32 TraceCountBefore =
			Coordinator.GetTraceLog().Num();

		const FWBMatchOperationResult Duplicate =
			Coordinator.SubmitActionId(
				Coordinator.GetState().PriorityPlayer,
				EndTurnActionId);
		Test.TestFalse(TEXT("Duplicate EndTurn is rejected"),
			Duplicate.bOk);
		Test.TestEqual(TEXT("Duplicate diagnostic"),
			Duplicate.Reason,
			FString(TEXT("turn_transition_pending_decision")));

		const FString ChoiceId =
			Transition.NextLegalActions[0].ActionId;
		const FWBMatchOperationResult Resumed =
			Coordinator.SubmitActionId(
				Coordinator.GetState().PriorityPlayer,
				ChoiceId);
		Test.TestTrue(TEXT("Pending transition resumes"),
			Resumed.bOk);
		Test.TestFalse(TEXT("Resume completes"),
			Coordinator.IsTurnTransitionInProgress());
		Test.TestEqual(TEXT("Resume does not reroll"),
			Coordinator.GetTurnStartSequenceState().MPRoll,
			MPRollBefore);
		Test.TestEqual(TEXT("Resume does not reset MP"),
			Coordinator.GetState().GetPlayerById(
				Coordinator.GetState().CurrentPlayer)->RemainingMP,
			MPBefore);
		Test.TestFalse(TEXT("Resume does not retick statuses"),
			ContainsTrace(
				Resumed.TraceEvents,
				FName(TEXT("start_turn_status_ticks"))));
		Test.TestFalse(TEXT("Resume does not reset resources"),
			ContainsTrace(
				Resumed.TraceEvents,
				FName(TEXT("turn_start_attacks_reset")))
			|| ContainsTrace(
				Resumed.TraceEvents,
				FName(TEXT("turn_start_wall_allowance_restored"))));
		Test.TestTrue(TEXT("Resume appends trace"),
			Coordinator.GetTraceLog().Num()
				> TraceCountBefore);
		Test.TestTrue(TEXT("Normal decisions follow completion"),
			!Resumed.NextLegalActions.IsEmpty()
			&& Resumed.NextLegalActions.ContainsByPredicate(
				[](const FWBMatchLegalAction& Action)
				{
					return Action.Family
						!= EWBMatchActionFamily::TurnStartTrigger;
				}));

		const FWBMatchOperationResult Stale =
			Coordinator.SubmitActionId(
				Coordinator.GetState().PriorityPlayer,
				ChoiceId);
		Test.TestFalse(TEXT("Stale decision is rejected"),
			Stale.bOk);
	}
	else
	{
		Test.TestTrue(TEXT("Transition completes"),
			Transition.bCompleted);
		Test.TestFalse(TEXT("No pending decision"),
			Transition.bPendingDecision);
		Test.TestTrue(TEXT("Normal decision generated"),
			!Transition.NextLegalActions.IsEmpty());
	}
	return true;
}

bool RunRuntimeCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const bool bPending =
		Name.Contains(TEXT("PendingDecisionKeepsInputLocked"));
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult Started;
	if (!StartCoordinator(
		Test,
		Coordinator,
		Started,
		bPending))
	{
		return false;
	}
	const FWBMatchLegalAction* EndTurn =
		FindEndTurn(Started.NextLegalActions);
	Test.TestNotNull(TEXT("EndTurn is legal"), EndTurn);
	if (EndTurn == nullptr)
	{
		return false;
	}

	FWBGameStateData State = Coordinator.GetState();
	FWBQueuedMPRollSource LegacyRolls;
	LegacyRolls.EnqueueRoll(6);
	FWBRuntimeTurnResolutionContext Context;
	Context.MatchCoordinator = &Coordinator;
	Context.MPRollSource = &LegacyRolls;
	const FWBRuntimeSelectedActionResult Result =
		WBRuntimeTurnResolutionAdapter::
			ApplyRuntimeSelectedActionWithResult(
				State,
				EndTurn->CoreAction,
				Context);

	Test.TestTrue(TEXT("Runtime action succeeds"),
		Result.ApplyResult.bOk);
	Test.TestTrue(TEXT("Coordinator owns transition"),
		Result.bCoordinatorOwnedTransition);
	Test.TestFalse(TEXT("Legacy runtime RNG not consumed"),
		Result.bConsumedMPRoll);
	Test.TestEqual(TEXT("Queued roll remains"),
		LegacyRolls.NumRemainingRolls(), 1);
	Test.TestEqual(TEXT("State mirrors committed coordinator"),
		State.TurnNumber,
		Coordinator.GetState().TurnNumber);
	Test.TestEqual(TEXT("Runtime reports active player"),
		Result.ActivePlayerId,
		Coordinator.GetState().CurrentPlayer);
	Test.TestTrue(TEXT("Runtime receives coordinator trace"),
		bPending
			? Result.bPendingDecision
			: ContainsTrace(
				Result.ApplyResult.TraceEvents,
				FName(TEXT("turn_started"))));
	return true;
}

bool RunProductionCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	WBMatchCoordinator Coordinator;
	FWBMatchOperationResult Started;
	if (!StartCoordinator(Test, Coordinator, Started))
	{
		return false;
	}
	Test.TestTrue(TEXT("Production startup completes"),
		Started.bCompleted);
	Test.TestTrue(TEXT("Production startup has decision"),
		!Started.NextLegalActions.IsEmpty());
	Test.TestEqual(TEXT("Production starts turn one"),
		Coordinator.GetState().TurnNumber, 1);

	FWBProductionRuntimeBootstrapRequest StartupRequest;
	StartupRequest.MatchSpecificationPath =
		ProductionPath(TEXT("match_spec.json"));
	const FWBProductionStartupResult Baseline =
		WBProductionStartupResult::FromBootstrap(
			StartupRequest,
			GetProductionBootstrap());
	const FWBProductionStartupResult PublicResult =
		WBProductionStartupResult::StartedFromBootstrap(
			Baseline,
			Coordinator,
			1,
			2,
			!Started.NextLegalActions.IsEmpty());
	const FString Serialized =
		WBProductionStartupResult::Serialize(PublicResult);
	Test.TestEqual(TEXT("Startup contract preserved"),
		PublicResult.ResultCode,
		FString(TEXT("production_started")));
	Test.TestFalse(TEXT("Startup result hides instances"),
		Serialized.Contains(TEXT("instance_id")));

	if (Name.Contains(TEXT("RepeatedResultByteIdentical")))
	{
		WBMatchCoordinator Again;
		FWBMatchOperationResult AgainStarted;
		if (!StartCoordinator(Test, Again, AgainStarted))
		{
			return false;
		}
		const FWBProductionStartupResult AgainResult =
			WBProductionStartupResult::StartedFromBootstrap(
				Baseline,
				Again,
				1,
				2,
				!AgainStarted.NextLegalActions.IsEmpty());
		Test.TestEqual(TEXT("Startup bytes stable"),
			WBProductionStartupResult::Serialize(AgainResult),
			Serialized);
	}
	return true;
}

bool RunReplayCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const bool bPause =
		Name.Contains(TEXT("PendingTriggerDecision"));
	const FWBMatchInitializationRequest Request =
		MakeAuthorityRequest(bPause);
	WBMatchCoordinator First;
	WBMatchCoordinator Second;
	const FWBMatchOperationResult FirstStarted =
		First.InitializeMatch(Request);
	const FWBMatchOperationResult SecondStarted =
		Second.InitializeMatch(Request);
	Test.TestTrue(TEXT("First replay starts"),
		FirstStarted.bOk);
	Test.TestTrue(TEXT("Second replay starts"),
		SecondStarted.bOk);
	if (!FirstStarted.bOk || !SecondStarted.bOk)
	{
		return false;
	}

	const FWBMatchLegalAction* FirstEnd =
		FindEndTurn(FirstStarted.NextLegalActions);
	const FWBMatchLegalAction* SecondEnd =
		FindEndTurn(SecondStarted.NextLegalActions);
	Test.TestNotNull(TEXT("First EndTurn"), FirstEnd);
	Test.TestNotNull(TEXT("Second EndTurn"), SecondEnd);
	if (FirstEnd == nullptr || SecondEnd == nullptr)
	{
		return false;
	}
	Test.TestEqual(TEXT("Recorded action IDs stable"),
		FirstEnd->ActionId,
		SecondEnd->ActionId);

	const FWBMatchOperationResult FirstResult =
		First.SubmitActionId(
			FirstEnd->PlayerId,
			FirstEnd->ActionId);
	const FWBMatchOperationResult SecondResult =
		Second.SubmitActionId(
			SecondEnd->PlayerId,
			SecondEnd->ActionId);
	Test.TestTrue(TEXT("First replay EndTurn succeeds"),
		FirstResult.bOk);
	Test.TestTrue(TEXT("Second replay EndTurn succeeds"),
		SecondResult.bOk);
	if (!FirstResult.bOk || !SecondResult.bOk)
	{
		Test.AddError(FString::Printf(
			TEXT("Replay EndTurn failure: first=%s second=%s"),
			*FirstResult.Reason,
			*SecondResult.Reason));
		return false;
	}
	Test.TestEqual(TEXT("Replay traces equivalent"),
		WBReplayTrace::SerializeEvents(
			FirstResult.TraceEvents),
		WBReplayTrace::SerializeEvents(
			SecondResult.TraceEvents));
	Test.TestEqual(TEXT("Replay legal action IDs stable"),
		ActionIds(FirstResult.NextLegalActions),
		ActionIds(SecondResult.NextLegalActions));
	Test.TestEqual(TEXT("Replay RNG state outcome stable"),
		First.GetTurnStartSequenceState().MPRoll,
		Second.GetTurnStartSequenceState().MPRoll);
	Test.TestEqual(TEXT("Replay pause boundary stable"),
		FirstResult.bPendingDecision,
		SecondResult.bPendingDecision);

	if (bPause)
	{
		if (FirstResult.NextLegalActions.IsEmpty()
			|| SecondResult.NextLegalActions.IsEmpty())
		{
			Test.AddError(
				TEXT("Replay pause produced no legal choice"));
			return false;
		}
		const FString ChoiceId =
			FirstResult.NextLegalActions[0].ActionId;
		Test.TestEqual(TEXT("Replay trigger action stable"),
			ChoiceId,
			SecondResult.NextLegalActions[0].ActionId);
		const FWBMatchOperationResult FirstResume =
			First.SubmitActionId(
				First.GetState().PriorityPlayer,
				ChoiceId);
		const FWBMatchOperationResult SecondResume =
			Second.SubmitActionId(
				Second.GetState().PriorityPlayer,
				ChoiceId);
		Test.TestEqual(TEXT("Resume trace equivalent"),
			WBReplayTrace::SerializeEvents(
				FirstResume.TraceEvents),
			WBReplayTrace::SerializeEvents(
				SecondResume.TraceEvents));
	}
	return true;
}

bool RunCompatibilityCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	WBMatchCoordinator SeedCoordinator;
	FWBMatchOperationResult Started;
	if (!StartCoordinator(
		Test,
		SeedCoordinator,
		Started))
	{
		return false;
	}
	const int32 PlayerId =
		SeedCoordinator.GetState().CurrentPlayer;
	constexpr int32 ExplicitRoll = 4;
	FWBGameStateData DirectState =
		SeedCoordinator.GetState();
	FWBGameStateData ControllerState = DirectState;
	FWBGameStateData EffectState = DirectState;

	const FWBApplyActionResult Direct =
		WBMatchCoordinator::
			ApplyLegacyCompatibilityTurnTransition(
				DirectState,
				PlayerId,
				ExplicitRoll);
	FWBTurnCommand Command;
	Command.Mode =
		EWBTurnCommandMode::DeterministicFullTransition;
	Command.ActingPlayerId = PlayerId;
	Command.NextPlayerExplicitMPRoll = ExplicitRoll;
	const FWBApplyActionResult Controller =
		WBTurnController::ApplyTurnCommand(
			ControllerState,
			Command);
	const FWBApplyActionResult Effect =
		WBEffectRunner::
			ApplyDeterministicTurnTransition(
				EffectState,
				PlayerId,
				ExplicitRoll);

	Test.TestTrue(TEXT("Compatibility helper succeeds"),
		Direct.bOk && Controller.bOk && Effect.bOk);
	Test.TestEqual(TEXT("Controller delegates trace"),
		WBReplayTrace::SerializeEvents(
			Controller.TraceEvents),
		WBReplayTrace::SerializeEvents(
			Direct.TraceEvents));
	Test.TestEqual(TEXT("Effect helper delegates trace"),
		WBReplayTrace::SerializeEvents(
			Effect.TraceEvents),
		WBReplayTrace::SerializeEvents(
			Direct.TraceEvents));
	Test.TestEqual(TEXT("Compatibility turn state"),
		ControllerState.TurnNumber,
		DirectState.TurnNumber);

	FString ControllerHeader;
	FFileHelper::LoadFileToString(
		ControllerHeader,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source/WandboundCore/Public/WBTurnController.h")));
	Test.TestTrue(TEXT("Deprecation is documented"),
		ControllerHeader.Contains(TEXT("Compatibility only"))
		&& ControllerHeader.Contains(
			TEXT("Production full transitions use WBMatchCoordinator")));
	return true;
}

void FindProductionSourceFiles(TArray<FString>& OutFiles)
{
	const TArray<FString> Roots = {
		FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source/WandboundRuntime")),
		FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Source/WandboundUE"))
	};
	for (const FString& Root : Roots)
	{
		IFileManager::Get().FindFilesRecursive(
			OutFiles,
			*Root,
			TEXT("*.cpp"),
			true,
			false);
		IFileManager::Get().FindFilesRecursive(
			OutFiles,
			*Root,
			TEXT("*.h"),
			true,
			false);
	}
}

bool RunSourceGuardCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	TArray<FString> Files;
	FindProductionSourceFiles(Files);
	Test.TestTrue(TEXT("Production files discovered"),
		!Files.IsEmpty());
	FString ProductionSource;
	for (const FString& File : Files)
	{
		FString Contents;
		if (FFileHelper::LoadFileToString(Contents, *File))
		{
			ProductionSource += Contents;
		}
	}

	Test.TestFalse(TEXT("No production TurnController call"),
		ProductionSource.Contains(
			TEXT("WBTurnController::")));
	Test.TestFalse(TEXT("No production full helper call"),
		ProductionSource.Contains(
			TEXT("ApplyDeterministicTurnTransition(")));
	Test.TestFalse(TEXT("No runtime player advance"),
		ProductionSource.Contains(
			TEXT("AdvanceTurnBasic(")));
	Test.TestFalse(TEXT("No runtime turn-start mutation"),
		ProductionSource.Contains(
			TEXT("ApplyTurnStartResourceSetupForPlayer("))
		|| ProductionSource.Contains(
			TEXT("ApplyTurnStartMPRollForPlayer("))
		|| ProductionSource.Contains(
			TEXT("ResetTurnStartResourcesForPlayer(")));
	Test.TestFalse(TEXT("No runtime gameplay RNG"),
		ProductionSource.Contains(
			TEXT("TryGetNextMPRoll(")));

	FString Audit;
	FFileHelper::LoadFileToString(
		Audit,
		*FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Docs/Turn_Transition_Authority_Migration_Audit.md")));
	Test.TestFalse(TEXT("Task audit excludes Godot edits"),
		Audit.Contains(TEXT("Reference/GodotProject/")));
	Test.TestFalse(TEXT("Task audit excludes Meshy edits"),
		Audit.Contains(TEXT("Plugins/meshy/Content/")));
	Test.TestFalse(TEXT("Task audit excludes model/map edits"),
		Audit.Contains(TEXT(".uasset"))
		|| Audit.Contains(TEXT(".umap")));
	return true;
}

bool RunTurnAuthorityCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	if (Name.Contains(TEXT(".Audit.")))
	{
		return RunAuditCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Coordinator.")))
	{
		return RunCoordinatorCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Runtime.")))
	{
		return RunRuntimeCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Production.")))
	{
		return RunProductionCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Replay.")))
	{
		return RunReplayCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Compatibility.")))
	{
		return RunCompatibilityCase(Test, Name);
	}
	return RunSourceGuardCase(Test, Name);
}
}

#define IMPLEMENT_TURN_AUTHORITY_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST( \
		ClassName, TestName, \
		EAutomationTestFlags::EditorContext \
			| EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) \
	{ \
		(void)Parameters; \
		return RunTurnAuthorityCase(*this, TEXT(TestName)); \
	}

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityAuditAllCallers,
	"Wandbound.TurnAuthority.Audit.AllTurnControllerCallersClassified")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityAuditProductionOwned,
	"Wandbound.TurnAuthority.Audit.AllProductionCallersCoordinatorOwned")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityAuditNoUnknown,
	"Wandbound.TurnAuthority.Audit.NoUnknownFullTransitionCaller")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityAuditCompatibility,
	"Wandbound.TurnAuthority.Audit.CompatibilityCallersExplicit")

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorOwnsEndTurn,
	"Wandbound.TurnAuthority.Coordinator.OwnsEndTurnTransition")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorOwnsAdvance,
	"Wandbound.TurnAuthority.Coordinator.OwnsNextPlayerAdvance")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorOwnsRNG,
	"Wandbound.TurnAuthority.Coordinator.OwnsMPRNG")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorOwnsTurnStart,
	"Wandbound.TurnAuthority.Coordinator.OwnsTurnStartSequence")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorFirstDecision,
	"Wandbound.TurnAuthority.Coordinator.GeneratesFirstNormalDecision")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorDuplicate,
	"Wandbound.TurnAuthority.Coordinator.DuplicateEndTurnRejected")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorNoReroll,
	"Wandbound.TurnAuthority.Coordinator.ResumeDoesNotReroll")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorNoRetick,
	"Wandbound.TurnAuthority.Coordinator.ResumeDoesNotRetickStatuses")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorNoResetTwice,
	"Wandbound.TurnAuthority.Coordinator.ResumeDoesNotResetResourcesTwice")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCoordinatorStaleDecision,
	"Wandbound.TurnAuthority.Coordinator.StaleDecisionRejected")

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityRuntimeCoordinator,
	"Wandbound.TurnAuthority.Runtime.EndTurnUsesCoordinator")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityRuntimePendingLocked,
	"Wandbound.TurnAuthority.Runtime.PendingDecisionKeepsInputLocked")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityRuntimeCompleteUnlocked,
	"Wandbound.TurnAuthority.Runtime.CompletedTransitionUnlocksInput")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityRuntimeNoController,
	"Wandbound.TurnAuthority.Runtime.NoDirectTurnControllerCall")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityRuntimeNoRNG,
	"Wandbound.TurnAuthority.Runtime.NoGameplayRNGOwnership")

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityProductionBootstrap,
	"Wandbound.TurnAuthority.Production.BootstrapUsesCoordinator")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityProductionStartup,
	"Wandbound.TurnAuthority.Production.StartupResultPreserved")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityProductionFirstDecision,
	"Wandbound.TurnAuthority.Production.FirstTurnDecisionPreserved")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityProductionRepeated,
	"Wandbound.TurnAuthority.Production.RepeatedResultByteIdentical")

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityReplayCoordinator,
	"Wandbound.TurnAuthority.Replay.EndTurnUsesCoordinator")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityReplayTrace,
	"Wandbound.TurnAuthority.Replay.TraceEquivalent")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityReplayPending,
	"Wandbound.TurnAuthority.Replay.PendingTriggerDecisionPreserved")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityReplayRNG,
	"Wandbound.TurnAuthority.Replay.RNGConsumptionStable")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityReplayActionIds,
	"Wandbound.TurnAuthority.Replay.ActionIdsStable")

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCompatibilityDelegates,
	"Wandbound.TurnAuthority.Compatibility.AdapterDelegates")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCompatibilityNoAlgorithm,
	"Wandbound.TurnAuthority.Compatibility.NoIndependentFullAlgorithm")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCompatibilityDeprecated,
	"Wandbound.TurnAuthority.Compatibility.DeprecationDocumented")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityCompatibilityFixture,
	"Wandbound.TurnAuthority.Compatibility.TestFixtureStillSupported")

IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityGuardNoProductionController,
	"Wandbound.Authority.TurnTransition.NoProductionDirectTurnControllerCaller")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityGuardNoRuntimeMutation,
	"Wandbound.Authority.TurnTransition.NoRuntimeTurnMutation")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityGuardNoDuplicateRNG,
	"Wandbound.Authority.TurnTransition.NoDuplicateRNGPath")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityGuardNoGodot,
	"Wandbound.Authority.TurnTransition.NoGodotChanges")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityGuardNoMeshy,
	"Wandbound.Authority.TurnTransition.NoMeshyChanges")
IMPLEMENT_TURN_AUTHORITY_TEST(
	FWBTurnAuthorityGuardNoModels,
	"Wandbound.Authority.TurnTransition.NoModelOrMapChanges")

#undef IMPLEMENT_TURN_AUTHORITY_TEST

#endif
