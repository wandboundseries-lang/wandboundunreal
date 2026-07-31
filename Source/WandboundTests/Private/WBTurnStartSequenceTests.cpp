#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardLifecycle.h"
#include "WBProductionRuntimeBootstrap.h"
#include "WBProductionStartupResult.h"
#include "WBReplayTrace.h"
#include "WBTurnStartSequence.h"

namespace
{
FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const int32 HP = 6)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind =
		EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = HP;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	return Definition;
}

FWBCardDefinition MakeFiller()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("filler");
	Definition.PublicName = TEXT("Filler");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBTurnStartTriggerDefinition MakeDrawTrigger(
	const FString& TriggerId)
{
	FWBTurnStartTriggerDefinition Trigger;
	Trigger.TriggerId = TriggerId;
	Trigger.DrawCount = 1;
	return Trigger;
}

FWBTurnStartTriggerDefinition MakeDamageTrigger(
	const FString& TriggerId)
{
	FWBTurnStartTriggerDefinition Trigger;
	Trigger.TriggerId = TriggerId;
	Trigger.TargetRequirement =
		EWBCardEffectTargetRequirement::Unit;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = 1;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause =
		FName(TEXT("TurnStartEffect"));
	Payload.DamageEffect.SourceReason =
		FName(TEXT("turn_start_trigger"));
	Trigger.Payloads.Add(Payload);
	return Trigger;
}

FWBCardInstanceRef MakeCard(
	const FString& InstanceId,
	const int32 OwnerPlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = TEXT("filler");
	Card.OwnerPlayerId = OwnerPlayerId;
	return Card;
}

void AddDeck(
	FWBGameStateData& State,
	const int32 PlayerId,
	const int32 Count)
{
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FWBZoneCardEntry Entry;
		Entry.Card = MakeCard(
			FString::Printf(
				TEXT("private_p%d_%d"),
				PlayerId,
				Index),
			PlayerId);
		Entry.Zone = EWBCardZone::Deck;
		Entry.ZoneIndex = Index;
		Zones.Deck.Add(Entry);
	}
	State.CardZoneState.PlayerZones.Add(Zones);
}

FWBGameStateData MakeState(
	const int32 ActivePlayerId = 0,
	const int32 TurnNumber = 1,
	const int32 FirstPlayerId = 0,
	const int32 DeckCount = 6)
{
	FWBGameStateData State;
	State.CurrentPlayer = ActivePlayerId;
	State.PriorityPlayer = ActivePlayerId;
	State.FirstPlayerId = FirstPlayerId;
	State.TurnNumber = TurnNumber;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = 10;
	Player0.WallsLeft = 0;
	Player0.RemainingMP = 5;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 20;
	Player1.WallsLeft = 0;
	Player1.RemainingMP = 4;
	State.Players = { Player0, Player1 };

	FWBUnitState Hero0;
	Hero0.UnitId = 10;
	Hero0.OwnerId = 0;
	Hero0.CardId = TEXT("hero_0");
	Hero0.X = 4;
	Hero0.Y = 8;
	Hero0.HP = 6;
	Hero0.MaxHP = 6;
	Hero0.AttacksLeft = 0;
	Hero0.MaxAttacksPerTurn = 2;
	Hero0.SetCanonicalRL(3, 3, 0);
	FWBUnitState Hero1;
	Hero1.UnitId = 20;
	Hero1.OwnerId = 1;
	Hero1.CardId = TEXT("hero_1");
	Hero1.X = 4;
	Hero1.Y = 0;
	Hero1.HP = 6;
	Hero1.MaxHP = 6;
	Hero1.AttacksLeft = 0;
	Hero1.MaxAttacksPerTurn = 1;
	Hero1.SetCanonicalRL(3, 3, 0);
	State.Units = { Hero0, Hero1 };
	AddDeck(State, 0, DeckCount);
	AddDeck(State, 1, DeckCount);
	return State;
}

FWBCardDefinitionRepository MakeRepository(
	const TArray<FWBTurnStartTriggerDefinition>& Player0Triggers =
		TArray<FWBTurnStartTriggerDefinition>(),
	const TArray<FWBTurnStartTriggerDefinition>& Player1Triggers =
		TArray<FWBTurnStartTriggerDefinition>())
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("turn_start_tests");
	Repository.SourceVersion = TEXT("1");
	FWBCardDefinition Hero0 =
		MakeCharacter(TEXT("hero_0"));
	Hero0.TurnStartTriggers = Player0Triggers;
	FWBCardDefinition Hero1 =
		MakeCharacter(TEXT("hero_1"));
	Hero1.TurnStartTriggers = Player1Triggers;
	Repository.Definitions = {
		Hero0,
		Hero1,
		MakeFiller()
	};
	return Repository;
}

