#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBMatchCoordinator.h"
#include "WBReplayTrace.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 FirstPlayerId = 0;
constexpr int32 SecondPlayerId = 1;

FWBGenericEffectPayload MakeMatchDamagePayload(const int32 Amount)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::DamageEffect;
	Payload.DamageEffect.Amount = Amount;
	Payload.DamageEffect.bBypassArmor = true;
	Payload.DamageEffect.DamageCause = FName(TEXT("Effect"));
	Payload.DamageEffect.SourceReason = FName(TEXT("match_test"));
	return Payload;
}

FWBCardDefinition MakeMatchCharacterDefinition(
	const FString& CardId,
	const FString& PublicName,
	const bool bHasActivation = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = PublicName;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 8;
	Definition.CharacterStats.ATK = 3;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;

	if (bHasActivation)
	{
		FWBCardEffectDefinition Effect;
		Effect.EffectId = TEXT("arc_bolt");
		Effect.PublicLabel = TEXT("Arc Bolt");
		Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
		Effect.Payloads.Add(MakeMatchDamagePayload(1));
		Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
		Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
		Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
		Effect.SourceGate.bRequiresSourceUnit = true;
		Effect.SourceGate.bRequiresSourceUnitOwnership = true;
		Effect.SourceGate.bHasExplicitSourceGate = true;
		Definition.ActivatedEffects.Add(Effect);
	}

	return Definition;
}

FWBCardDefinition MakeMatchWandDefinition()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("test_wand");
	Definition.PublicName = TEXT("Training Wand");
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = 1;
	return Definition;
}

FWBCardDefinition MakeMatchFillerDefinition()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("test_filler");
	Definition.PublicName = TEXT("Practice Spell");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBCardInstanceRef MakeMatchCard(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerPlayerId;
	return Card;
}

