#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "WBMatchCoordinator.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionNPCReactionCombatSmoke.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardInstanceRef MakeCard(const FString& InstanceId, const FString& CardId, const int32 OwnerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerId;
	return Card;
}

FWBCardEffectDefinition MakeReactEffect(const FString& EffectId)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = TEXT("Respond");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Unit;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::HealEffect;
	Payload.HealEffect.Amount = 1;
	Payload.HealEffect.SourceReason = FName(TEXT("npc_reaction_fixture"));
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardDefinition MakeUnitDefinition(
	const FString& CardId,
	const EWBCardDefinitionKind Kind,
	const int32 AR,
	const bool bReact = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	Definition.CharacterStats.HP = 10;
	Definition.CharacterStats.ATK = Kind == EWBCardDefinitionKind::NPC ? 2 : 1;
	Definition.CharacterStats.AR = AR;
	Definition.CharacterStats.RL = 3;
	if (bReact)
	{
		Definition.ActivatedEffects.Add(MakeReactEffect(CardId + TEXT("_react")));
	}
	return Definition;
}

FWBCardDefinition MakeFiller()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("npc_authority_filler");
	Definition.PublicName = TEXT("NPC Authority Filler");
	Definition.Kind = EWBCardDefinitionKind::Action;
	return Definition;
}

FWBCardDefinition MakeTrap()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("npc_authority_trap");
	Definition.PublicName = TEXT("NPC Authority Trap");
	Definition.Kind = EWBCardDefinitionKind::Trap;
	Definition.TrapDamage = 1;
	return Definition;
}

FWBSetupMarkerPlacement MakeMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = PlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap
		? TEXT("npc_authority_trap")
		: TEXT("npc_authority_neutral");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBMatchPlayerSetup MakePlayer(const int32 PlayerId)
{
	FWBMatchPlayerSetup Setup;
	Setup.PlayerId = PlayerId;
	Setup.HeroCardId = PlayerId == 0
		? TEXT("npc_authority_hero_a")
		: TEXT("npc_authority_hero_b");
	Setup.HeroInstanceId = FString::Printf(TEXT("npc_authority_p%d_hero"), PlayerId);
	Setup.OrderedDeck.Add(MakeCard(Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Setup.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("npc_authority_p%d_filler_%d"), PlayerId, Index),
			TEXT("npc_authority_filler"),
			PlayerId));
	}
	return Setup;
}

FWBMatchInitializationRequest MakeRequest()
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 81227;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("npc_combat_authority_tests");
	Request.Repository.SourceVersion = TEXT("v1");
	Request.Repository.Definitions = {
		MakeUnitDefinition(TEXT("npc_authority_hero_a"), EWBCardDefinitionKind::Character, 1, true),
		MakeUnitDefinition(TEXT("npc_authority_hero_b"), EWBCardDefinitionKind::Character, 1, true),
		MakeUnitDefinition(TEXT("npc_authority_neutral"), EWBCardDefinitionKind::NPC, 2),
		MakeFiller(),
		MakeTrap()
	};
	Request.Players = { MakePlayer(0), MakePlayer(1) };
	Request.MarkerPlacements = {
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(0, 7), 2),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(1, 7), 3),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(0, 1), 6),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(1, 1), 7)
	};
	return Request;
}

FWBUnitState MakeNPC(const int32 UnitId, const FWBTile Tile)
{
	FWBUnitState NPC;
	NPC.UnitId = UnitId;
	NPC.OwnerId = -1;
	NPC.CardId = TEXT("npc_authority_neutral");
	NPC.X = Tile.X;
	NPC.Y = Tile.Y;
	NPC.HP = 10;
	NPC.MaxHP = 10;
	NPC.ATK = 2;
	NPC.AR = 2;
	NPC.MaxAttacksPerTurn = 1;
	NPC.NPCSpawnOrder = 0;
	NPC.NPCCreationTurnNumber = 1;
	return NPC;
}

const FWBMatchLegalAction* FindCore(
	const TArray<FWBMatchLegalAction>& Actions,
	const EWBActionType Type)
{
	return Actions.FindByPredicate([Type](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == Type;
	});
}

int32 CountTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.FilterByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	}).Num();
}