int32 TraceIndex(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.IndexOfByPredicate(
		[Kind](const FWBTraceEvent& Event)
		{
			return Event.Kind == Kind;
		});
}

int32 TraceCount(
	const TArray<FWBTraceEvent>& Events,
	const FName Kind)
{
	return Events.FilterByPredicate(
		[Kind](const FWBTraceEvent& Event)
		{
			return Event.Kind == Kind;
		}).Num();
}

int32 DeckCount(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	const FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindPlayerZones(
			State.CardZoneState,
			PlayerId);
	return Zones == nullptr ? -1 : Zones->Deck.Num();
}

int32 HandCount(
	const FWBGameStateData& State,
	const int32 PlayerId)
{
	const FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindPlayerZones(
			State.CardZoneState,
			PlayerId);
	return Zones == nullptr ? -1 : Zones->Hand.Num();
}

FWBTurnStartSequenceResult Begin(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	FWBTurnStartSequenceState& Sequence,
	const int32 Roll = 4)
{
	return WBTurnStartSequence::Begin(
		State,
		Repository,
		State.CurrentPlayer,
		Roll,
		Sequence);
}

FString ProductionPath(const FString& Name)
{
	return FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/InitialCanonical"),
		Name);
}

FWBProductionRuntimeBootstrapResult BootstrapProduction()
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath =
		ProductionPath(TEXT("root_manifest.json"));
	Request.MatchSpecificationPath =
		ProductionPath(TEXT("match_spec.json"));
	return WBProductionRuntimeBootstrap::Build(Request);
}

bool RunOrderingCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FWBGameStateData State = MakeState();
	FWBTurnStartSequenceState Sequence;
	const FWBTurnStartSequenceResult Result =
		Begin(State, MakeRepository(), Sequence);
	Test.TestTrue(TEXT("Sequence succeeds"), Result.bOk);
	Test.TestTrue(TEXT("Sequence completes"), Result.bCompleted);
	const int32 Draw = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("turn_start_draw_skipped")));
	const int32 MP = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("turn_start_mp_rolled")));
	const int32 Reset = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("turn_start_attacks_reset")));
	const int32 Status = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("turn_start_status_phase_started")));
	const int32 Effects = TraceIndex(
		Result.TraceEvents,
		FName(TEXT("turn_start_triggers_collected")));
	Test.TestTrue(TEXT("Draw before MP"), Draw < MP);
	Test.TestTrue(TEXT("MP before reset"), MP < Reset);
	Test.TestTrue(TEXT("Reset before statuses"), Reset < Status);
	Test.TestTrue(TEXT("Statuses before effects"), Status < Effects);

	if (Name.EndsWith(TEXT("CompleteSequenceDeterministic")))
	{
		FWBGameStateData ReplayState = MakeState();
		FWBTurnStartSequenceState ReplaySequence;
		const FWBTurnStartSequenceResult Replay =
			Begin(
				ReplayState,
				MakeRepository(),
				ReplaySequence);
		Test.TestEqual(
			TEXT("Trace replay stable"),
			WBReplayTrace::SerializeEvents(Result.TraceEvents),
			WBReplayTrace::SerializeEvents(Replay.TraceEvents));
		Test.TestEqual(
			TEXT("Final MP stable"),
			State.GetPlayerById(0)->RemainingMP,
			ReplayState.GetPlayerById(0)->RemainingMP);
	}
	return true;
}

