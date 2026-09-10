#include "Misc/AutomationTest.h"
#include "WBBattlePhase.h"
#include "WBMatchCoordinator.h"
#include "WBProductionMatchReplay.h"
#include "WBCardZoneTransitionTrigger.h"
#include "WBCardLifecycle.h"
#include "WBActionCodec.h"
#include "WBEffectRunner.h"
#include "WBProductionCardDatabase.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FWBCardEffectDefinition BattleEffect(const FString& Id, EWBGenericEffectOp Op,
	EWBCardActivationSourceZone Zone = EWBCardActivationSourceZone::Board)
{
	FWBCardEffectDefinition E;
	E.EffectId = Id;
	E.PublicLabel = TEXT("Respond");
	E.SourceGate.RequiredZone = Zone;
	E.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	E.SourceGate.bHasExplicitSourceGate = true;
	E.SourceGate.bRequiresSourceUnit = Zone == EWBCardActivationSourceZone::Board;
	E.SourceGate.bRequiresSourceUnitOwnership = true;
	E.SourceGate.bRequiresFixtureZoneOwnership = true;
	E.SourceGate.bOncePerTurn = true;
	E.SourceGate.OncePerTurnKey = Id;
	E.ActivationCondition.BattleRequirement = EWBCardEffectBattleRequirement::DuringBattle;
	FWBGenericEffectPayload P;
	P.Operation = Op;
	E.Payloads.Add(P);
	return E;
}
FWBMatchInitializationRequest BattleRequest(bool bResponses = true, int32 CounterAR = 0,
	int32 AttackerHP = 8, int32 DefenderHP = 8)
{
	FWBMatchInitializationRequest R;
	R.Seed = 99173;
	R.FirstPlayerId = 0;
	R.Repository.RepositoryId = TEXT("battle_fixture");
	R.Repository.SourceVersion = TEXT("battle_v1");
	for (int32 Player = 0; Player < 2; ++Player)
	{
		FWBCardDefinition D;
		D.CardId = FString::Printf(TEXT("battle_hero_%d"), Player);
		D.PublicName = TEXT("Battle Hero");
		D.Kind = EWBCardDefinitionKind::Character;
		D.CharacterStats.HP = Player == 0 ? AttackerHP : DefenderHP;
		D.CharacterStats.ATK = Player == 0 ? 3 : 1;
		D.CharacterStats.AR = Player == 0 ? 8 : CounterAR;
		D.CharacterStats.RL = 3;
		if (bResponses) D.ActivatedEffects.Add(BattleEffect(Player == 0 ? TEXT("battle_b") : TEXT("battle_a"),
			Player == 0 ? EWBGenericEffectOp::NegatePendingEffect : EWBGenericEffectOp::PreventPendingAttack));
		R.Repository.Definitions.Add(D);
		FWBMatchPlayerSetup P;
		P.PlayerId = Player;
		P.HeroCardId = D.CardId;
		P.HeroInstanceId = FString::Printf(TEXT("battle_hero_instance_%d"), Player);
		for (int32 I = 0; I < 16; ++I)
		{
			FWBCardInstanceRef C;
			C.OwnerPlayerId = Player;
			C.InstanceId = I == 0 ? P.HeroInstanceId : FString::Printf(TEXT("battle_p%d_card_%d"), Player, I);
			C.CardId = I == 0 ? D.CardId : (Player == 1 && I == 1 ? TEXT("battle_c_card") : TEXT("battle_filler"));
			P.OrderedDeck.Add(C);
		}
		R.Players.Add(P);
	}
	FWBCardDefinition Filler;
	Filler.CardId = TEXT("battle_filler"); Filler.PublicName = TEXT("Filler"); Filler.Kind = EWBCardDefinitionKind::Action;
	R.Repository.Definitions.Add(Filler);
	Filler.CardId = TEXT("battle_c_card"); Filler.PublicName = TEXT("Response");
	Filler.ActivatedEffects.Add(BattleEffect(TEXT("battle_c"), EWBGenericEffectOp::NegatePendingEffect, EWBCardActivationSourceZone::Hand));
	R.Repository.Definitions.Add(Filler);
	for (int32 Player = 0; Player < 2; ++Player)
	{
		for (int32 I = 0; I < 4; ++I)
		{
			FWBSetupMarkerPlacement M;
			M.PlayerId = Player;
			M.Type = I < 2 ? EWBMarkerType::Trap : EWBMarkerType::NPC;
			M.Tile = FWBTile(I % 2, Player == 0 ? (I < 2 ? 8 : 7) : (I < 2 ? 0 : 1));
			M.DefinitionId = I < 2 ? TEXT("battle_trap") : TEXT("battle_npc");
			M.PlacementOrder = Player * 4 + I;
			R.MarkerPlacements.Add(M);
		}
	}
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("battle_trap"); Trap.PublicName = TEXT("Trap"); Trap.Kind = EWBCardDefinitionKind::Trap; Trap.TrapDamage = 1;
	R.Repository.Definitions.Add(Trap);
	Trap.CardId = TEXT("battle_npc"); Trap.PublicName = TEXT("NPC"); Trap.Kind = EWBCardDefinitionKind::NPC;
	Trap.CharacterStats.HP = 2; Trap.CharacterStats.ATK = 1; Trap.CharacterStats.AR = 1;
	R.Repository.Definitions.Add(Trap);
	return R;
}
FWBMatchOperationResult SubmitCore(WBMatchCoordinator& C, EWBActionType Type)
{
	const auto Legal = C.EnumerateLegalActions();
	for (const auto& A : Legal.Actions)
		if (A.Family == EWBMatchActionFamily::CoreAction && A.CoreAction.Type == Type)
			return C.SubmitActionId(A.PlayerId, A.ActionId);
	FWBMatchOperationResult R; R.Reason = TEXT("fixture_core_action_missing"); return R;
}
bool SetupBattle(WBMatchCoordinator& C, const FWBMatchInitializationRequest& R)
{
	return C.InitializeMatch(R).bOk && SubmitCore(C, EWBActionType::EndTurn).bOk
		&& SubmitCore(C, EWBActionType::EndTurn).bOk;
}
bool ReactBattle(WBMatchCoordinator& C, const FString& Id)
{
	const auto Legal = C.EnumerateLegalActions();
	for (const auto& A : Legal.Actions)
		if (A.Family == EWBMatchActionFamily::Activation && A.ActivationCommand.Source.SourceEffectId == Id)
			return C.SubmitActionId(A.PlayerId, A.ActionId).bOk;
	return false;
}
bool DrainBattle(WBMatchCoordinator& C)
{
	for (int32 I = 0; I < 32 && C.GetState().IsResponsePhase(); ++I)
		if (!SubmitCore(C, EWBActionType::PassResponse).bOk) return false;
	return !C.GetState().IsResponsePhase();
}
int32 BoundaryCount(const WBMatchCoordinator& C, const TCHAR* Kind)
{
	return C.GetTraceLog().FilterByPredicate([Kind](const FWBTraceEvent& E){return E.Kind == FName(Kind);}).Num();
}
bool ReplayBattle(const FWBMatchInitializationRequest& R, const WBMatchCoordinator& Original)
{
	WBMatchCoordinator Fresh;
	if (!Fresh.InitializeMatch(R).bOk) return false;
	for (const auto& Record : Original.GetCommittedActionRecords())
	{
		if (Fresh.GetCurrentStateDigest() != Record.BeforeStateDigest
			|| !Fresh.SubmitActionId(Record.ActingPlayer, Record.ChosenActionId).bOk
			|| Fresh.GetCurrentStateDigest() != Record.AfterStateDigest) return false;
	}
	return Fresh.GetCurrentStateDigest() == Original.GetCurrentStateDigest()
		&& Fresh.GetCurrentTraceDigest() == Original.GetCurrentTraceDigest()
		&& Fresh.GetState().BattlePhase.BattleId == Original.GetState().BattlePhase.BattleId;
}
}
#define WB_BATTLE_TEST(C, N) IMPLEMENT_SIMPLE_AUTOMATION_TEST(C, "Wandbound.BattlePhase." N, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_BATTLE_TEST(FWBBattleEntry, "Entry.AcceptedDeclaration")
bool FWBBattleEntry::RunTest(const FString&)
{
	WBMatchCoordinator C;
	const auto R = BattleRequest();
	TestTrue(TEXT("Production setup and two turns"), SetupBattle(C, R));
	TestFalse(TEXT("Setup never starts Battle"), C.GetState().IsBattlePhaseActive());
	TestEqual(TEXT("No setup boundary"), BoundaryCount(C, TEXT("battle_phase_started")), 0);
	const auto Attack = SubmitCore(C, EWBActionType::Attack);
	TestTrue(*Attack.Reason, Attack.bOk);
	const auto& S = C.GetState();
	TestTrue(TEXT("Active before PreHit choice"), S.IsBattlePhaseActive());
	TestTrue(TEXT("Pending continuation"), S.HasPendingAttack());
	TestTrue(TEXT("PreHit stage"), S.PendingAttack.Stage == EWBAttackContinuationStage::PreHit);
	TestTrue(TEXT("Response phase unchanged"), C.GetMatchPhase() == EWBMatchLoopPhase::Response);
	TestEqual(TEXT("Opponent first"), S.PriorityPlayer, 1);
	TestEqual(TEXT("Turn owner unchanged"), S.CurrentPlayer, 0);
	TestEqual(TEXT("Declaring player"), S.BattlePhase.DeclaringPlayerId, 0);
	TestEqual(TEXT("Root attacker"), S.BattlePhase.OriginalAttackerUnitId, S.GetPlayerById(0)->HeroUnitId);
	TestEqual(TEXT("Root defender"), S.BattlePhase.OriginalDefenderUnitId, S.GetPlayerById(1)->HeroUnitId);
	TestEqual(TEXT("Root turn"), S.BattlePhase.TurnNumber, S.TurnNumber);
	TestEqual(TEXT("Root action"), S.BattlePhase.RootActionId, S.PendingAttack.DeclarationActionId);
	TestEqual(TEXT("Root continuation"), S.BattlePhase.RootContinuationId, S.PendingAttack.ContinuationId);
	TestFalse(TEXT("Battle identity exists"), S.BattlePhase.BattleId.IsEmpty());
	TestEqual(TEXT("One start"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
	TestEqual(TEXT("Not ended early"), BoundaryCount(C, TEXT("battle_phase_ended")), 0);
	FString Reason;
	TestTrue(TEXT("Valid state"), WBBattlePhase::Validate(S, Reason));
	TestTrue(TEXT("Fresh partial replay"), ReplayBattle(R, C));
	return true;
}

WB_BATTLE_TEST(FWBBattleNested, "Reaction.NestedABC")
bool FWBBattleNested::RunTest(const FString&)
{
	WBMatchCoordinator C;
	const auto R = BattleRequest();
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	TestTrue(TEXT("Attack"), SubmitCore(C, EWBActionType::Attack).bOk);
	const FString Id = C.GetState().BattlePhase.BattleId;
	const FString Root = C.GetState().BattlePhase.RootActionId;
	const int32 HP = C.GetState().GetUnitById(C.GetState().BattlePhase.OriginalDefenderUnitId)->HP;
	TestTrue(TEXT("React A accepted"), ReactBattle(C, TEXT("battle_a")));
	TestEqual(TEXT("A same Battle"), C.GetState().BattlePhase.BattleId, Id);
	TestEqual(TEXT("A stack"), C.GetPendingEffectActivationStack().Num(), 1);
	TestTrue(TEXT("React B accepted"), ReactBattle(C, TEXT("battle_b")));
	TestEqual(TEXT("B same Battle"), C.GetState().BattlePhase.BattleId, Id);
	TestEqual(TEXT("B stack"), C.GetPendingEffectActivationStack().Num(), 2);
	TestTrue(TEXT("React C accepted"), ReactBattle(C, TEXT("battle_c")));
	if (C.GetState().IsBattlePhaseActive())
	{
		TestEqual(TEXT("C same Battle while suspended"), C.GetState().BattlePhase.BattleId, Id);
		TestEqual(TEXT("Root never replaced"), C.GetState().BattlePhase.RootActionId, Root);
	}
	else
	{
		TestEqual(TEXT("Automatic passes completed C/B/A"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
	}
	TestEqual(TEXT("No nested start"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
	TestTrue(TEXT("Suspended replay"), ReplayBattle(R, C));
	TestTrue(TEXT("LIFO resolves"), DrainBattle(C));
	TestFalse(TEXT("Battle closed"), C.GetState().IsBattlePhaseActive());
	TestFalse(TEXT("Pending closed"), C.GetState().HasPendingAttack());
	TestEqual(TEXT("Stack empty"), C.GetPendingEffectActivationStack().Num(), 0);
	TestEqual(TEXT("One end"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
	TestEqual(TEXT("No damage"), C.GetState().GetUnitById(C.GetState().GetPlayerById(1)->HeroUnitId)->HP, HP);
	TestEqual(TEXT("No counter"), BoundaryCount(C, TEXT("counter_started")), 0);
	TestTrue(TEXT("Action resumes"), C.GetMatchPhase() == EWBMatchLoopPhase::Action);
	TestTrue(TEXT("Fresh completed replay"), ReplayBattle(R, C));
	return true;
}

WB_BATTLE_TEST(FWBBattleNoCounter, "Completion.NormalAndReplay")
bool FWBBattleNoCounter::RunTest(const FString&)
{
	WBMatchCoordinator C;
	const auto R = BattleRequest(false);
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	const int32 Records = C.GetCommittedActionRecords().Num();
	TestTrue(TEXT("Attack"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestFalse(TEXT("Battle ends"), C.GetState().IsBattlePhaseActive());
	TestFalse(TEXT("Pending ends"), C.GetState().HasPendingAttack());
	TestTrue(TEXT("Action resumes"), C.GetMatchPhase() == EWBMatchLoopPhase::Action);
	TestEqual(TEXT("Start"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
	TestEqual(TEXT("End"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
	TestEqual(TEXT("No counter"), BoundaryCount(C, TEXT("counter_started")), 0);
	TestEqual(TEXT("One user decision"), C.GetCommittedActionRecords().Num(), Records + 1);
	TestEqual(TEXT("Damage once"), C.GetState().GetUnitById(C.GetState().GetPlayerById(1)->HeroUnitId)->HP, 5);
	TestEqual(TEXT("Priority restored"), C.GetState().PriorityPlayer, C.GetState().CurrentPlayer);
	TestTrue(TEXT("Replay"), ReplayBattle(R, C));
	TestTrue(TEXT("Normal EndTurn available"), SubmitCore(C, EWBActionType::EndTurn).bOk);
	TestFalse(TEXT("Turn cannot inherit Battle"), C.GetState().IsBattlePhaseActive());
	return true;
}

WB_BATTLE_TEST(FWBBattleCounter, "Counter.OneEnvelope")
bool FWBBattleCounter::RunTest(const FString&)
{
	WBMatchCoordinator C;
	const auto R = BattleRequest(false, 8);
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	const int32 Records = C.GetCommittedActionRecords().Num();
	TestTrue(TEXT("Attack"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestEqual(TEXT("One counter"), BoundaryCount(C, TEXT("counter_started")), 1);
	TestEqual(TEXT("One start"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
	TestEqual(TEXT("One end"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
	TestEqual(TEXT("Counter not accepted user decision"), C.GetCommittedActionRecords().Num(), Records + 1);
	TestEqual(TEXT("Attacker damaged once"), C.GetState().GetUnitById(C.GetState().GetPlayerById(0)->HeroUnitId)->HP, 7);
	TestEqual(TEXT("Defender damaged once"), C.GetState().GetUnitById(C.GetState().GetPlayerById(1)->HeroUnitId)->HP, 5);
	TestFalse(TEXT("No stale Battle"), C.GetState().IsBattlePhaseActive());
	TestFalse(TEXT("No pending counter"), C.GetState().HasPendingAttack());
	TestTrue(TEXT("Replay counter"), ReplayBattle(R, C));
	TestTrue(TEXT("Action returned"), C.GetMatchPhase() == EWBMatchLoopPhase::Action);
	return true;
}

WB_BATTLE_TEST(FWBBattleTerminal, "Terminal.OriginalAndCounter")
bool FWBBattleTerminal::RunTest(const FString&)
{
	for (bool bCounter : {false, true})
	{
		WBMatchCoordinator C;
		const auto R = BattleRequest(false, bCounter ? 8 : 0, bCounter ? 1 : 8, bCounter ? 8 : 2);
		TestTrue(TEXT("Setup"), SetupBattle(C, R));
		TestTrue(TEXT("Lethal attack"), SubmitCore(C, EWBActionType::Attack).bOk);
		TestTrue(TEXT("Terminal"), C.GetState().bGameOver);
		TestTrue(TEXT("GameOver not Action"), C.GetMatchPhase() == EWBMatchLoopPhase::GameOver);
		TestFalse(TEXT("Battle cleared"), C.GetState().IsBattlePhaseActive());
		TestFalse(TEXT("Pending cleared"), C.GetState().HasPendingAttack());
		TestFalse(TEXT("Response cleared"), C.GetState().HasOpenReactionWindow());
		TestEqual(TEXT("Effects cleared"), C.GetPendingEffectActivationStack().Num(), 0);
		TestEqual(TEXT("Single start"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
		TestEqual(TEXT("Single end"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
		TestEqual(TEXT("No actions"), C.EnumerateLegalActions().Actions.Num(), 0);
		TestTrue(TEXT("Terminal replay"), ReplayBattle(R, C));
		int32 EndIndex = INDEX_NONE, TerminalIndex = INDEX_NONE;
		for (int32 I = 0; I < C.GetTraceLog().Num(); ++I)
		{
			if (C.GetTraceLog()[I].Kind == FName(TEXT("battle_phase_ended"))) EndIndex = I;
			if (C.GetTraceLog()[I].Kind == FName(TEXT("terminal_state_committed"))) TerminalIndex = I;
		}
		TestTrue(TEXT("End before terminal commit"), EndIndex >= 0 && TerminalIndex > EndIndex);
	}
	return true;
}

WB_BATTLE_TEST(FWBBattlePrivacy, "PublicSummaryAndDigest")
bool FWBBattlePrivacy::RunTest(const FString&)
{
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest()));
	TestFalse(TEXT("Closed public summary"), C.BuildObservation(0).PublicBattle.bActive);
	TestTrue(TEXT("Declare"), SubmitCore(C, EWBActionType::Attack).bOk);
	const auto A = C.BuildObservation(0).PublicBattle;
	const auto B = C.BuildObservation(1).PublicBattle;
	TestTrue(TEXT("Player zero sees Battle"), A.bActive);
	TestTrue(TEXT("Player one sees Battle"), B.bActive);
	TestEqual(TEXT("Same player"), A.DeclaringPlayerId, B.DeclaringPlayerId);
	TestEqual(TEXT("Same attacker"), A.OriginalAttackerUnitId, B.OriginalAttackerUnitId);
	TestEqual(TEXT("Same defender"), A.OriginalDefenderUnitId, B.OriginalDefenderUnitId);
	const FString Json = WBReplayTrace::SerializeEvents(C.GetTraceLog());
	TestFalse(TEXT("Battle ID never public"), Json.Contains(C.GetState().BattlePhase.BattleId));
	FWBGameStateData Copy = C.GetState();
	const FString Before = WBProductionMatchReplay::BuildGameStateDigest(Copy);
	Copy.ClearBattlePhase();
	TestNotEqual(TEXT("Active envelope in protected digest"), Before, WBProductionMatchReplay::BuildGameStateDigest(Copy));
	TestFalse(TEXT("Copy reset"), Copy.IsBattlePhaseActive());
	TestTrue(TEXT("Original preserved"), C.GetState().IsBattlePhaseActive());
	TestTrue(TEXT("Drain"), DrainBattle(C));
	TestFalse(TEXT("Summary closes"), C.BuildObservation(1).PublicBattle.bActive);
	TestEqual(TEXT("Closed participant defaults"), C.BuildObservation(1).PublicBattle.OriginalAttackerUnitId, INDEX_NONE);
	TestEqual(TEXT("Schema unchanged"), WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_BATTLE_TEST(FWBBattleConditions, "Conditions.NoTimingGrant")
bool FWBBattleConditions::RunTest(const FString&)
{
	FWBCardEffectActivationCondition Condition;
	TestTrue(TEXT("Any outside"), Condition.MatchesBattle(false));
	TestTrue(TEXT("Any inside"), Condition.MatchesBattle(true));
	TestTrue(TEXT("Parse during"), Condition.ReadBattleRequirement(TEXT("during_battle")));
	TestFalse(TEXT("During rejects outside"), Condition.MatchesBattle(false));
	TestTrue(TEXT("During accepts inside"), Condition.MatchesBattle(true));
	TestTrue(TEXT("Parse outside"), Condition.ReadBattleRequirement(TEXT("outside_battle")));
	TestTrue(TEXT("Outside accepts outside"), Condition.MatchesBattle(false));
	TestFalse(TEXT("Outside rejects inside"), Condition.MatchesBattle(true));
	TestFalse(TEXT("Unknown rejected"), Condition.ReadBattleRequirement(TEXT("Battle")));
	TestTrue(TEXT("Parse any"), Condition.ReadBattleRequirement(TEXT("any")));
	TestTrue(TEXT("Any restored"), Condition.MatchesBattle(true));
	Condition.BattleRequirement = static_cast<EWBCardEffectBattleRequirement>(255);
	TestFalse(TEXT("Invalid rejects inside"), Condition.MatchesBattle(true));
	TestFalse(TEXT("Invalid rejects outside"), Condition.MatchesBattle(false));
	WBMatchCoordinator C;
	auto R = BattleRequest();
	auto& E = R.Repository.Definitions[0].ActivatedEffects[0];
	E.SourceGate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	TestFalse(TEXT("DuringBattle never grants normal activation"), ReactBattle(C, TEXT("battle_b")));
	TestTrue(TEXT("Attack accepted"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestFalse(TEXT("Normal timing not granted inside Battle"), ReactBattle(C, TEXT("battle_b")));
	TestTrue(TEXT("Existing response timing legal"), ReactBattle(C, TEXT("battle_a")));
	TestTrue(TEXT("Close"), DrainBattle(C));
	TestFalse(TEXT("No latent normal timing"), ReactBattle(C, TEXT("battle_b")));
	return true;
}

WB_BATTLE_TEST(FWBBattleInjection, "Actions.DirectInjectionRollback")
bool FWBBattleInjection::RunTest(const FString&)
{
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest()));
	const auto BeforeActions = C.EnumerateLegalActions().Actions;
	TestTrue(TEXT("Attack opens"), SubmitCore(C, EWBActionType::Attack).bOk);
	const FString StateDigest = C.GetCurrentStateDigest();
	const FString TraceDigest = C.GetCurrentTraceDigest();
	const FString Id = C.GetState().BattlePhase.BattleId;
	const int32 Records = C.GetCommittedActionRecords().Num();
	for (const auto& A : BeforeActions)
	{
		TestFalse(TEXT("Every stale normal action rejected"), C.SubmitActionId(A.PlayerId, A.ActionId).bOk);
		TestEqual(TEXT("Rejected state unchanged"), C.GetCurrentStateDigest(), StateDigest);
		TestEqual(TEXT("Rejected traces unchanged"), C.GetCurrentTraceDigest(), TraceDigest);
		TestEqual(TEXT("Rejected Battle unchanged"), C.GetState().BattlePhase.BattleId, Id);
		TestEqual(TEXT("Rejected never recorded"), C.GetCommittedActionRecords().Num(), Records);
	}
	for (const TCHAR* IdText : {TEXT("move:p1:u1:0,0:1,0"), TEXT("attack:p1:u1:t0"),
		TEXT("summon:p1:injected"), TEXT("equip:p1:injected"), TEXT("discard:p1:injected"),
		TEXT("end_turn:p1"), TEXT("activate:p1:injected"), TEXT("hybrid:p1:injected")})
	{
		TestFalse(TEXT("Injected normal action rejected at response priority"), C.SubmitActionId(1, IdText).bOk);
		TestEqual(TEXT("Injected action no state mutation"), C.GetCurrentStateDigest(), StateDigest);
		TestEqual(TEXT("Injected action no record"), C.GetCommittedActionRecords().Num(), Records);
	}
	const auto Legal = C.EnumerateLegalActions();
	TestTrue(TEXT("Response generation valid"), Legal.bOk);
	TestTrue(TEXT("Response choices nonempty"), !Legal.Actions.IsEmpty());
	for (const auto& A : Legal.Actions)
		TestTrue(TEXT("Only response families"), A.Family == EWBMatchActionFamily::Activation
			|| (A.Family == EWBMatchActionFamily::CoreAction && A.CoreAction.Type == EWBActionType::PassResponse));
	TestFalse(TEXT("EndTurn blocked"), SubmitCore(C, EWBActionType::EndTurn).bOk);
	TestFalse(TEXT("Move blocked"), SubmitCore(C, EWBActionType::Move).bOk);
	TestFalse(TEXT("Attack blocked"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestEqual(TEXT("Still same Battle"), C.GetState().BattlePhase.BattleId, Id);
	return true;
}

WB_BATTLE_TEST(FWBBattleRejected, "Entry.RejectedAttacks")
bool FWBBattleRejected::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 6; ++Case)
	{
		WBMatchCoordinator C;
		TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest()));
		const auto Legal = C.EnumerateLegalActions();
		const auto* Attack = Legal.Actions.FindByPredicate([](const FWBMatchLegalAction& A)
			{return A.Family == EWBMatchActionFamily::CoreAction && A.CoreAction.Type == EWBActionType::Attack;});
		TestNotNull(TEXT("Initially legal attack"), Attack);
		if (!Attack) return false;
		auto& S = C.GetMutableStateForTest();
		auto* U = S.GetMutableUnitById(Attack->CoreAction.SourceUnitId);
		if (Case == 0) U->AR = 0;
		if (Case == 1)
		{
			const auto* Target = S.GetUnitById(Attack->CoreAction.TargetUnitId);
			S.AddWallForTest(FWBWallEdge(FWBTile(U->X, U->Y),
				FWBTile(U->X, U->Y + (Target->Y > U->Y ? 1 : -1))));
		}
		if (Case == 2) U->AttacksLeft = 0;
		if (Case == 3) U->AddStatus(FName(TEXT("Stunned")), 1);
		if (Case == 4) U->AddStatus(FName(TEXT("CannotAttack")), 1);
		const FString Before = C.GetCurrentStateDigest();
		const int32 Records = C.GetCommittedActionRecords().Num();
		TestFalse(*FString::Printf(TEXT("Illegal declaration rejected case %d"), Case), C.SubmitActionId(Attack->PlayerId,
			Case == 5 ? Attack->ActionId + TEXT(":stale") : Attack->ActionId).bOk);
		TestFalse(TEXT("No Battle created"), S.IsBattlePhaseActive());
		TestFalse(TEXT("No continuation created"), S.HasPendingAttack());
		TestEqual(TEXT("No start trace"), BoundaryCount(C, TEXT("battle_phase_started")), 0);
		TestEqual(TEXT("State unchanged"), C.GetCurrentStateDigest(), Before);
		TestEqual(TEXT("No accepted action"), C.GetCommittedActionRecords().Num(), Records);
	}
	return true;
}

WB_BATTLE_TEST(FWBBattleFrozenRemoval, "Cancellation.FrozenAndRemoval")
bool FWBBattleFrozenRemoval::RunTest(const FString&)
{
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup Frozen"), SetupBattle(C, BattleRequest(false, 8)));
	const int32 Defender = C.GetState().GetPlayerById(1)->HeroUnitId;
	C.GetMutableStateForTest().GetMutableUnitById(Defender)->AddStatus(FName(TEXT("Frozen")), 1);
	TestTrue(TEXT("Frozen attack accepted"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestEqual(TEXT("No HP loss"), C.GetState().GetUnitById(Defender)->HP, 8);
	TestFalse(TEXT("Frozen removed"), C.GetState().GetUnitById(Defender)->HasStatus(FName(TEXT("Frozen"))));
	TestEqual(TEXT("Counter suppressed"), BoundaryCount(C, TEXT("counter_started")), 0);
	TestEqual(TEXT("One Frozen Battle"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
	TestEqual(TEXT("Frozen Battle closes"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
	for (int32 Player : {0, 1})
	{
		WBMatchCoordinator Removed;
		TestTrue(TEXT("Setup removal"), SetupBattle(Removed, BattleRequest()));
		TestTrue(TEXT("Open before removal"), SubmitCore(Removed, EWBActionType::Attack).bOk);
		Removed.GetMutableStateForTest().GetMutableUnitById(Removed.GetState().GetPlayerById(Player)->HeroUnitId)->RemoveUnitFromBoard();
		TestTrue(TEXT("Cancellation processed"), DrainBattle(Removed));
		TestFalse(TEXT("Cancellation closes Battle"), Removed.GetState().IsBattlePhaseActive());
		TestFalse(TEXT("Cancellation closes pending"), Removed.GetState().HasPendingAttack());
		TestEqual(TEXT("One cancellation"), BoundaryCount(Removed, TEXT("attack_continuation_cancelled")), 1);
		TestEqual(TEXT("One end"), BoundaryCount(Removed, TEXT("battle_phase_ended")), 1);
		TestTrue(TEXT("Action restored"), Removed.GetMatchPhase() == EWBMatchLoopPhase::Action);
	}
	return true;
}

WB_BATTLE_TEST(FWBBattleInvariants, "State.MalformedFailsClosed")
bool FWBBattleInvariants::RunTest(const FString&)
{
	FWBGameStateData Empty;
	FString Reason;
	TestTrue(TEXT("Default valid"), WBBattlePhase::Validate(Empty, Reason));
	TestFalse(TEXT("Default inactive"), Empty.IsBattlePhaseActive());
	Empty.BattlePhase.bActive = true;
	TestFalse(TEXT("Orphan rejected"), WBBattlePhase::Validate(Empty, Reason));
	Empty.ClearBattlePhase();
	TestTrue(TEXT("Reset valid"), WBBattlePhase::Validate(Empty, Reason));
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest()));
	TestTrue(TEXT("Open"), SubmitCore(C, EWBActionType::Attack).bOk);
	const auto Good = C.GetState();
	for (int32 Case = 0; Case < 8; ++Case)
	{
		auto Bad = Good;
		if (Case == 0) Bad.ClearBattlePhase();
		if (Case == 1) Bad.ClearPendingAttack();
		if (Case == 2) Bad.bGameOver = true;
		if (Case == 3) ++Bad.TurnNumber;
		if (Case == 4) Bad.BattlePhase.BattleId.Reset();
		if (Case == 5) Bad.BattlePhase.DeclaringPlayerId = -1;
		if (Case == 6) Bad.PendingAttack.AuthorityKind = EWBAttackAuthorityKind::NeutralNPC;
		if (Case == 7) Bad.BattlePhase.RootActionId = TEXT("wrong");
		TestFalse(TEXT("Invalid authority rejected"), WBBattlePhase::Validate(Bad, Reason));
		TestFalse(TEXT("Named diagnostic"), Reason.IsEmpty());
		C.GetMutableStateForTest() = Bad;
		TestFalse(TEXT("Malformed coordinator fails generation"), C.EnumerateLegalActions().bOk);
	}
	C.GetMutableStateForTest() = Good;
	TestTrue(TEXT("Valid state restored"), WBBattlePhase::Validate(C.GetState(), Reason));
	TestFalse(TEXT("Legacy turn rejected"), WBMatchCoordinator::ApplyLegacyCompatibilityTurnTransition(
		C.GetMutableStateForTest(), 0, 3).bOk);
	TestEqual(TEXT("Turn unchanged"), C.GetState().TurnNumber, Good.TurnNumber);
	return true;
}


WB_BATTLE_TEST(FWBBattleMultiple, "Action.MultipleBattles")
bool FWBBattleMultiple::RunTest(const FString&)
{
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest()));
	const int32 Attacker = C.GetState().GetPlayerById(0)->HeroUnitId;
	C.GetMutableStateForTest().GetMutableUnitById(Attacker)->AttacksLeft = 2;
	TestTrue(TEXT("First declaration"), SubmitCore(C, EWBActionType::Attack).bOk);
	const FString First = C.GetState().BattlePhase.BattleId;
	const int32 Turn = C.GetState().TurnNumber;
	TestTrue(TEXT("First closes"), DrainBattle(C));
	TestFalse(TEXT("First envelope cleared"), C.GetState().IsBattlePhaseActive());
	TestTrue(TEXT("Action between battles"), C.GetMatchPhase() == EWBMatchLoopPhase::Action);
	TestTrue(TEXT("Second declaration"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestTrue(TEXT("Second active"), C.GetState().IsBattlePhaseActive());
	TestNotEqual(TEXT("Repeated exact Attack ID gets new Battle ID"), C.GetState().BattlePhase.BattleId, First);
	TestEqual(TEXT("Same Action turn"), C.GetState().TurnNumber, Turn);
	TestEqual(TEXT("Two starts"), BoundaryCount(C, TEXT("battle_phase_started")), 2);
	TestTrue(TEXT("Second closes"), DrainBattle(C));
	TestEqual(TEXT("Two ends"), BoundaryCount(C, TEXT("battle_phase_ended")), 2);
	TestFalse(TEXT("No identity leakage"), C.GetState().IsBattlePhaseActive());
	return true;
}

WB_BATTLE_TEST(FWBBattlePipeline, "Damage.AllStagesInsideEnvelope")
bool FWBBattlePipeline::RunTest(const FString&)
{
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest(false, 8)));
	TestTrue(TEXT("Attack"), SubmitCore(C, EWBActionType::Attack).bOk);
	const auto& Traces = C.GetTraceLog();
	int32 Start = INDEX_NONE, End = INDEX_NONE, Counter = INDEX_NONE;
	for (int32 I = 0; I < Traces.Num(); ++I)
	{
		if (Traces[I].Kind == FName(TEXT("battle_phase_started"))) Start = I;
		if (Traces[I].Kind == FName(TEXT("battle_phase_ended"))) End = I;
		if (Traces[I].Kind == FName(TEXT("counter_started"))) Counter = I;
	}
	TestTrue(TEXT("Start precedes end"), Start >= 0 && End > Start);
	TestTrue(TEXT("Counter inside same envelope"), Counter > Start && Counter < End);
	for (const TCHAR* Stage : {TEXT("automatic_pre_damage_modifiers"), TEXT("calculate_damage"),
		TEXT("substitute_damage"), TEXT("apply_damage"), TEXT("counter_eligibility")})
	{
		int32 Seen = 0;
		for (int32 I = 0; I < Traces.Num(); ++I)
			if (Traces[I].Kind == FName(TEXT("attack_stage_entered")) && Traces[I].AttackContinuationStage == FName(Stage))
			{
				++Seen;
				TestTrue(TEXT("Every current stage lies inside Battle"), I > Start && I < End);
			}
		TestTrue(TEXT("Pipeline stage observed"), Seen > 0);
	}
	for (int32 I = Start + 1; I < End; ++I)
	{
		TestNotEqual(TEXT("No intermediate end"), Traces[I].Kind, FName(TEXT("battle_phase_ended")));
		TestNotEqual(TEXT("No intermediate start"), Traces[I].Kind, FName(TEXT("battle_phase_started")));
	}
	return true;
}

WB_BATTLE_TEST(FWBBattleOutsideCondition, "Conditions.OutsideRestoredAfterBattle")
bool FWBBattleOutsideCondition::RunTest(const FString&)
{
	auto R = BattleRequest();
	auto E = BattleEffect(TEXT("outside_heal"), EWBGenericEffectOp::HealEffect);
	E.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	E.Payloads[0].HealEffect.Amount = 1;
	E.Payloads[0].HealEffect.SourceReason = FName(TEXT("battle_fixture"));
	E.SourceGate.Timing = EWBCardActivationTimingRequirement::NormalTurnPriority;
	E.SourceGate.bOncePerTurn = false;
	E.ActivationCondition.BattleRequirement = EWBCardEffectBattleRequirement::OutsideBattle;
	R.Repository.Definitions[0].ActivatedEffects.Add(E);
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	auto HasOutside = [&C]()
	{
		return C.EnumerateLegalActions().Actions.ContainsByPredicate([](const FWBMatchLegalAction& A)
			{return A.Family == EWBMatchActionFamily::Activation && A.ActivationCommand.Source.SourceEffectId == TEXT("outside_heal");});
	};
	TestTrue(TEXT("Outside condition before Battle"), HasOutside());
	TestTrue(TEXT("Attack"), SubmitCore(C, EWBActionType::Attack).bOk);
	TestFalse(TEXT("Outside unavailable in Battle"), HasOutside());
	TestTrue(TEXT("Close"), DrainBattle(C));
	TestTrue(TEXT("Outside legal after Battle"), HasOutside());
	TestTrue(TEXT("Fresh replay"), ReplayBattle(R, C));
	return true;
}


WB_BATTLE_TEST(FWBBattleNPC, "NPC.NoPlayerEnvelope")
bool FWBBattleNPC::RunTest(const FString&)
{
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, BattleRequest(false)));
	auto S = C.GetState();
	const int32 Hero = S.GetPlayerById(0)->HeroUnitId;
	const auto* H = S.GetUnitById(Hero);
	const FWBTile HeroTile(H->X, H->Y);
	FWBUnitState NPC;
	NPC.UnitId = 1000;
	NPC.CardId = TEXT("battle_npc");
	NPC.SetOwnerAndControllerForRules(-1, -1);
	NPC.X = H->X;
	NPC.Y = H->Y == 0 ? 1 : H->Y - 1;
	NPC.HP = 2; NPC.MaxHP = 2; NPC.ATK = 1; NPC.AR = 1; NPC.AttacksLeft = 1;
	TestTrue(TEXT("NPC fixture added"), S.AddUnitForTest(NPC));
	FWBAction Attack;
	Attack.Type = EWBActionType::Attack; Attack.PlayerId = -1;
	Attack.SourceUnitId = NPC.UnitId; Attack.TargetUnitId = Hero;
	Attack.FromTile = FWBTile(NPC.X, NPC.Y); Attack.ToTile = HeroTile;
	const auto Declared = WBEffectRunner::ApplyNPCAttackDeclare(S, C.GetRepository(), Attack);
	TestTrue(*Declared.Reason, Declared.bOk);
	TestTrue(TEXT("NPC pending exists"), S.HasPendingAttack());
	TestFalse(TEXT("NPC no Battle"), S.IsBattlePhaseActive());
	TestTrue(TEXT("NPC automatic provenance"), S.PendingAttack.AttackDeclaration == EWBDeclarationProvenance::Automatic);
	TestTrue(TEXT("Neutral authority unchanged"), S.PendingAttack.AuthorityKind == EWBAttackAuthorityKind::NeutralNPC);
	FString Reason;
	TestTrue(TEXT("NPC without Battle is valid"), WBBattlePhase::Validate(S, Reason));
	TArray<FWBTraceEvent> Events;
	TestFalse(TEXT("Cannot wrap NPC in player Battle"), WBBattlePhase::Begin(S, 1, 1, Events, Reason));
	TestEqual(TEXT("No Battle boundaries"), Events.Num(), 0);
	TestFalse(TEXT("No public Battle"), WBBattlePhase::BuildPublicSummary(S).bActive);
	return true;
}

WB_BATTLE_TEST(FWBBattleTransition, "Regression.AutomaticTransitionPreservesEnvelope")
bool FWBBattleTransition::RunTest(const FString&)
{
	auto R = BattleRequest();
	FWBCardZoneTransitionTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("battle_draw_observer");
	Trigger.SourceScope = EWBCardZoneTransitionTriggerSourceScope::MovedCardSelf;
	Trigger.Filter.bRequireSourceZone = true;
	Trigger.Filter.RequiredSourceZone = EWBCardZone::Deck;
	Trigger.Filter.bRequireDestinationZone = true;
	Trigger.Filter.RequiredDestinationZone = EWBCardZone::Hand;
	Trigger.DrawCount = 0;
	for (auto& D : R.Repository.Definitions)
		if (D.CardId == TEXT("battle_filler")) D.CardZoneTransitionTriggers.Add(Trigger);
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	TestTrue(TEXT("Open Battle"), SubmitCore(C, EWBActionType::Attack).bOk);
	auto S = C.GetState();
	const FString Id = S.BattlePhase.BattleId;
	const FString Root = S.BattlePhase.RootActionId;
	FWBCardZoneTransitionContext Context;
	Context.Cause = EWBCardZoneTransitionCause::Effect;
	Context.ContinuationId = TEXT("battle_synthetic_draw");
	const auto Draw = WBCardLifecycle::DrawOneCard(S, 0, Context);
	TestTrue(*Draw.Reason, Draw.bOk);
	const auto Resolved = WBCardZoneTransitionTrigger::ResolveCommittedTransitions(S, R.Repository, Draw.TransitionEvents);
	TestTrue(*Resolved.Reason, Resolved.bOk);
	TestEqual(TEXT("One automatic trigger"), Resolved.ResolvedTriggerCount, 1);
	TestEqual(TEXT("Same Battle ID"), S.BattlePhase.BattleId, Id);
	TestEqual(TEXT("Same root"), S.BattlePhase.RootActionId, Root);
	TestTrue(TEXT("Still active"), S.IsBattlePhaseActive());
	TestTrue(TEXT("Same pending attack"), S.HasPendingAttack());
	TestEqual(TEXT("Priority unchanged"), S.PriorityPlayer, C.GetState().PriorityPlayer);
	TestEqual(TEXT("No nested draws for zero count"), Resolved.NestedTransitionEvents.Num(), 0);
	TestFalse(TEXT("No new Battle boundary"), Resolved.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& E)
		{return E.Kind == FName(TEXT("battle_phase_started")) || E.Kind == FName(TEXT("battle_phase_ended"));}));
	FString Reason;
	TestTrue(TEXT("Still valid Battle"), WBBattlePhase::Validate(S, Reason));
	return true;
}

WB_BATTLE_TEST(FWBBattleProductionParser, "Parser.SyntheticBundle")
bool FWBBattleProductionParser::RunTest(const FString&)
{
	const auto Loaded = WBProductionCardDatabase::LoadManifestSuite(TEXT("Data/Replay/BattlePhaseFixture/root_manifest.json"));
	TestTrue(*Loaded.Reason, Loaded.bOk);
	if (!Loaded.bOk || !Loaded.Snapshot.IsValid()) return false;
	AddInfo(TEXT("Battle fixture bundle digest: ") + Loaded.Snapshot->ContentDigest);
	const auto* Hero = Loaded.Snapshot->FindHero(TEXT("battle_nested_hero_b"));
	TestNotNull(TEXT("Synthetic response Hero"), Hero);
	if (!Hero || Hero->CoreDefinition.ActivatedEffects.IsEmpty()) return false;
	TestTrue(TEXT("Production DuringBattle parsed"),
		Hero->CoreDefinition.ActivatedEffects[0].ActivationCondition.BattleRequirement == EWBCardEffectBattleRequirement::DuringBattle);
	TestFalse(TEXT("Outside denied by parsed condition"), Hero->CoreDefinition.ActivatedEffects[0].ActivationCondition.MatchesBattle(false));
	TestTrue(TEXT("Inside allowed by condition"), Hero->CoreDefinition.ActivatedEffects[0].ActivationCondition.MatchesBattle(true));
	TestTrue(TEXT("Still requires ResponseWindow timing"), Hero->CoreDefinition.ActivatedEffects[0].SourceGate.Timing == EWBCardActivationTimingRequirement::ResponseWindow);
	TestTrue(TEXT("Still requires Board source"), Hero->CoreDefinition.ActivatedEffects[0].SourceGate.RequiredZone == EWBCardActivationSourceZone::Board);
	TestTrue(TEXT("Still once per turn"), Hero->CoreDefinition.ActivatedEffects[0].SourceGate.bOncePerTurn);
	return true;
}

WB_BATTLE_TEST(FWBBattlePostHit, "Reaction.PostHitThroughCounter")
bool FWBBattlePostHit::RunTest(const FString&)
{
	auto R = BattleRequest(false, 8);
	auto E = BattleEffect(TEXT("battle_posthit_heal"), EWBGenericEffectOp::HealEffect);
	E.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	E.Payloads[0].HealEffect.Amount = 1;
	E.Payloads[0].HealEffect.SourceReason = FName(TEXT("battle_fixture"));
	R.Repository.Definitions[1].ActivatedEffects.Add(E);
	WBMatchCoordinator C;
	TestTrue(TEXT("Setup"), SetupBattle(C, R));
	TestFalse(TEXT("DuringBattle unavailable outside"), ReactBattle(C, E.EffectId));
	TestTrue(TEXT("Attack"), SubmitCore(C, EWBActionType::Attack).bOk);
	const FString Id = C.GetState().BattlePhase.BattleId;
	const FString Root = C.GetState().BattlePhase.RootActionId;
	TestTrue(TEXT("PreHit active"), C.GetState().IsBattlePhaseActive());
	TestTrue(TEXT("Pass PreHit"), SubmitCore(C, EWBActionType::PassResponse).bOk);
	TestTrue(TEXT("PostHit active"), C.GetState().IsBattlePhaseActive());
	TestTrue(TEXT("Existing PostHit window"), C.GetState().ReactionWindow.Kind == EWBReactionWindowKind::PostHit);
	TestEqual(TEXT("PostHit same Battle"), C.GetState().BattlePhase.BattleId, Id);
	TestEqual(TEXT("PostHit same root"), C.GetState().BattlePhase.RootActionId, Root);
	TestEqual(TEXT("No end before Counter eligibility"), BoundaryCount(C, TEXT("battle_phase_ended")), 0);
	TestTrue(TEXT("PostHit partial replay"), ReplayBattle(R, C));
	TestTrue(TEXT("Harmless DuringBattle React"), ReactBattle(C, E.EffectId));
	TestTrue(TEXT("Drain nested PostHit response and Counter"), DrainBattle(C));
	TestEqual(TEXT("One Counter"), BoundaryCount(C, TEXT("counter_started")), 1);
	TestEqual(TEXT("One Battle start"), BoundaryCount(C, TEXT("battle_phase_started")), 1);
	TestEqual(TEXT("One Battle end"), BoundaryCount(C, TEXT("battle_phase_ended")), 1);
	TestFalse(TEXT("Closed after Counter"), C.GetState().IsBattlePhaseActive());
	TestTrue(TEXT("Action resumed"), C.GetMatchPhase() == EWBMatchLoopPhase::Action);
	TestTrue(TEXT("Completed replay"), ReplayBattle(R, C));
	for (const auto& Event : C.GetTraceLog())
		if (Event.Kind == FName(TEXT("counter_started")))
		{
			TestFalse(TEXT("Counter is not Declared Attack"), Event.bDeclaredAttack);
			TestTrue(TEXT("Counter remains automatic"), Event.bCounterAttack);
		}
	return true;
}

#endif