FWBMatchPlayerSetup MakeMatchPlayerSetup(const int32 PlayerId)
{
	const FString Prefix = PlayerId == FirstPlayerId ? TEXT("p0") : TEXT("SECRET_P1");
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroInstanceId = FString::Printf(TEXT("p%d_hero"), PlayerId);
	Setup.HeroCardId = PlayerId == FirstPlayerId ? TEXT("hero_alpha") : TEXT("hero_beta");
	Setup.OrderedDeck.Add(MakeMatchCard(Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	Setup.OrderedDeck.Add(MakeMatchCard(Prefix + TEXT("_summon"), TEXT("student"), PlayerId));
	Setup.OrderedDeck.Add(MakeMatchCard(Prefix + TEXT("_wand"), TEXT("test_wand"), PlayerId));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeMatchCard(
			FString::Printf(TEXT("%s_fill_%d"), *Prefix, Index),
			TEXT("test_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeMatchRequest(const bool bReverseRepositoryOrder = false)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 424242;
	Request.FirstPlayerId = FirstPlayerId;
	Request.bDeferMarkerSetup = true;
	Request.Repository.RepositoryId = TEXT("match_test_repository");
	Request.Repository.SourceVersion = TEXT("match_test_v1");
	Request.Repository.Definitions = {
		MakeMatchCharacterDefinition(TEXT("hero_alpha"), TEXT("Aster Hero"), true),
		MakeMatchCharacterDefinition(TEXT("hero_beta"), TEXT("Briar Hero")),
		MakeMatchCharacterDefinition(TEXT("student"), TEXT("Student")),
		MakeMatchWandDefinition(),
		MakeMatchFillerDefinition()
	};
	if (bReverseRepositoryOrder)
	{
		Algo::Reverse(Request.Repository.Definitions);
	}
	Request.Players = { MakeMatchPlayerSetup(FirstPlayerId), MakeMatchPlayerSetup(SecondPlayerId) };
	return Request;
}

const FWBMatchLegalAction* FindFamilyAction(
	const FWBMatchLegalActionGenerationResult& Result,
	const EWBMatchActionFamily Family)
{
	return Result.Actions.FindByPredicate([Family](const FWBMatchLegalAction& Action)
	{
		return Action.Family == Family;
	});
}

const FWBMatchLegalAction* FindCoreAction(
	const FWBMatchLegalActionGenerationResult& Result,
	const EWBActionType Type)
{
	return Result.Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

const FWBMatchLegalAction* FindMoveTo(
	const FWBMatchLegalActionGenerationResult& Result,
	const FWBTile& Tile)
{
	return Result.Actions.FindByPredicate([Tile](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Move
			&& Action.CoreAction.ToTile == Tile;
	});
}

const FWBMatchLegalAction* FindEquipToUnit(
	const FWBMatchLegalActionGenerationResult& Result,
	const int32 UnitId)
{
	return Result.Actions.FindByPredicate([UnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Equip
			&& Action.EquipRequest.TargetUnitId == UnitId;
	});
}

const FWBMatchLegalAction* FindActivationTarget(
	const FWBMatchLegalActionGenerationResult& Result,
	const int32 UnitId)
{
	return Result.Actions.FindByPredicate([UnitId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.EffectRequest.Target.TargetUnitId == UnitId;
	});
}

const FWBMatchLegalAction* FindDiscardInstance(
	const FWBMatchLegalActionGenerationResult& Result,
	const FString& InstanceId)
{
	return Result.Actions.FindByPredicate([&InstanceId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Discard
			&& Action.DiscardCardInstanceId == InstanceId;
	});
}

bool HasTraceKind(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

int32 FindTraceIndex(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.IndexOfByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

TArray<FString> ActionIds(const FWBMatchLegalActionGenerationResult& Result)
{
	TArray<FString> Ids;
	for (const FWBMatchLegalAction& Action : Result.Actions)
	{
		Ids.Add(Action.ActionId);
	}
	return Ids;
}

FString MatchStateFingerprint(const FWBGameStateData& State)
{
	FString Result = FString::Printf(
		TEXT("turn=%d,current=%d,priority=%d,phase=%d,over=%d,winner=%d|"),
		State.TurnNumber,
		State.CurrentPlayer,
		State.PriorityPlayer,
		static_cast<int32>(State.Phase),
		State.bGameOver ? 1 : 0,
		State.WinnerPlayerId);

	TArray<FWBUnitState> Units = State.Units;
	Units.Sort([](const FWBUnitState& A, const FWBUnitState& B)
	{
		return A.UnitId < B.UnitId;
	});
	for (const FWBUnitState& Unit : Units)
	{
		Result += FString::Printf(
			TEXT("u%d:p%d:%d,%d:hp%d/%d:rl%d/%d:a%d:mp%d:d%d:r%d|"),
			Unit.UnitId,
			Unit.OwnerId,
			Unit.X,
			Unit.Y,
			Unit.HP,
			Unit.MaxHP,
			Unit.GetCurrentRLForRules(),
			Unit.RLUsed,
			Unit.AttacksLeft,
			Unit.MPRemaining,
			Unit.bDefeated ? 1 : 0,
			Unit.bRemovedFromBoard ? 1 : 0);
	}

	TArray<FWBPlayerCardZoneState> PlayerZones = State.GetCardZoneState().PlayerZones;
	PlayerZones.Sort([](const FWBPlayerCardZoneState& A, const FWBPlayerCardZoneState& B)
	{
		return A.PlayerId < B.PlayerId;
	});
	for (const FWBPlayerCardZoneState& Zones : PlayerZones)
	{
		Result += FString::Printf(TEXT("p%d:"), Zones.PlayerId);
		for (const FWBZoneCardEntry& Entry : Zones.Deck)
		{
			Result += TEXT("D") + Entry.Card.InstanceId + TEXT(",");
		}
		for (const FWBZoneCardEntry& Entry : Zones.Hand)
		{
			Result += TEXT("H") + Entry.Card.InstanceId + TEXT(",");
		}
		for (const FWBZoneCardEntry& Entry : Zones.Discard)
		{
			Result += TEXT("X") + Entry.Card.InstanceId + TEXT(",");
		}
		Result += TEXT("|");
	}
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		Result += FString::Printf(
			TEXT("E%s:u%d:%s:%d|"),
			*Entry.Card.InstanceId,
			Entry.EquippedToUnitId,
			*Entry.SlotId,
			Entry.EquipOrder);
	}
	return Result;
}

bool ForceEquipHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const int32 UnitId)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(),
		PlayerId);
	if (Zones == nullptr)
	{
		return false;
	}

	const int32 HandIndex = Zones->Hand.IndexOfByPredicate([&InstanceId](const FWBZoneCardEntry& Entry)
	{
		return Entry.Card.InstanceId == InstanceId;
	});
	if (HandIndex == INDEX_NONE)
	{
		return false;
	}

	FWBEquippedCardEntry Equipped;
	Equipped.Card = Zones->Hand[HandIndex].Card;
	Equipped.EquippedToUnitId = UnitId;
	Equipped.SlotId = TEXT("wand");
	Equipped.EquipOrder = 0;
	Zones->Hand.RemoveAt(HandIndex);
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Equipped);
	if (FWBUnitState* Unit = State.GetMutableUnitById(UnitId))
	{
		Unit->RLUsed = 1;
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorDeterministicInitializationTest,
	"Wandbound.Core.MatchCoordinator.Initialization.DeterministicSetupAndOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorDeterministicInitializationTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator First;
	WBMatchCoordinator Second;
	const FWBMatchOperationResult FirstResult = First.InitializeMatch(MakeMatchRequest());
	const FWBMatchOperationResult SecondResult = Second.InitializeMatch(MakeMatchRequest(true));

	TestTrue(TEXT("First initialization succeeds"), FirstResult.bOk);
	TestTrue(TEXT("Second initialization succeeds"), SecondResult.bOk);
	TestEqual(TEXT("State is deterministic"), MatchStateFingerprint(First.GetState()), MatchStateFingerprint(Second.GetState()));
	TestEqual(TEXT("Setup trace is deterministic"), WBReplayTrace::SerializeEvents(First.GetTraceLog()), WBReplayTrace::SerializeEvents(Second.GetTraceLog()));
	const TArray<FString> FirstActionIds = ActionIds(First.EnumerateLegalActions());
	const TArray<FString> SecondActionIds = ActionIds(Second.EnumerateLegalActions());
	TestEqual(TEXT("Legal action counts ignore repository insertion order"), FirstActionIds.Num(), SecondActionIds.Num());
	for (int32 Index = 0; Index < FMath::Min(FirstActionIds.Num(), SecondActionIds.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Legal action order %d"), Index), FirstActionIds[Index], SecondActionIds[Index]);
	}
	TestEqual(TEXT("First player explicit"), First.GetFirstPlayerId(), FirstPlayerId);
	TestEqual(TEXT("Action phase begins"), First.GetMatchPhase(), EWBMatchLoopPhase::Action);
	TestTrue(TEXT("Marker boundary explicit"), HasTraceKind(First.GetTraceLog(), FName(TEXT("setup_markers_deferred"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorSetupAndObservationTest,
	"Wandbound.Core.MatchCoordinator.Initialization.HeroesHandsAndHiddenObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorSetupAndObservationTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Result = Coordinator.InitializeMatch(MakeMatchRequest());
	const FWBGameStateData& State = Coordinator.GetState();
	const FWBPlayerCardZoneState* P0Zones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), FirstPlayerId);
	const FWBPlayerCardZoneState* P1Zones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), SecondPlayerId);
	const FWBMatchObservation Observation = Coordinator.BuildObservation(FirstPlayerId);

	TestTrue(TEXT("Initialization succeeds"), Result.bOk);
	TestNotNull(TEXT("Player zero zones"), P0Zones);
	TestNotNull(TEXT("Player one zones"), P1Zones);
	if (P0Zones == nullptr || P1Zones == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Player zero opening hand"), P0Zones->Hand.Num(), 6);
	TestEqual(TEXT("Player one opening hand"), P1Zones->Hand.Num(), 6);
	TestFalse(TEXT("Player zero Hero removed from deck"), P0Zones->Deck.ContainsByPredicate([](const FWBZoneCardEntry& Entry)
	{
		return Entry.Card.InstanceId == TEXT("p0_hero");
	}));
	TestEqual(TEXT("Player zero Hero id"), State.GetPlayerById(FirstPlayerId)->HeroUnitId, FirstPlayerId);
	TestEqual(TEXT("Player one Hero id"), State.GetPlayerById(SecondPlayerId)->HeroUnitId, SecondPlayerId);
	TestEqual(TEXT("Player zero Hero spawn"), FWBTile(State.GetUnitById(FirstPlayerId)->X, State.GetUnitById(FirstPlayerId)->Y), FWBTile(4, 8));
	TestEqual(TEXT("Player one Hero spawn"), FWBTile(State.GetUnitById(SecondPlayerId)->X, State.GetUnitById(SecondPlayerId)->Y), FWBTile(4, 0));
	TestEqual(TEXT("Viewer sees own six cards"), Observation.CardZones.OwnHand.Cards.Num(), 6);
	TestFalse(TEXT("Opponent private identities remain hidden"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
		Observation.CardZones,
		TEXT("SECRET_P1")));
	TestTrue(TEXT("Only priority player receives legal actions"), !Observation.LegalActions.IsEmpty());
	TestTrue(TEXT("Non-priority observation has no legal actions"), Coordinator.BuildObservation(SecondPlayerId).LegalActions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorMalformedSetupAtomicTest,
	"Wandbound.Core.MatchCoordinator.Initialization.MalformedSetupIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorMalformedSetupAtomicTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Baseline initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	const FString StateBefore = MatchStateFingerprint(Coordinator.GetState());
	const FString TraceBefore = WBReplayTrace::SerializeEvents(Coordinator.GetTraceLog());

	FWBMatchInitializationRequest InvalidRequest = MakeMatchRequest();
	InvalidRequest.Players[1].OrderedDeck[1].InstanceId = InvalidRequest.Players[0].OrderedDeck[1].InstanceId;
	const FWBMatchOperationResult Failure = Coordinator.InitializeMatch(InvalidRequest);

	TestFalse(TEXT("Duplicate setup fails"), Failure.bOk);
	TestEqual(TEXT("Structured setup reason"), Failure.Reason, FString(TEXT("duplicate_instance_id")));
	TestEqual(TEXT("Prior state unchanged"), MatchStateFingerprint(Coordinator.GetState()), StateBefore);
	TestEqual(TEXT("Prior trace unchanged"), WBReplayTrace::SerializeEvents(Coordinator.GetTraceLog()), TraceBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorFirstTurnRestrictionsTest,
	"Wandbound.Core.MatchCoordinator.TurnOne.FirstPlayerRestrictionsExpire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorFirstTurnRestrictionsTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	FWBUnitState* Hero0 = State.GetMutableUnitById(FirstPlayerId);
	FWBUnitState* Hero1 = State.GetMutableUnitById(SecondPlayerId);
	Hero0->X = 4;
	Hero0->Y = 4;
	Hero0->MPRemaining = 2;
	Hero0->AttacksLeft = 1;
	Hero1->X = 5;
	Hero1->Y = 4;
	State.GetMutablePlayerById(FirstPlayerId)->RemainingMP = 2;

	const FWBMatchLegalActionGenerationResult TurnOne = Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("Turn-one generation succeeds"), TurnOne.bOk);
	TestNull(TEXT("Attack is blocked on first-player turn one"), FindCoreAction(TurnOne, EWBActionType::Attack));
	TestNull(TEXT("Opposing-half move is blocked"), FindMoveTo(TurnOne, FWBTile(4, 3)));
	TestNotNull(TEXT("Own-half move remains legal"), FindMoveTo(TurnOne, FWBTile(4, 5)));

	State.TurnNumber = 2;
	const FWBMatchLegalActionGenerationResult LaterTurn = Coordinator.EnumerateLegalActions();
	TestNotNull(TEXT("Attack restriction expires"), FindCoreAction(LaterTurn, EWBActionType::Attack));
	TestNotNull(TEXT("Half-board restriction expires"), FindMoveTo(LaterTurn, FWBTile(4, 3)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorMoveAndRejectionTest,
	"Wandbound.Core.MatchCoordinator.Actions.MoveWrongPlayerAndStaleAreTransactional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorMoveAndRejectionTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Move = FindCoreAction(Legal, EWBActionType::Move);
	TestNotNull(TEXT("Move exists"), Move);
	if (Move == nullptr)
	{
		return false;
	}

	const FString BeforeWrongPlayer = MatchStateFingerprint(Coordinator.GetState());
	const FWBMatchOperationResult WrongPlayer = Coordinator.SubmitActionId(SecondPlayerId, Move->ActionId);
	TestFalse(TEXT("Wrong player rejected"), WrongPlayer.bOk);
	TestEqual(TEXT("Wrong-player reason"), WrongPlayer.Reason, FString(TEXT("wrong_player")));
	TestEqual(TEXT("Wrong-player state unchanged"), MatchStateFingerprint(Coordinator.GetState()), BeforeWrongPlayer);

	const FWBTile Destination = Move->CoreAction.ToTile;
	const FString SubmittedId = Move->ActionId;
	const FWBMatchOperationResult Applied = Coordinator.SubmitActionId(FirstPlayerId, SubmittedId);
	TestTrue(TEXT("Legal move succeeds"), Applied.bOk);
	TestEqual(TEXT("Hero moved"), FWBTile(Coordinator.GetState().GetUnitById(FirstPlayerId)->X, Coordinator.GetState().GetUnitById(FirstPlayerId)->Y), Destination);
	TestTrue(TEXT("Action submitted traced"), HasTraceKind(Applied.TraceEvents, FName(TEXT("action_submitted"))));
	TestTrue(TEXT("Automatic resolution traced"), HasTraceKind(Applied.TraceEvents, FName(TEXT("automatic_resolution"))));

	const FString BeforeStale = MatchStateFingerprint(Coordinator.GetState());
	const FString TraceBeforeStale = WBReplayTrace::SerializeEvents(Coordinator.GetTraceLog());
	const FWBMatchOperationResult Stale = Coordinator.SubmitActionId(FirstPlayerId, SubmittedId);
	TestFalse(TEXT("Stale action rejected"), Stale.bOk);
	TestEqual(TEXT("Stale reason"), Stale.Reason, FString(TEXT("stale_or_illegal_action")));
	TestEqual(TEXT("Stale state unchanged"), MatchStateFingerprint(Coordinator.GetState()), BeforeStale);
	TestEqual(TEXT("Stale trace unchanged"), WBReplayTrace::SerializeEvents(Coordinator.GetTraceLog()), TraceBeforeStale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorCardActionsTest,
	"Wandbound.Core.MatchCoordinator.Actions.SummonEquipDiscardAndRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorCardActionsTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);

	FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Summon = FindFamilyAction(Legal, EWBMatchActionFamily::Summon);
	TestNotNull(TEXT("Summon exists"), Summon);
	if (Summon == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult SummonResult = Coordinator.SubmitActionId(FirstPlayerId, Summon->ActionId);
	TestTrue(TEXT("Summon succeeds"), SummonResult.bOk);
	TestTrue(TEXT("Summon traced"), HasTraceKind(SummonResult.TraceEvents, FName(TEXT("summon_unit"))));
	TestTrue(TEXT("Legal actions refreshed after summon"), !SummonResult.NextLegalActions.IsEmpty());

	Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Equip = FindEquipToUnit(Legal, FirstPlayerId);
	TestNotNull(TEXT("Equip to Hero exists"), Equip);
	if (Equip == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult EquipResult = Coordinator.SubmitActionId(FirstPlayerId, Equip->ActionId);
	TestTrue(TEXT("Equip succeeds"), EquipResult.bOk);
	TestTrue(TEXT("Equip traced"), HasTraceKind(EquipResult.TraceEvents, FName(TEXT("equip_wand"))));
	TestEqual(TEXT("One equipped card"), Coordinator.GetState().GetCardZoneState().EquippedCards.Num(), 1);
	TestEqual(TEXT("RL used updated"), Coordinator.GetState().GetUnitById(FirstPlayerId)->RLUsed, 1);

	Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Discard = FindDiscardInstance(Legal, TEXT("p0_fill_0"));
	TestNotNull(TEXT("Discard exists"), Discard);
	if (Discard == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult DiscardResult = Coordinator.SubmitActionId(FirstPlayerId, Discard->ActionId);
	TestTrue(TEXT("Discard succeeds"), DiscardResult.bOk);
	TestTrue(TEXT("Discard traced"), HasTraceKind(DiscardResult.TraceEvents, FName(TEXT("card_discarded"))));
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(Coordinator.GetState().GetCardZoneState(), FirstPlayerId);
	TestTrue(TEXT("Discard destination contains selected card"), Zones->Discard.ContainsByPredicate([](const FWBZoneCardEntry& Entry)
	{
		return Entry.Card.InstanceId == TEXT("p0_fill_0");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorActivationDeathTest,
	"Wandbound.Core.MatchCoordinator.Actions.ActivationDeathEquipmentCleanupAndTerminalBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorActivationDeathTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	TestTrue(TEXT("Opponent wand equipped for cleanup scenario"), ForceEquipHandCard(State, SecondPlayerId, TEXT("SECRET_P1_wand"), SecondPlayerId));
	State.GetMutableUnitById(SecondPlayerId)->HP = 1;

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Activation = FindActivationTarget(Legal, SecondPlayerId);
	TestNotNull(TEXT("Targeted activation exists"), Activation);
	if (Activation == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(FirstPlayerId, Activation->ActionId);
	TestTrue(TEXT("Activation succeeds"), Result.bOk);
	TestTrue(TEXT("Match ends"), Result.bGameOver);
	TestEqual(TEXT("Winner set"), Result.WinnerPlayerId, FirstPlayerId);
	TestEqual(TEXT("Coordinator phase terminal"), Coordinator.GetMatchPhase(), EWBMatchLoopPhase::GameOver);
	TestTrue(TEXT("Hero removed"), Coordinator.GetState().GetUnitById(SecondPlayerId)->bRemovedFromBoard);
	TestTrue(TEXT("Equipment cleanup traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("equipped_card_discarded_on_death"))));
	TestTrue(TEXT("Game over traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("game_over"))));
	TestTrue(TEXT("Terminal legal set empty"), Result.NextLegalActions.IsEmpty());

	const FString TerminalState = MatchStateFingerprint(Coordinator.GetState());
	const FWBMatchOperationResult Rejected = Coordinator.SubmitActionId(FirstPlayerId, Activation->ActionId);
	TestFalse(TEXT("Post-game action rejected"), Rejected.bOk);
	TestEqual(TEXT("Post-game reason"), Rejected.Reason, FString(TEXT("game_over")));
	TestEqual(TEXT("Terminal state unchanged"), MatchStateFingerprint(Coordinator.GetState()), TerminalState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorAttackResolutionTest,
	"Wandbound.Core.MatchCoordinator.Actions.AttackAutomaticallyResolvesDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorAttackResolutionTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	State.TurnNumber = 2;
	State.GetMutableUnitById(FirstPlayerId)->X = 4;
	State.GetMutableUnitById(FirstPlayerId)->Y = 4;
	State.GetMutableUnitById(FirstPlayerId)->AttacksLeft = 1;
	State.GetMutableUnitById(SecondPlayerId)->X = 5;
	State.GetMutableUnitById(SecondPlayerId)->Y = 4;
	const int32 HPBefore = State.GetUnitById(SecondPlayerId)->HP;

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = FindCoreAction(Legal, EWBActionType::Attack);
	TestNotNull(TEXT("Attack exists"), Attack);
	if (Attack == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(FirstPlayerId, Attack->ActionId);
	TestTrue(TEXT("Attack succeeds"), Result.bOk);
	TestFalse(TEXT("Pending attack is resolved"), Coordinator.GetState().HasPendingAttack());
	TestEqual(TEXT("Damage applied"), Coordinator.GetState().GetUnitById(SecondPlayerId)->HP, HPBefore - 3);
	TestTrue(TEXT("Declaration traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("attack_declared"))));
	TestTrue(TEXT("Damage traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("attack_damage_resolved"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorOverflowResolutionTest,
	"Wandbound.Core.MatchCoordinator.AutomaticResolution.ResonanceOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorOverflowResolutionTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	TestTrue(TEXT("Wand forced equipped"), ForceEquipHandCard(State, FirstPlayerId, TEXT("p0_wand"), FirstPlayerId));
	State.GetMutableUnitById(FirstPlayerId)->SetCanonicalRL(0, 0, 1);

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Discard = FindDiscardInstance(Legal, TEXT("p0_fill_0"));
	TestNotNull(TEXT("Trigger action exists"), Discard);
	if (Discard == nullptr)
	{
		return false;
	}
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(FirstPlayerId, Discard->ActionId);
	TestTrue(TEXT("Action and overflow succeed"), Result.bOk);
	TestTrue(TEXT("Overflow begin traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("rl_overflow_begin"))));
	TestTrue(TEXT("Overflow removal traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("rl_overflow_remove_wand"))));
	TestEqual(TEXT("No equipment remains"), Coordinator.GetState().GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("RL used recalculated"), Coordinator.GetState().GetUnitById(FirstPlayerId)->RLUsed, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorAutomaticFailureAtomicTest,
	"Wandbound.Core.MatchCoordinator.AutomaticResolution.FailureRollsBackAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorAutomaticFailureAtomicTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	FWBEquippedCardEntry MissingDefinition;
	MissingDefinition.Card = MakeMatchCard(TEXT("bad_equipment"), TEXT("missing_wand_definition"), FirstPlayerId);
	MissingDefinition.EquippedToUnitId = FirstPlayerId;
	MissingDefinition.SlotId = TEXT("wand");
	MissingDefinition.EquipOrder = 0;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(MissingDefinition);

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Move = FindCoreAction(Legal, EWBActionType::Move);
	TestNotNull(TEXT("Move still generated"), Move);
	if (Move == nullptr)
	{
		return false;
	}
	const FString StateBefore = MatchStateFingerprint(Coordinator.GetState());
	const FString TraceBefore = WBReplayTrace::SerializeEvents(Coordinator.GetTraceLog());
	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(FirstPlayerId, Move->ActionId);
	TestFalse(TEXT("Automatic resolution failure rejects action"), Result.bOk);
	TestEqual(TEXT("Failure reason preserved"), Result.Reason, FString(TEXT("card_definition_not_found")));
	TestEqual(TEXT("Action state rolled back"), MatchStateFingerprint(Coordinator.GetState()), StateBefore);
	TestEqual(TEXT("Trace rolled back"), WBReplayTrace::SerializeEvents(Coordinator.GetTraceLog()), TraceBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorTurnTransitionTest,
	"Wandbound.Core.MatchCoordinator.TurnTransition.DrawResourcesAndNPCBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorTurnTransitionTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	const FWBPlayerCardZoneState* BeforeZones = WBCardZoneState::FindPlayerZones(Coordinator.GetState().GetCardZoneState(), SecondPlayerId);
	const int32 HandBefore = BeforeZones->Hand.Num();
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = FindCoreAction(Legal, EWBActionType::EndTurn);
	TestNotNull(TEXT("End turn exists"), EndTurn);
	if (EndTurn == nullptr)
	{
		return false;
	}

	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(FirstPlayerId, EndTurn->ActionId);
	const FWBGameStateData& State = Coordinator.GetState();
	const FWBPlayerCardZoneState* AfterZones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), SecondPlayerId);
	TestTrue(TEXT("End turn succeeds"), Result.bOk);
	TestEqual(TEXT("Current player advanced"), State.CurrentPlayer, SecondPlayerId);
	TestEqual(TEXT("Priority advanced"), State.PriorityPlayer, SecondPlayerId);
	TestEqual(TEXT("Turn number advanced"), State.TurnNumber, 2);
	TestEqual(TEXT("Second player first turn draws"), AfterZones->Hand.Num(), HandBefore + 1);
	TestTrue(TEXT("MP assigned in deterministic range"), State.GetPlayerById(SecondPlayerId)->RemainingMP >= 1 && State.GetPlayerById(SecondPlayerId)->RemainingMP <= 6);
	TestEqual(TEXT("Hero attacks reset"), State.GetUnitById(SecondPlayerId)->AttacksLeft, State.GetUnitById(SecondPlayerId)->MaxAttacksPerTurn);
	TestTrue(TEXT("Turn ended traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("turn_ended"))));
	TestTrue(TEXT("NPC boundary traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("npc_phase_deferred"))));
	TestTrue(TEXT("Player advanced traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("player_advanced"))));
	TestTrue(TEXT("Next turn started traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("turn_started"))));
	TestTrue(TEXT("NPC boundary precedes player advance"), FindTraceIndex(Result.TraceEvents, FName(TEXT("npc_phase_deferred"))) < FindTraceIndex(Result.TraceEvents, FName(TEXT("player_advanced"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorBurnTerminalTest,
	"Wandbound.Core.MatchCoordinator.TurnTransition.BurnDeathStopsAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorBurnTerminalTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Initialization succeeds"), Coordinator.InitializeMatch(MakeMatchRequest()).bOk);
	FWBUnitState* Hero = Coordinator.GetMutableStateForTest().GetMutableUnitById(FirstPlayerId);
	Hero->HP = 1;
	Hero->AddStatus(FName(TEXT("Burn")), 1);
	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = FindCoreAction(Legal, EWBActionType::EndTurn);
	TestNotNull(TEXT("End turn exists"), EndTurn);
	if (EndTurn == nullptr)
	{
		return false;
	}

	const FWBMatchOperationResult Result = Coordinator.SubmitActionId(FirstPlayerId, EndTurn->ActionId);
	TestTrue(TEXT("Terminal transition succeeds"), Result.bOk);
	TestTrue(TEXT("Burn ends match"), Result.bGameOver);
	TestEqual(TEXT("Opponent wins"), Result.WinnerPlayerId, SecondPlayerId);
	TestEqual(TEXT("Player does not advance"), Coordinator.GetState().CurrentPlayer, FirstPlayerId);
	TestEqual(TEXT("Turn does not advance"), Coordinator.GetState().TurnNumber, 1);
	TestFalse(TEXT("NPC phase skipped after game over"), HasTraceKind(Result.TraceEvents, FName(TEXT("npc_phase_deferred"))));
	TestFalse(TEXT("Next player skipped after game over"), HasTraceKind(Result.TraceEvents, FName(TEXT("player_advanced"))));
	TestTrue(TEXT("Game over traced"), HasTraceKind(Result.TraceEvents, FName(TEXT("game_over"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorSeveralTurnsDeterminismTest,
	"Wandbound.Core.MatchCoordinator.Headless.SeveralTurnsReplayDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorSeveralTurnsDeterminismTest::RunTest(const FString& Parameters)
{
	WBMatchCoordinator First;
	WBMatchCoordinator Second;
	TestTrue(TEXT("First initialization succeeds"), First.InitializeMatch(MakeMatchRequest()).bOk);
	TestTrue(TEXT("Second initialization succeeds"), Second.InitializeMatch(MakeMatchRequest()).bOk);

	for (int32 Step = 0; Step < 6; ++Step)
	{
		const FWBMatchLegalActionGenerationResult FirstLegal = First.EnumerateLegalActions();
		const FWBMatchLegalActionGenerationResult SecondLegal = Second.EnumerateLegalActions();
		const FWBMatchLegalAction* FirstEnd = FindCoreAction(FirstLegal, EWBActionType::EndTurn);
		const FWBMatchLegalAction* SecondEnd = FindCoreAction(SecondLegal, EWBActionType::EndTurn);
		TestNotNull(*FString::Printf(TEXT("First end turn %d"), Step), FirstEnd);
		TestNotNull(*FString::Printf(TEXT("Second end turn %d"), Step), SecondEnd);
		if (FirstEnd == nullptr || SecondEnd == nullptr)
		{
			return false;
		}
		TestEqual(*FString::Printf(TEXT("Action ids match %d"), Step), FirstEnd->ActionId, SecondEnd->ActionId);
		TestTrue(*FString::Printf(TEXT("First step succeeds %d"), Step), First.SubmitActionId(First.GetState().PriorityPlayer, FirstEnd->ActionId).bOk);
		TestTrue(*FString::Printf(TEXT("Second step succeeds %d"), Step), Second.SubmitActionId(Second.GetState().PriorityPlayer, SecondEnd->ActionId).bOk);
	}

	TestEqual(TEXT("Several-turn state deterministic"), MatchStateFingerprint(First.GetState()), MatchStateFingerprint(Second.GetState()));
	TestEqual(TEXT("Several-turn trace deterministic"), WBReplayTrace::SerializeEvents(First.GetTraceLog()), WBReplayTrace::SerializeEvents(Second.GetTraceLog()));
	TestEqual(TEXT("Six transitions advance to turn seven"), First.GetState().TurnNumber, 7);
	TestEqual(TEXT("Current player cycles deterministically"), First.GetState().CurrentPlayer, FirstPlayerId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMatchCoordinatorHeadlessSourceGuardTest,
	"Wandbound.Core.MatchCoordinator.Headless.CoreOnlyAndSeededRNG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMatchCoordinatorHeadlessSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Header;
	FString Source;
	TestTrue(TEXT("Header loads"), FFileHelper::LoadFileToString(
		Header,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Public/WBMatchCoordinator.h"))));
	TestTrue(TEXT("Source loads"), FFileHelper::LoadFileToString(
		Source,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp"))));
	const FString Combined = Header + Source;

	TestFalse(TEXT("No runtime module dependency"), Combined.Contains(TEXT("WandboundRuntime")));
	TestFalse(TEXT("No world dependency"), Combined.Contains(TEXT("UWorld")));
	TestFalse(TEXT("No Actor dependency"), Combined.Contains(TEXT("AActor")));
	TestFalse(TEXT("No Component dependency"), Combined.Contains(TEXT("UActorComponent")));
	TestFalse(TEXT("No global random API"), Combined.Contains(TEXT("FMath::Rand")));
	TestTrue(TEXT("Explicit deterministic RNG state"), Combined.Contains(TEXT("WorkingRandomState")));
	return true;
}

#endif