bool RunDrawCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const bool bSecondPlayer =
		Name.Contains(TEXT("SecondPlayer"));
	const bool bFirstPlayerTurnTwo =
		Name.Contains(TEXT("FirstPlayerTurnTwo"))
		|| Name.Contains(TEXT("PrivateIdentityProtected"))
		|| Name.Contains(TEXT("EmptyDeck"));
	const int32 PlayerId = bSecondPlayer ? 1 : 0;
	const int32 TurnNumber =
		bSecondPlayer ? 2 : (bFirstPlayerTurnTwo ? 3 : 1);
	const int32 StartingDeck =
		Name.Contains(TEXT("EmptyDeck")) ? 0 : 3;
	FWBGameStateData State = MakeState(
		PlayerId,
		TurnNumber,
		0,
		StartingDeck);
	FWBTurnStartSequenceState Sequence;
	const FWBTurnStartSequenceResult Result =
		Begin(State, MakeRepository(), Sequence);

	if (Name.Contains(TEXT("EmptyDeck")))
	{
		Test.TestFalse(TEXT("Empty deck behavior fails closed"), Result.bOk);
		Test.TestEqual(TEXT("Empty deck diagnostic"),
			Result.Reason, FString(TEXT("deck_empty")));
		return true;
	}

	Test.TestTrue(TEXT("Draw sequence succeeds"), Result.bOk);
	const bool bShouldSkip =
		PlayerId == 0 && TurnNumber == 1;
	Test.TestEqual(
		TEXT("Skip policy"),
		Sequence.bDrawSkipped,
		bShouldSkip);
	Test.TestEqual(
		TEXT("Deck consumption"),
		DeckCount(State, PlayerId),
		StartingDeck - (bShouldSkip ? 0 : 1));
	Test.TestEqual(
		TEXT("Hand growth"),
		HandCount(State, PlayerId),
		bShouldSkip ? 0 : 1);

	if (Name.Contains(TEXT("NoFalseDrawTrace")))
	{
		Test.TestEqual(
			TEXT("No false card draw event"),
			TraceCount(
				Result.TraceEvents,
				FName(TEXT("turn_start_card_drawn"))),
			0);
	}
	if (Name.Contains(TEXT("PrivateIdentityProtected")))
	{
		const FString Serialized =
			WBReplayTrace::SerializeEvents(
				Result.TraceEvents);
		Test.TestFalse(TEXT("Private instance hidden"),
			Serialized.Contains(TEXT("private_p")));
		Test.TestFalse(TEXT("Private card identity hidden"),
			Serialized.Contains(TEXT("\"card_id\":\"filler\"")));
	}
	if (Name.Contains(TEXT("DistinctFromOpeningHandDraw")))
	{
		Test.TestEqual(
			TEXT("No setup opening draw reason"),
			TraceIndex(
				Result.TraceEvents,
				FName(TEXT("opening_hand_draw"))),
			INDEX_NONE);
	}
	return true;
}

bool RunMPCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	TArray<FWBTurnStartTriggerDefinition> Triggers;
	if (Name.Contains(TEXT("NoReroll")))
	{
		Triggers = {
			MakeDrawTrigger(TEXT("alpha")),
			MakeDrawTrigger(TEXT("beta"))
		};
	}
	const FWBCardDefinitionRepository Repository =
		MakeRepository(Triggers);
	FWBGameStateData State = MakeState();
	FWBTurnStartSequenceState Sequence;
	FWBTurnStartSequenceResult Result =
		Begin(State, Repository, Sequence, 3);
	Test.TestTrue(TEXT("MP sequence succeeds"), Result.bOk);
	Test.TestEqual(TEXT("Exactly one MP trace"),
		TraceCount(Result.TraceEvents,
			FName(TEXT("turn_start_mp_rolled"))), 1);
	Test.TestEqual(TEXT("MP result applied"),
		State.GetPlayerById(0)->RemainingMP, 3);
	Test.TestEqual(TEXT("Last roll applied"),
		State.GetPlayerById(0)->LastMPRoll, 3);

	if (Name.Contains(TEXT("ReplayStable"))
		|| Name.Contains(TEXT("AuthoritativeRNG")))
	{
		FWBGameStateData ReplayState = MakeState();
		FWBTurnStartSequenceState ReplaySequence;
		const FWBTurnStartSequenceResult Replay =
			Begin(ReplayState, Repository, ReplaySequence, 3);
		Test.TestEqual(TEXT("MP replay trace"),
			WBReplayTrace::SerializeEvents(Result.TraceEvents),
			WBReplayTrace::SerializeEvents(Replay.TraceEvents));
	}
	if (Name.Contains(TEXT("NoReroll")))
	{
		Test.TestTrue(TEXT("Trigger choice pauses"), Result.bChoiceRequired);
		const int32 MPRollBefore =
			State.GetPlayerById(0)->LastMPRoll;
		const FWBTurnStartSequenceResult Continued =
			WBTurnStartSequence::SubmitChoice(
				State,
				Repository,
				Result.LegalChoiceActionIds[0],
				Sequence);
		Test.TestTrue(TEXT("Choice resumes"), Continued.bOk);
		Test.TestEqual(TEXT("No reroll after resume"),
			State.GetPlayerById(0)->LastMPRoll,
			MPRollBefore);
		Test.TestEqual(TEXT("Resume emits no MP roll"),
			TraceCount(Continued.TraceEvents,
				FName(TEXT("turn_start_mp_rolled"))), 0);
	}
	return true;
}