struct FNPCReactionRun
{
	bool bOk = false;
	FString Reason;
	int32 InitialPriority = -1;
	int32 PriorityAfterFirstPass = -1;
	int32 TargetHPBefore = -1;
	int32 TargetHPAfter = -1;
	int32 MPRollTraceCount = 0;
	int32 DamageTraceCount = 0;
	int32 CounterTraceCount = 0;
	bool bCounterWasDeclaredAttack = false;
	bool bCounterTargetWasDeclared = false;
	int32 NPCHPAfter = -1;
	FString StateDigest;
	FString TraceDigest;
};

FNPCReactionRun RunNPCReaction(const int32 TargetPlayerId)
{
	FNPCReactionRun Run;
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest());
	if (!Started.bOk)
	{
		Run.Reason = Started.Reason;
		return Run;
	}
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	const FWBPlayerStateData* TargetPlayer = State.GetPlayerById(TargetPlayerId);
	const FWBUnitState* TargetHero = TargetPlayer == nullptr
		? nullptr
		: State.GetUnitById(TargetPlayer->HeroUnitId);
	if (TargetHero == nullptr)
	{
		Run.Reason = TEXT("target_hero_missing");
		return Run;
	}
	const int32 TargetHeroUnitId = TargetHero->UnitId;
	Run.TargetHPBefore = TargetHero->HP;
	const int32 NPCUnitId = 900 + TargetPlayerId;
	const FWBTile NPCTile(TargetHero->X, TargetHero->Y + (TargetPlayerId == 0 ? -1 : 1));
	if (!State.AddUnitForTest(MakeNPC(NPCUnitId, NPCTile)))
	{
		Run.Reason = TEXT("npc_add_failed");
		return Run;
	}

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* EndTurn = Legal.bOk
		? FindCore(Legal.Actions, EWBActionType::EndTurn)
		: nullptr;
	if (EndTurn == nullptr)
	{
		Run.Reason = Legal.bOk ? TEXT("end_turn_missing") : Legal.Reason;
		return Run;
	}
	const FWBMatchOperationResult Ended =
		Coordinator.SubmitActionId(EndTurn->PlayerId, EndTurn->ActionId);
	if (!Ended.bOk || Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Response)
	{
		Run.Reason = Ended.bOk ? TEXT("npc_pre_hit_not_open") : Ended.Reason;
		return Run;
	}
	Run.InitialPriority = Coordinator.GetState().PriorityPlayer;

	int32 AcceptedPasses = 0;
	for (int32 Guard = 0; Guard < 12
		&& Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response; ++Guard)
	{
		const FWBMatchLegalActionGenerationResult ResponseLegal = Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = ResponseLegal.bOk
			? FindCore(ResponseLegal.Actions, EWBActionType::PassResponse)
			: nullptr;
		if (Pass == nullptr)
		{
			Run.Reason = ResponseLegal.bOk ? TEXT("pass_response_missing") : ResponseLegal.Reason;
			return Run;
		}
		const FWBMatchOperationResult Passed =
			Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId);
		if (!Passed.bOk)
		{
			Run.Reason = Passed.Reason;
			return Run;
		}
		++AcceptedPasses;
		if (AcceptedPasses == 1)
		{
			Run.PriorityAfterFirstPass = Coordinator.GetState().PriorityPlayer;
		}
	}
	if (Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response)
	{
		Run.Reason = TEXT("response_guard_exceeded");
		return Run;
	}
	TargetHero = Coordinator.GetState().GetUnitById(TargetHeroUnitId);
	Run.TargetHPAfter = TargetHero == nullptr ? 0 : TargetHero->HP;
	Run.MPRollTraceCount = CountTrace(Coordinator.GetTraceLog(), FName(TEXT("npc_mp_rolled")));
	Run.DamageTraceCount = CountTrace(Coordinator.GetTraceLog(), FName(TEXT("npc_attack_damage_resolved")));
	Run.CounterTraceCount = CountTrace(Coordinator.GetTraceLog(), FName(TEXT("counter_started")));
	if (const FWBTraceEvent* CounterTrace = Coordinator.GetTraceLog().FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("counter_started"));
		}))
	{
		Run.bCounterWasDeclaredAttack = CounterTrace->bDeclaredAttack;
		Run.bCounterTargetWasDeclared = CounterTrace->bDeclaredTarget;
	}
	const FWBUnitState* FinalNPC = Coordinator.GetState().GetUnitById(NPCUnitId);
	Run.NPCHPAfter = FinalNPC == nullptr ? 0 : FinalNPC->HP;
	Run.StateDigest = Coordinator.GetCurrentStateDigest();
	Run.TraceDigest = Coordinator.GetCurrentTraceDigest();
	Run.bOk = true;
	return Run;
}