bool RunResetCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FWBGameStateData State = MakeState();
	if (Name.Contains(TEXT("InactiveUnitsIgnored")))
	{
		State.GetMutableUnitById(10)->RemoveUnitFromBoard();
	}
	State.GetMutableUnitById(10)->MaxAttacksPerTurn = 3;
	State.GetMutableUnitById(10)->AttacksLeft = 0;
	State.GetMutableUnitById(20)->AttacksLeft = 0;
	TArray<FWBTurnStartTriggerDefinition> Triggers;
	if (Name.Contains(TEXT("NoDuplicateReset")))
	{
		Triggers = {
			MakeDrawTrigger(TEXT("alpha")),
			MakeDrawTrigger(TEXT("beta"))
		};
	}
	const FWBCardDefinitionRepository Repository =
		MakeRepository(Triggers);
	FWBTurnStartSequenceState Sequence;
	const FWBTurnStartSequenceResult Result =
		Begin(State, Repository, Sequence, 2);
	Test.TestTrue(TEXT("Reset sequence succeeds"), Result.bOk);
	const FWBUnitState* ActiveUnit = State.GetUnitById(10);
	if (Name.Contains(TEXT("InactiveUnitsIgnored")))
	{
		Test.TestEqual(TEXT("Inactive unit not reset"),
			ActiveUnit->AttacksLeft, 0);
	}
	else
	{
		Test.TestEqual(TEXT("Modified attack limit preserved"),
			ActiveUnit->AttacksLeft, 3);
	}
	Test.TestEqual(TEXT("Opponent attack unchanged"),
		State.GetUnitById(20)->AttacksLeft, 0);
	Test.TestEqual(TEXT("Wall restored"),
		State.GetPlayerById(0)->WallsLeft, 1);
	Test.TestEqual(TEXT("Opponent wall unchanged"),
		State.GetPlayerById(1)->WallsLeft, 0);
	Test.TestEqual(TEXT("One reset trace"),
		TraceCount(Result.TraceEvents,
			FName(TEXT("turn_start_attacks_reset"))), 1);
	if (Name.Contains(TEXT("NoDuplicateReset")))
	{
		Test.TestTrue(TEXT("Sequence pauses after reset"),
			Result.bChoiceRequired);
		const FWBTurnStartSequenceResult Continued =
			WBTurnStartSequence::SubmitChoice(
				State,
				Repository,
				Result.LegalChoiceActionIds[0],
				Sequence);
		Test.TestTrue(TEXT("Sequence resumes"), Continued.bOk);
		Test.TestEqual(TEXT("Resume does not reset again"),
			TraceCount(Continued.TraceEvents,
				FName(TEXT("turn_start_attacks_reset"))), 0);
	}
	return true;
}

bool RunStatusCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FWBGameStateData State = MakeState();
	FWBUnitState* Hero = State.GetMutableUnitById(10);
	Hero->AddStatus(FName(TEXT("Poison")), 1);
	if (Name.Contains(TEXT("Lethal"))
		|| Name.Contains(TEXT("DeathCleanup"))
		|| Name.Contains(TEXT("Terminal")))
	{
		Hero->HP = 0;
	}
	FWBTurnStartSequenceState Sequence;
	const FWBTurnStartSequenceResult Result =
		Begin(State, MakeRepository(), Sequence);
	Test.TestTrue(TEXT("Status sequence accepted"), Result.bOk);
	const int32 Reset = TraceIndex(Result.TraceEvents,
		FName(TEXT("turn_start_attacks_reset")));
	const int32 Status = TraceIndex(Result.TraceEvents,
		FName(TEXT("start_turn_status_ticks")));
	Test.TestTrue(TEXT("Status after reset"), Reset < Status);

	if (Name.Contains(TEXT("DurationDecrements"))
		|| Name.Contains(TEXT("Expiration")))
	{
		Test.TestFalse(TEXT("Timed poison expires"),
			State.GetUnitById(10)->HasStatus(
				FName(TEXT("Poison"))));
		Test.TestTrue(TEXT("Expiration before effect collection"),
			TraceIndex(Result.TraceEvents,
				FName(TEXT("status_expired")))
				< TraceIndex(Result.TraceEvents,
					FName(TEXT("turn_start_triggers_collected"))));
	}
	if (Name.Contains(TEXT("Lethal"))
		|| Name.Contains(TEXT("DeathCleanup")))
	{
		Test.TestFalse(TEXT("Zero-HP Hero leaves board"),
			State.GetUnitById(10)->IsUnitOnBoard());
	}
	if (Name.Contains(TEXT("Terminal")))
	{
		Test.TestTrue(TEXT("Terminal match stops sequence"),
			Result.bTerminal);
		Test.TestFalse(TEXT("Effects are not collected"),
			Sequence.bEffectsResolved);
		Test.TestTrue(TEXT("Game over"), State.bGameOver);
	}
	return true;
}

bool RunEffectCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const bool bTargetChoice =
		Name.Contains(TEXT("RequiredTargetChoice"));
	const bool bMultiple =
		Name.Contains(TEXT("MultipleTriggers"))
		|| Name.Contains(TEXT("ControllerChooses"))
		|| Name.Contains(TEXT("StableActionIds"))
		|| Name.Contains(TEXT("ReplayPreserves"))
		|| Name.Contains(TEXT("RequiredTargetChoice"));
	TArray<FWBTurnStartTriggerDefinition> Triggers;
	if (bTargetChoice)
	{
		Triggers.Add(MakeDamageTrigger(TEXT("targeted")));
	}
	else
	{
		Triggers.Add(MakeDrawTrigger(TEXT("alpha")));
		if (bMultiple)
		{
			Triggers.Add(MakeDrawTrigger(TEXT("beta")));
		}
	}
	FWBGameStateData State = MakeState();
	if (Name.Contains(TEXT("DeadSource")))
	{
		State.GetMutableUnitById(10)->HP = 0;
	}
	if (Name.Contains(TEXT("NegatedSource")))
	{
		State.GetMutableUnitById(10)->AddStatus(
			FName(TEXT("Negated")));
	}
	const FWBCardDefinitionRepository Repository =
		MakeRepository(Triggers);
	FWBTurnStartSequenceState Sequence;
	FWBTurnStartSequenceResult Result =
		Begin(State, Repository, Sequence);
	Test.TestTrue(TEXT("Effect sequence succeeds"), Result.bOk);

	if (Name.Contains(TEXT("DeadSource"))
		|| Name.Contains(TEXT("NegatedSource")))
	{
		Test.TestEqual(TEXT("Invalid source not collected"),
			Sequence.PendingTriggers.Num(), 0);
		Test.TestTrue(TEXT("Sequence completes without source"),
			Result.bCompleted || Result.bTerminal);
		return true;
	}
	if (bMultiple)
	{
		Test.TestTrue(TEXT("Choice required"), Result.bChoiceRequired);
		Test.TestTrue(TEXT("Stable choices available"),
			Result.LegalChoiceActionIds.Num() >= 2);
		for (const FString& ActionId : Result.LegalChoiceActionIds)
		{
			Test.TestTrue(TEXT("Stable action namespace"),
				ActionId.StartsWith(TEXT("turn_start_trigger:p")));
		}
		const FString Selected =
			Result.LegalChoiceActionIds.Last();
		const FWBTurnStartSequenceResult Continued =
			WBTurnStartSequence::SubmitChoice(
				State,
				Repository,
				Selected,
				Sequence);
		Test.TestTrue(TEXT("Selected order resolves"), Continued.bOk);
		Test.TestTrue(TEXT("Sequence completes after choice"),
			Continued.bCompleted);
		Test.TestEqual(TEXT("Selected choice traced"),
			TraceIndex(Continued.TraceEvents,
				FName(TEXT("turn_start_trigger_order_selected")))
				!= INDEX_NONE,
			true);

		if (Name.Contains(TEXT("ReplayPreserves")))
		{
			FWBGameStateData ReplayState = MakeState();
			FWBTurnStartSequenceState ReplaySequence;
			FWBTurnStartSequenceResult Replay =
				Begin(ReplayState, Repository, ReplaySequence);
			const FWBTurnStartSequenceResult ReplayContinued =
				WBTurnStartSequence::SubmitChoice(
					ReplayState,
					Repository,
					Selected,
					ReplaySequence);
			Test.TestEqual(TEXT("Choice replay trace stable"),
				WBReplayTrace::SerializeEvents(Continued.TraceEvents),
				WBReplayTrace::SerializeEvents(
					ReplayContinued.TraceEvents));
		}
		if (bTargetChoice)
		{
			Test.TestEqual(TEXT("Target took effect damage"),
				State.GetUnitById(
					Selected.EndsWith(TEXT("t20")) ? 20 : 10)->HP,
				5);
		}
	}
	else
	{
		Test.TestTrue(TEXT("Single surviving trigger auto resolves"),
			Result.bCompleted);
		Test.TestEqual(TEXT("Trigger draw added one card"),
			HandCount(State, 0), 1);
	}
	return true;
}