FWBGameStateData MakeCounterState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 2;
	FWBUnitState Attacker;
	Attacker.UnitId = 10;
	Attacker.OwnerId = 0;
	Attacker.X = 4;
	Attacker.Y = 4;
	Attacker.HP = 5;
	Attacker.MaxHP = 5;
	Attacker.AR = 1;
	FWBUnitState Defender = Attacker;
	Defender.UnitId = 20;
	Defender.OwnerId = 1;
	Defender.Y = 3;
	State.Units = { Attacker, Defender };
	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.Stage = EWBAttackContinuationStage::Counter;
	Pending.AttackerUnitId = 10;
	Pending.DefenderUnitId = 20;
	Pending.OriginalAttackerUnitId = 10;
	Pending.OriginalDefenderUnitId = 20;
	State.PendingAttack = Pending;
	return State;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCReactionPlayerZeroPriorityTest,
	"Wandbound.NPC.Combat.Reaction.PlayerZeroDefenderGetsFirstPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCReactionPlayerZeroPriorityTest::RunTest(const FString&)
{
	const FNPCReactionRun Run = RunNPCReaction(0);
	TestTrue(TEXT("Run succeeds"), Run.bOk);
	TestEqual(TEXT("Player zero first"), Run.InitialPriority, 0);
	TestEqual(TEXT("Priority alternates"), Run.PriorityAfterFirstPass, 1);
	TestEqual(TEXT("Damage once"), Run.TargetHPAfter, Run.TargetHPBefore - 2);
	TestEqual(TEXT("One NPC damage trace"), Run.DamageTraceCount, 1);
	TestEqual(TEXT("One MP roll"), Run.MPRollTraceCount, 1);
	TestEqual(TEXT("Defender counter occurs"), Run.CounterTraceCount, 1);
	TestFalse(TEXT("Counter is not a declared attack"), Run.bCounterWasDeclaredAttack);
	TestFalse(TEXT("Automatic counter target is not declared"), Run.bCounterTargetWasDeclared);
	TestEqual(TEXT("Counter damages NPC once"), Run.NPCHPAfter, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCReactionPlayerOnePriorityTest,
	"Wandbound.NPC.Combat.Reaction.PlayerOneDefenderGetsFirstPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCReactionPlayerOnePriorityTest::RunTest(const FString&)
{
	const FNPCReactionRun Run = RunNPCReaction(1);
	TestTrue(TEXT("Run succeeds"), Run.bOk);
	TestEqual(TEXT("Player one first"), Run.InitialPriority, 1);
	TestEqual(TEXT("Priority alternates"), Run.PriorityAfterFirstPass, 0);
	TestEqual(TEXT("Damage once"), Run.TargetHPAfter, Run.TargetHPBefore - 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCReactionDeterminismTest,
	"Wandbound.NPC.Combat.Replay.FreshEquivalentRunMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCReactionDeterminismTest::RunTest(const FString&)
{
	const FNPCReactionRun First = RunNPCReaction(0);
	const FNPCReactionRun Second = RunNPCReaction(0);
	TestTrue(TEXT("First succeeds"), First.bOk);
	TestTrue(TEXT("Second succeeds"), Second.bOk);
	TestEqual(TEXT("State digest"), First.StateDigest, Second.StateDigest);
	TestEqual(TEXT("Trace digest"), First.TraceDigest, Second.TraceDigest);
	TestEqual(TEXT("Roll count stable"), First.MPRollTraceCount, Second.MPRollTraceCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCounterabilityTypedUnitCapabilityTest,
	"Wandbound.Combat.Counterability.TypedUnitCapabilitySkipsCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCounterabilityTypedUnitCapabilityTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeCounterState();
	TestTrue(TEXT("Ordinary counter eligible"), WBRules::CanResolveCounterattack(State).bOk);
	State.GetMutableUnitById(10)->CombatCapabilities.Add(
		EWBCombatCapability::AttacksCannotBeCountered);
	const FWBActionQueryResult Query = WBRules::CanResolveCounterattack(State);
	TestFalse(TEXT("Counter denied"), Query.bOk);
	TestEqual(TEXT("Deterministic reason"), Query.Reason, FString(TEXT("attack_cannot_be_countered")));
	TestTrue(TEXT("Damage stage remains legal"), WBRules::CanResolvePendingAttackDamage(State).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCounterabilityEquippedCapabilityTest,
	"Wandbound.Combat.Counterability.EquippedTypedCapabilitySkipsCounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCounterabilityEquippedCapabilityTest::RunTest(const FString&)
{
	FWBGameStateData State = MakeCounterState();
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = 0;
	State.CardZoneState.PlayerZones.Add(Zones);
	FWBEquippedCardEntry Equipped;
	Equipped.Card = MakeCard(TEXT("fixture_wand_instance"), TEXT("fixture_wand_a"), 0);
	Equipped.EquippedToUnitId = 10;
	State.CardZoneState.EquippedCards.Add(Equipped);
	FWBCardDefinition Wand;
	Wand.CardId = TEXT("fixture_wand_a");
	Wand.PublicName = TEXT("Fixture Wand A");
	Wand.Kind = EWBCardDefinitionKind::Wand;
	Wand.GrantedCombatCapabilitiesWhileEquipped.Add(
		EWBCombatCapability::AttacksCannotBeCountered);
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("counterability_fixture");
	Repository.SourceVersion = TEXT("v1");
	Repository.Definitions.Add(Wand);
	const FWBActionQueryResult Query = WBRules::CanResolveCounterattack(State, Repository);
	TestFalse(TEXT("Equipped capability denies counter"), Query.bOk);
	TestEqual(TEXT("Generic reason"), Query.Reason, FString(TEXT("attack_cannot_be_countered")));
	Wand.CardId = TEXT("fixture_wand_b");
	Repository.Definitions = { Wand };
	State.CardZoneState.EquippedCards[0].Card.CardId = TEXT("fixture_wand_b");
	TestFalse(TEXT("Equivalent fixture id has same behavior"),
		WBRules::CanResolveCounterattack(State, Repository).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCounterabilityReactionWindowsRemainTest,
	"Wandbound.Combat.Counterability.PreHitAndPostHitRemainCounterOnlySkipped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCounterabilityReactionWindowsRemainTest::RunTest(const FString&)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(MakeRequest());
	TestTrue(TEXT("Match starts"), Started.bOk);
	if (!Started.bOk)
	{
		return true;
	}
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	State.TurnNumber = 2;
	const FWBPlayerStateData* PlayerZero = State.GetPlayerById(0);
	const FWBPlayerStateData* PlayerOne = State.GetPlayerById(1);
	FWBUnitState* Attacker = PlayerZero == nullptr
		? nullptr
		: State.GetMutableUnitById(PlayerZero->HeroUnitId);
	FWBUnitState* Defender = PlayerOne == nullptr
		? nullptr
		: State.GetMutableUnitById(PlayerOne->HeroUnitId);
	TestNotNull(TEXT("Attacker exists"), Attacker);
	TestNotNull(TEXT("Defender exists"), Defender);
	if (Attacker == nullptr || Defender == nullptr)
	{
		return true;
	}
	Attacker->X = 4;
	Attacker->Y = 4;
	Attacker->AR = 1;
	Attacker->AttacksLeft = 1;
	Attacker->CombatCapabilities.Add(EWBCombatCapability::AttacksCannotBeCountered);
	Defender->X = 4;
	Defender->Y = 3;
	Defender->AR = 1;
	const int32 AttackerUnitId = Attacker->UnitId;
	const int32 DefenderUnitId = Defender->UnitId;
	const int32 AttackerATK = Attacker->ATK;
	const int32 AttackerHPBefore = Attacker->HP;
	const int32 DefenderHPBefore = Defender->HP;

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Attack = Legal.Actions.FindByPredicate(
		[AttackerUnitId, DefenderUnitId](const FWBMatchLegalAction& Action)
		{
			return Action.Family == EWBMatchActionFamily::CoreAction
				&& Action.CoreAction.Type == EWBActionType::Attack
				&& Action.CoreAction.SourceUnitId == AttackerUnitId
				&& Action.CoreAction.TargetUnitId == DefenderUnitId;
		});
	TestNotNull(TEXT("Attack legal"), Attack);
	if (Attack == nullptr)
	{
		return true;
	}
	const FWBMatchOperationResult Declared =
		Coordinator.SubmitActionId(Attack->PlayerId, Attack->ActionId);
	TestTrue(TEXT("Attack accepted"), Declared.bOk);
	for (int32 Guard = 0; Guard < 12
		&& Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response; ++Guard)
	{
		const FWBMatchLegalActionGenerationResult ResponseLegal = Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass =
			FindCore(ResponseLegal.Actions, EWBActionType::PassResponse);
		TestNotNull(TEXT("Response pass legal"), Pass);
		if (Pass == nullptr)
		{
			break;
		}
		TestTrue(
			TEXT("Pass accepted"),
			Coordinator.SubmitActionId(Pass->PlayerId, Pass->ActionId).bOk);
	}
	TestNotEqual(TEXT("Attack completes"), Coordinator.GetMatchPhase(), EWBMatchLoopPhase::Response);
	TestEqual(TEXT("PreHit and PostHit both opened"),
		CountTrace(Coordinator.GetTraceLog(), FName(TEXT("reaction_window_opened"))), 2);
	TestEqual(TEXT("Counter skipped"),
		CountTrace(Coordinator.GetTraceLog(), FName(TEXT("counter_started"))), 0);
	TestEqual(TEXT("Defender takes ordinary damage"),
		Coordinator.GetState().GetUnitById(DefenderUnitId)->HP,
		DefenderHPBefore - AttackerATK);
	TestEqual(TEXT("Attacker takes no counter damage"),
		Coordinator.GetState().GetUnitById(AttackerUnitId)->HP,
		AttackerHPBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCReactionCombatFixtureLoadsTest,
	"Wandbound.NPC.Combat.Fixture.BundleLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCReactionCombatFixtureLoadsTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/NPCReactionCombatFixture/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	if (!Loaded.bOk)
	{
		for (const FWBProductionCardDBDiagnostic& Diagnostic : Loaded.Diagnostics)
		{
			AddError(FString::Printf(
				TEXT("%s | %s | %s"),
				*Diagnostic.Code,
				*Diagnostic.FieldPath,
				*Diagnostic.Message));
		}
	}
	TestTrue(TEXT("NPC reaction combat fixture loads"), Loaded.bOk);
	if (Loaded.Snapshot.IsValid())
	{
		AddInfo(FString::Printf(
			TEXT("NPC_REACTION_COMBAT_FIXTURE_DIGEST=%s"),
			*Loaded.Snapshot->ContentDigest));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCReactionCombatProductionSmokeTest,
	"Wandbound.NPC.Combat.Fixture.ProductionSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCReactionCombatProductionSmokeTest::RunTest(const FString&)
{
	const FString Root = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/NPCReactionCombatFixture"));
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(Root, TEXT("root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(Root, TEXT("match_spec.json"));
	Request.bAllowTestBundle = true;
	const FWBProductionNPCReactionCombatSmokeResult Result =
		WBProductionNPCReactionCombatSmoke::Run(Request);
	TestTrue(
		*FString::Printf(TEXT("Production NPC combat smoke succeeds: %s"), *Result.Reason),
		Result.bOk);
	TestTrue(TEXT("Response action ID captured"), !Result.ReactionActionId.IsEmpty());
	TestTrue(TEXT("Player attack action ID captured"), !Result.PlayerAttackActionId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBCombatAuthorityNoCardIdBranchTest,
	"Wandbound.Authority.Combat.NoCardIdSpecificExceptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCombatAuthorityNoCardIdBranchTest::RunTest(const FString&)
{
	const TArray<FString> Paths = {
		TEXT("Source/WandboundCore/Private/WBMatchCoordinator.cpp"),
		TEXT("Source/WandboundCore/Private/WBRules.cpp"),
		TEXT("Source/WandboundCore/Private/WBEffectRunner.cpp"),
		TEXT("Source/WandboundCore/Private/WBNPCPhaseResolution.cpp")
	};
	const TArray<FString> Forbidden = {
		TEXT("Oddsman"), TEXT("Sealplate"), TEXT("Null Sigil"),
		TEXT("Claimshifter"), TEXT("Crash-In"), TEXT("Sever Thread"),
		TEXT("Shatter Parry"), TEXT("Lockout Sigil"), TEXT("wand_equip_no_counter")
	};
	for (const FString& RelativePath : Paths)
	{
		FString Source;
		TestTrue(*RelativePath, FFileHelper::LoadFileToString(
			Source, *FPaths::Combine(FPaths::ProjectDir(), RelativePath)));
		for (const FString& Name : Forbidden)
		{
			TestFalse(*(RelativePath + TEXT(" excludes ") + Name), Source.Contains(Name));
		}
	}
	return true;
}

#endif