bool RunProductionCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	const FWBProductionRuntimeBootstrapResult Bootstrap =
		BootstrapProduction();
	Test.TestTrue(TEXT("Production bootstrap succeeds"),
		Bootstrap.bOk);
	if (!Bootstrap.bOk)
	{
		Test.AddError(Bootstrap.Reason);
		return false;
	}
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started =
		Coordinator.InitializeMatch(
			Bootstrap.InitializationRequest);
	Test.TestTrue(TEXT("Production match starts"), Started.bOk);
	Test.TestTrue(TEXT("First turn-start completes"),
		Coordinator.WasTurnStartCompleted());
	const FWBTurnStartSequenceState& Sequence =
		Coordinator.GetTurnStartSequenceState();
	Test.TestTrue(TEXT("First-player draw skip recorded"),
		Sequence.bDrawSkipped);
	Test.TestTrue(TEXT("MP recorded"), Sequence.bMPGenerated);
	Test.TestTrue(TEXT("Resources reset"),
		Sequence.bResourcesReset);
	Test.TestTrue(TEXT("Statuses resolved"),
		Sequence.bStatusesResolved);
	Test.TestTrue(TEXT("Effects resolved"),
		Sequence.bEffectsResolved);
	Test.TestTrue(TEXT("Decision follows completion"),
		!Started.NextLegalActions.IsEmpty());

	FWBProductionRuntimeBootstrapRequest Request;
	Request.MatchSpecificationPath =
		ProductionPath(TEXT("match_spec.json"));
	const FWBProductionStartupResult Baseline =
		WBProductionStartupResult::FromBootstrap(
			Request,
			Bootstrap);
	const FWBProductionStartupResult PublicResult =
		WBProductionStartupResult::StartedFromBootstrap(
			Baseline,
			Coordinator,
			1,
			2,
			!Started.NextLegalActions.IsEmpty());
	const FString Serialized =
		WBProductionStartupResult::Serialize(PublicResult);
	Test.TestEqual(TEXT("Production startup succeeds"),
		PublicResult.ResultCode,
		FString(TEXT("production_started")));
	Test.TestTrue(TEXT("Public turn-start field"),
		Serialized.Contains(
			TEXT("\"turn_start_completed\""))
			&& PublicResult.bTurnStartCompleted);
	Test.TestFalse(TEXT("No private hand identity"),
		Serialized.Contains(TEXT("private_"))
			|| Serialized.Contains(TEXT("instance_id")));

	if (Name.Contains(TEXT("RepeatedResultByteIdentical")))
	{
		WBMatchCoordinator ReplayCoordinator;
		const FWBMatchOperationResult ReplayStarted =
			ReplayCoordinator.InitializeMatch(
				Bootstrap.InitializationRequest);
		const FWBProductionStartupResult ReplayResult =
			WBProductionStartupResult::StartedFromBootstrap(
				Baseline,
				ReplayCoordinator,
				1,
				2,
				!ReplayStarted.NextLegalActions.IsEmpty());
		Test.TestEqual(TEXT("Repeated result byte-identical"),
			Serialized,
			WBProductionStartupResult::Serialize(
				ReplayResult));
	}
	return true;
}

bool RunAuthorityCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	FString Coordinator;
	FString Runtime;
	FString Presentation;
	FFileHelper::LoadFileToString(
		Coordinator,
		*FPaths::Combine(FPaths::ProjectDir(),
			TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp")));
	FFileHelper::LoadFileToString(
		Runtime,
		*FPaths::Combine(FPaths::ProjectDir(),
			TEXT("Source/WandboundRuntime/Private/WBProductionStartupResult.cpp")));
	FFileHelper::LoadFileToString(
		Presentation,
		*FPaths::Combine(FPaths::ProjectDir(),
			TEXT("Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp")));
	if (Name.Contains(TEXT("CoreOwnsSequence")))
	{
		Test.TestTrue(TEXT("Coordinator invokes Core sequence"),
			Coordinator.Contains(TEXT("WBTurnStartSequence::Begin")));
	}
	else if (Name.Contains(TEXT("RuntimeCannotReroll")))
	{
		Test.TestFalse(TEXT("Runtime has no RNG"),
			Runtime.Contains(TEXT("RollD6"))
				|| Runtime.Contains(TEXT("RandomState")));
	}
	else if (Name.Contains(TEXT("PresentationCannotMutate")))
	{
		Test.TestFalse(TEXT("Presentation does not run turn start"),
			Presentation.Contains(TEXT("ApplyTurnStart"))
				|| Presentation.Contains(TEXT("WBTurnStartSequence::")));
	}
	else if (Name.Contains(TEXT("NoGodotChanges")))
	{
		Test.TestFalse(TEXT("No Godot dependency"),
			Coordinator.Contains(TEXT(".gd"))
				|| Coordinator.Contains(
					TEXT("Reference/GodotProject")));
	}
	else if (Name.Contains(TEXT("NoMeshyChanges")))
	{
		Test.TestFalse(TEXT("No Meshy dependency"),
			Coordinator.Contains(TEXT("Meshy")));
	}
	else
	{
		Test.TestFalse(TEXT("No model import path"),
			Coordinator.Contains(TEXT(".fbx"))
				|| Coordinator.Contains(TEXT(".uasset")));
	}
	return true;
}

bool RunNamedCase(
	FAutomationTestBase& Test,
	const FString& Name)
{
	if (Name.Contains(TEXT(".TurnStart.Order.")))
	{
		return RunOrderingCase(Test, Name);
	}
	if (Name.Contains(TEXT(".TurnStart.Draw.")))
	{
		return RunDrawCase(Test, Name);
	}
	if (Name.Contains(TEXT(".TurnStart.MP.")))
	{
		return RunMPCase(Test, Name);
	}
	if (Name.Contains(TEXT(".TurnStart.Reset.")))
	{
		return RunResetCase(Test, Name);
	}
	if (Name.Contains(TEXT(".TurnStart.Status.")))
	{
		return RunStatusCase(Test, Name);
	}
	if (Name.Contains(TEXT(".TurnStart.Effects.")))
	{
		return RunEffectCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Production.TurnStart.")))
	{
		return RunProductionCase(Test, Name);
	}
	if (Name.Contains(TEXT(".Authority.TurnStart.")))
	{
		return RunAuthorityCase(Test, Name);
	}
	Test.AddError(TEXT("Unmapped turn-start test"));
	return false;
}

#define WB_TURN_START_TEST(ClassName, Path) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST( \
		ClassName, Path, \
		EAutomationTestFlags::EditorContext \
			| EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString&) \
	{ \
		return RunNamedCase(*this, TEXT(Path)); \
	}

WB_TURN_START_TEST(FWBTSDrawBeforeMP, "Wandbound.TurnStart.Order.DrawBeforeMP")
WB_TURN_START_TEST(FWBTsMPBeforeReset, "Wandbound.TurnStart.Order.MPBeforeResourceReset")
WB_TURN_START_TEST(FWBTsResetBeforeStatus, "Wandbound.TurnStart.Order.ResourceResetBeforeStatuses")
WB_TURN_START_TEST(FWBTsStatusBeforeEffects, "Wandbound.TurnStart.Order.StatusesBeforeEffects")
WB_TURN_START_TEST(FWBTsDeterministic, "Wandbound.TurnStart.Order.CompleteSequenceDeterministic")

WB_TURN_START_TEST(FWBTsFirstDrawSkip, "Wandbound.TurnStart.Draw.FirstPlayerTurnOneSkipped")
WB_TURN_START_TEST(FWBTsSecondDraw, "Wandbound.TurnStart.Draw.SecondPlayerFirstTurnDraws")
WB_TURN_START_TEST(FWBTsFirstTurnTwoDraw, "Wandbound.TurnStart.Draw.FirstPlayerTurnTwoDraws")
WB_TURN_START_TEST(FWBTsSkipDeck, "Wandbound.TurnStart.Draw.SkipDoesNotConsumeDeck")
WB_TURN_START_TEST(FWBTsNoFalseDraw, "Wandbound.TurnStart.Draw.NoFalseDrawTrace")
WB_TURN_START_TEST(FWBTsPrivateDraw, "Wandbound.TurnStart.Draw.PrivateIdentityProtected")
WB_TURN_START_TEST(FWBTsDistinctDraw, "Wandbound.TurnStart.Draw.DistinctFromOpeningHandDraw")
WB_TURN_START_TEST(FWBTsEmptyDeck, "Wandbound.TurnStart.Draw.EmptyDeckBehaviorPreserved")

WB_TURN_START_TEST(FWBTsOneRoll, "Wandbound.TurnStart.MP.ExactlyOneRoll")
WB_TURN_START_TEST(FWBTsAuthorityRNG, "Wandbound.TurnStart.MP.AuthoritativeRNG")
WB_TURN_START_TEST(FWBTsReplayRNG, "Wandbound.TurnStart.MP.ReplayStable")
WB_TURN_START_TEST(FWBTsNoReroll, "Wandbound.TurnStart.MP.NoRerollAfterPausedDecision")
WB_TURN_START_TEST(FWBTsMPExisting, "Wandbound.TurnStart.MP.ExistingAccumulationPreserved")

WB_TURN_START_TEST(FWBTsAttackReset, "Wandbound.TurnStart.Reset.AttacksResetAfterMP")
WB_TURN_START_TEST(FWBTsAttackLimit, "Wandbound.TurnStart.Reset.ModifiedAttackLimitPreserved")
WB_TURN_START_TEST(FWBTsInactiveReset, "Wandbound.TurnStart.Reset.InactiveUnitsIgnored")
WB_TURN_START_TEST(FWBTsWallReset, "Wandbound.TurnStart.Reset.WallPlacementRestored")
WB_TURN_START_TEST(FWBTsOpponentWall, "Wandbound.TurnStart.Reset.OpponentWallNotRestored")
WB_TURN_START_TEST(FWBTsNoDuplicateReset, "Wandbound.TurnStart.Reset.NoDuplicateResetAfterResume")

WB_TURN_START_TEST(FWBTsStatusAfterReset, "Wandbound.TurnStart.Status.TickAfterResourceReset")
WB_TURN_START_TEST(FWBTsStatusDuration, "Wandbound.TurnStart.Status.DurationDecrements")
WB_TURN_START_TEST(FWBTsStatusExpire, "Wandbound.TurnStart.Status.ExpirationCompletesBeforeEffects")
WB_TURN_START_TEST(FWBTsStatusLethal, "Wandbound.TurnStart.Status.LethalDamageRemovesUnit")
WB_TURN_START_TEST(FWBTsStatusCleanup, "Wandbound.TurnStart.Status.DeathCleanupCompletes")
WB_TURN_START_TEST(FWBTsStatusTerminal, "Wandbound.TurnStart.Status.TerminalMatchStopsSequence")

WB_TURN_START_TEST(FWBTsPostStatusCollect, "Wandbound.TurnStart.Effects.CollectedFromPostStatusState")
WB_TURN_START_TEST(FWBTsDeadSource, "Wandbound.TurnStart.Effects.DeadSourceDoesNotTrigger")
WB_TURN_START_TEST(FWBTsSurvivingSource, "Wandbound.TurnStart.Effects.SurvivingSourceTriggers")
WB_TURN_START_TEST(FWBTsNegatedSource, "Wandbound.TurnStart.Effects.NegatedSourceDoesNotTrigger")
WB_TURN_START_TEST(FWBTsMultipleTriggers, "Wandbound.TurnStart.Effects.MultipleTriggersRequireOrdering")
WB_TURN_START_TEST(FWBTsControllerOrder, "Wandbound.TurnStart.Effects.ControllerChoosesOrder")
WB_TURN_START_TEST(FWBTsStableIds, "Wandbound.TurnStart.Effects.StableActionIds")
WB_TURN_START_TEST(FWBTsReplayOrder, "Wandbound.TurnStart.Effects.ReplayPreservesOrder")
WB_TURN_START_TEST(FWBTsTargetChoice, "Wandbound.TurnStart.Effects.RequiredTargetChoiceAllowed")

WB_TURN_START_TEST(FWBTsProdComplete, "Wandbound.Production.TurnStart.FirstPlayerTurnOneCompletes")
WB_TURN_START_TEST(FWBTsProdSkip, "Wandbound.Production.TurnStart.DrawSkipRecorded")
WB_TURN_START_TEST(FWBTsProdMP, "Wandbound.Production.TurnStart.MPRecorded")
WB_TURN_START_TEST(FWBTsProdReset, "Wandbound.Production.TurnStart.ResourcesReset")
WB_TURN_START_TEST(FWBTsProdStatus, "Wandbound.Production.TurnStart.StatusesResolved")
WB_TURN_START_TEST(FWBTsProdEffects, "Wandbound.Production.TurnStart.EffectsResolved")
WB_TURN_START_TEST(FWBTsProdDecision, "Wandbound.Production.TurnStart.FirstDecisionAfterCompletion")
WB_TURN_START_TEST(FWBTsProdSafe, "Wandbound.Production.TurnStart.StartupResultPublicSafe")
WB_TURN_START_TEST(FWBTsProdRepeat, "Wandbound.Production.TurnStart.RepeatedResultByteIdentical")

WB_TURN_START_TEST(FWBTsAuthorityCore, "Wandbound.Authority.TurnStart.CoreOwnsSequence")
WB_TURN_START_TEST(FWBTsAuthorityRuntime, "Wandbound.Authority.TurnStart.RuntimeCannotReroll")
WB_TURN_START_TEST(FWBTsAuthorityPresentation, "Wandbound.Authority.TurnStart.PresentationCannotMutate")
WB_TURN_START_TEST(FWBTsAuthorityGodot, "Wandbound.Authority.TurnStart.NoGodotChanges")
WB_TURN_START_TEST(FWBTsAuthorityMeshy, "Wandbound.Authority.TurnStart.NoMeshyChanges")
WB_TURN_START_TEST(FWBTsAuthorityModels, "Wandbound.Authority.TurnStart.NoModelImports")

#undef WB_TURN_START_TEST
}

#endif
