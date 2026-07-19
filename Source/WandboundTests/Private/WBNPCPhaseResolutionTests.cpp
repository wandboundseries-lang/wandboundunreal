#include "Misc/AutomationTest.h"

#include "WBNPCPhaseResolution.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardDefinition MakeDefinition(const FString& CardId, const EWBCardDefinitionKind Kind)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	Definition.CharacterStats.HP = 6;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 0;
	return Definition;
}

FWBCardDefinitionRepository MakeRepository()
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("npc_phase_tests");
	Repository.SourceVersion = TEXT("v1");
	Repository.Definitions = {
		MakeDefinition(TEXT("npc_alpha"), EWBCardDefinitionKind::NPC),
		MakeDefinition(TEXT("npc_beta"), EWBCardDefinitionKind::NPC),
		MakeDefinition(TEXT("basic_trap"), EWBCardDefinitionKind::Trap)
	};
	return Repository;
}

FWBUnitState MakePlayerUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile Tile,
	const int32 HP = 8)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("player_unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = HP;
	Unit.ATK = 1;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBUnitState MakeNPC(
	const int32 UnitId,
	const int32 SpawnOrder,
	const FWBTile Tile,
	const FString& CardId = TEXT("npc_alpha"))
{
	FWBUnitState Unit = MakePlayerUnit(UnitId, -1, Tile, 6);
	Unit.CardId = CardId;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 0;
	Unit.NPCSpawnOrder = SpawnOrder;
	Unit.NPCCreationTurnNumber = 1;
	return Unit;
}

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 1;
	State.PriorityPlayer = 1;
	State.TurnNumber = 2;
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBPlayerStateData Player;
		Player.PlayerId = PlayerId;
		Player.HeroUnitId = INDEX_NONE;
		State.Players.Add(Player);
		FWBPlayerCardZoneState Zones;
		Zones.PlayerId = PlayerId;
		State.CardZoneState.PlayerZones.Add(Zones);
	}
	return State;
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

TArray<int32> TraceSources(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	TArray<int32> Sources;
	for (const FWBTraceEvent& Event : Events)
	{
		if (Event.Kind == Kind)
		{
			Sources.Add(Event.SourceUnitId);
		}
	}
	return Sources;
}

FString StateFingerprint(const FWBGameStateData& State)
{
	FString Result = FString::Printf(TEXT("g%d:w%d|"), State.bGameOver ? 1 : 0, State.WinnerPlayerId);
	for (const FWBUnitState& Unit : State.Units)
	{
		Result += FString::Printf(
			TEXT("u%d:o%d:%d,%d:h%d:a%d:m%d:s%d|"),
			Unit.UnitId,
			Unit.OwnerId,
			Unit.X,
			Unit.Y,
			Unit.HP,
			Unit.CurrentArmor,
			Unit.MPRemaining,
			Unit.NPCSpawnOrder);
	}
	return Result;
}

const FWBTraceEvent* FindFirstTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.FindByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCSystemAuthorityTest,
	"Wandbound.Core.NPC.Authority.NeutralUsesInternalRulesOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCSystemAuthorityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(4, 2)));
	FWBAction Move;
	Move.Type = EWBActionType::Move;
	Move.PlayerId = -1;
	Move.SourceUnitId = 10;
	Move.FromTile = FWBTile(4, 4);
	Move.ToTile = FWBTile(4, 3);
	TestFalse(TEXT("Player move path rejects neutral"), WBRules::QueryMove(State, Move).bOk);
	TestTrue(TEXT("NPC authority accepts legal neutral move"), WBRules::QueryNPCMove(State, Move, 1).bOk);
	for (const FWBAction& Action : WBRules::GenerateLegalActionsForPlayer(State, 0))
	{
		TestNotEqual(TEXT("Player legal actions never source NPC"), Action.SourceUnitId, 10);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCDeterministicRollTest,
	"Wandbound.Core.NPC.RNG.OneCanonicalRollPerEligibleNPC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCDeterministicRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData First = MakeState();
	First.Units = { MakeNPC(11, 1, FWBTile(8, 8)), MakeNPC(10, 0, FWBTile(0, 0)) };
	FWBGameStateData Second = First;
	uint32 FirstRNG = 12345;
	uint32 SecondRNG = 12345;
	const FWBNPCPhaseResolutionResult A = WBNPCPhaseResolution::ResolvePhase(First, MakeRepository(), FirstRNG, 0);
	const FWBNPCPhaseResolutionResult B = WBNPCPhaseResolution::ResolvePhase(Second, MakeRepository(), SecondRNG, 0);
	TestTrue(TEXT("First phase succeeds"), A.bOk);
	TestTrue(TEXT("Second phase succeeds"), B.bOk);
	TestEqual(TEXT("Exactly two rolls"), A.MPRolls.Num(), 2);
	TestTrue(TEXT("Identical rolls"), A.MPRolls == B.MPRolls);
	TestEqual(TEXT("Identical final RNG"), FirstRNG, SecondRNG);
	TestEqual(TEXT("Identical traces"), WBReplayTrace::SerializeEvents(A.TraceEvents), WBReplayTrace::SerializeEvents(B.TraceEvents));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCAttackArmorTest,
	"Wandbound.Core.NPC.Combat.AttacksInRangeThroughArmorPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCAttackArmorTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	FWBUnitState Target = MakePlayerUnit(1, 0, FWBTile(4, 3), 5);
	Target.SetArmorForTest(1, 1);
	State.Units.Add(Target);
	uint32 RNG = 99;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestEqual(TEXT("Armor consumed"), State.GetUnitById(1)->CurrentArmor, 0);
	TestEqual(TEXT("One HP damage after armor"), State.GetUnitById(1)->HP, 4);
	TestEqual(TEXT("Attack consumed"), State.GetUnitById(10)->AttacksLeft, 0);
	TestTrue(TEXT("NPC declaration traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_attack_declared"))));
	TestTrue(TEXT("NPC damage traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_attack_damage_resolved"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCMoveThenAttackTest,
	"Wandbound.Core.NPC.Combat.MovesThenAttacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCMoveThenAttackTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(4, 2), 5));
	uint32 RNG = 7;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestEqual(TEXT("NPC enters attack position"), State.GetUnitById(10)->Y, 3);
	TestEqual(TEXT("Target takes attack"), State.GetUnitById(1)->HP, 3);
	TestTrue(TEXT("Move traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_moved"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCStableOrderTest,
	"Wandbound.Core.NPC.Order.SpawnOrderIndependentOfInsertion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCStableOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units = { MakeNPC(20, 1, FWBTile(8, 8)), MakeNPC(10, 0, FWBTile(0, 0)) };
	uint32 RNG = 44;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	const TArray<int32> Order = TraceSources(Result.TraceEvents, FName(TEXT("npc_action_started")));
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestEqual(TEXT("Two actions start"), Order.Num(), 2);
	if (Order.Num() == 2)
	{
		TestEqual(TEXT("Oldest first"), Order[0], 10);
		TestEqual(TEXT("Newest second"), Order[1], 20);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCBlockedAndStatusTest,
	"Wandbound.Core.NPC.Movement.BlockedAndStunnedAreNormalSkips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCBlockedAndStatusTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	FWBUnitState Stunned = MakeNPC(10, 0, FWBTile(0, 0));
	Stunned.AddStatus(FName(TEXT("Stunned")), 1);
	State.Units.Add(Stunned);
	State.Units.Add(MakeNPC(11, 1, FWBTile(8, 8)));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(8, 6)));
	State.AddWallForTest(FWBWallEdge(FWBTile(8, 8), FWBTile(8, 7)));
	State.AddWallForTest(FWBWallEdge(FWBTile(8, 8), FWBTile(7, 8)));
	uint32 RNG = 123;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestTrue(TEXT("No-action outcomes do not fail phase"), Result.bOk);
	TestEqual(TEXT("Both eligible NPCs roll"), Result.MPRolls.Num(), 2);
	TestTrue(TEXT("Skip traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_skipped"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCTargetEligibilityTest,
	"Wandbound.Core.NPC.Targeting.PlayerUnitsOnlyWithStableTieBreak",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCTargetEligibilityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	State.Units.Add(MakeNPC(11, 1, FWBTile(4, 3)));
	State.Units.Add(MakePlayerUnit(2, 1, FWBTile(3, 4), 5));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(5, 4), 5));
	uint32 RNG = 12;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	const FWBTraceEvent* Selected = FindFirstTrace(Result.TraceEvents, FName(TEXT("npc_target_selected")));
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestNotNull(TEXT("Target selected"), Selected);
	if (Selected != nullptr)
	{
		TestEqual(TEXT("Owner then unit ID tie-break selects player zero"), Selected->TargetUnitId, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCTrapDeathTest,
	"Wandbound.Core.NPC.Marker.TrapKillsMoverAndStopsAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCTrapDeathTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	FWBUnitState NPC = MakeNPC(10, 0, FWBTile(0, 0));
	NPC.HP = 2;
	NPC.MaxHP = 2;
	State.Units.Add(NPC);
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(3, 0)));
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 0;
	Marker.OwnerPlayerId = 0;
	Marker.Type = EWBMarkerType::Trap;
	Marker.Tile = FWBTile(1, 0);
	Marker.PlacementOrder = 0;
	Marker.InternalMarkerCardId = TEXT("basic_trap");
	State.CardZoneState.MarkerPlaceholders.Add(Marker);
	uint32 RNG = 1;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestTrue(TEXT("Phase succeeds despite NPC death"), Result.bOk);
	TestFalse(TEXT("NPC removed from board"), State.GetUnitById(10)->IsUnitOnBoard());
	TestTrue(TEXT("NPC marker wrapper traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_marker_triggered"))));
	TestTrue(TEXT("Defeat traced"), HasTrace(Result.TraceEvents, FName(TEXT("unit_defeated"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCTriggerTargetTieBreakTest,
	"Wandbound.Core.NPC.Targeting.TriggeringUnitBreaksEqualRankTie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCTriggerTargetTieBreakTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	FWBUnitState NPC = MakeNPC(10, 0, FWBTile(4, 4));
	NPC.NPCTriggeredByUnitId = 2;
	State.Units.Add(NPC);
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(3, 4), 5));
	State.Units.Add(MakePlayerUnit(2, 1, FWBTile(5, 4), 5));
	uint32 RNG = 67;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	const FWBTraceEvent* Selected = FindFirstTrace(Result.TraceEvents, FName(TEXT("npc_target_selected")));
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestNotNull(TEXT("Target selected"), Selected);
	if (Selected != nullptr)
	{
		TestEqual(TEXT("Triggering unit wins equal tie"), Selected->TargetUnitId, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCHeroDefeatTest,
	"Wandbound.Core.NPC.Combat.HeroDefeatStopsPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCHeroDefeatTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	State.Units.Add(MakeNPC(11, 1, FWBTile(8, 8)));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(4, 3), 1));
	State.GetMutablePlayerById(0)->HeroUnitId = 1;
	uint32 RNG = 91;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestTrue(TEXT("Phase succeeds terminally"), Result.bOk);
	TestTrue(TEXT("Game over"), State.bGameOver);
	TestEqual(TEXT("Opponent wins"), State.WinnerPlayerId, 1);
	TestEqual(TEXT("Later NPC never rolls"), Result.MPRolls.Num(), 1);
	TestTrue(TEXT("Hero defeat traced"), HasTrace(Result.TraceEvents, FName(TEXT("hero_defeated"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCPendingSpawnActsTest,
	"Wandbound.Core.NPC.Spawn.NewSpawnActsInSamePhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCPendingSpawnActsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(8, 4)));
	FWBPendingNPCSpawnState Pending;
	Pending.PendingSpawnId = 0;
	Pending.SourceMarkerId = 4;
	Pending.MarkerOwnerPlayerId = 0;
	Pending.NPCDefinitionId = TEXT("npc_alpha");
	Pending.OriginTile = FWBTile(4, 4);
	Pending.SpawnOrder = 0;
	Pending.CreatedTurnNumber = 1;
	State.PendingNPCSpawns.Add(Pending);
	uint32 RNG = 8;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestEqual(TEXT("One spawn"), Result.SpawnedCount, 1);
	TestEqual(TEXT("Spawn is eligible immediately"), Result.EligibleNPCCount, 1);
	TestEqual(TEXT("Spawn rolls"), Result.MPRolls.Num(), 1);
	TestTrue(TEXT("Spawn action starts"), HasTrace(Result.TraceEvents, FName(TEXT("npc_action_started"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCAtomicFailureTest,
	"Wandbound.Core.NPC.Transaction.MalformedStateRestoresStateAndRNG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCAtomicFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(0, 0)));
	State.Units.Add(MakeNPC(11, 0, FWBTile(8, 8)));
	const FString Before = StateFingerprint(State);
	uint32 RNG = 55;
	const uint32 RNGBefore = RNG;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	TestFalse(TEXT("Duplicate spawn order fails"), Result.bOk);
	TestEqual(TEXT("Explicit reason"), Result.Reason, FString(TEXT("npc_spawn_metadata_invalid")));
	TestEqual(TEXT("State unchanged"), StateFingerprint(State), Before);
	TestEqual(TEXT("RNG unchanged"), RNG, RNGBefore);
	TestEqual(TEXT("No trace committed"), Result.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCHiddenMarkerPlanningTest,
	"Wandbound.Core.NPC.HiddenInfo.MarkerIdentityDoesNotAffectPlanning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCHiddenMarkerPlanningTest::RunTest(const FString& Parameters)
{
	FWBGameStateData TrapState = MakeState();
	TrapState.Units.Add(MakeNPC(10, 0, FWBTile(0, 0)));
	TrapState.Units.Add(MakePlayerUnit(1, 0, FWBTile(3, 0), 8));
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 0;
	Marker.OwnerPlayerId = 0;
	Marker.Type = EWBMarkerType::Trap;
	Marker.Tile = FWBTile(1, 0);
	Marker.PlacementOrder = 0;
	Marker.InternalMarkerCardId = TEXT("basic_trap");
	TrapState.CardZoneState.MarkerPlaceholders.Add(Marker);
	FWBGameStateData NPCState = TrapState;
	NPCState.CardZoneState.MarkerPlaceholders[0].Type = EWBMarkerType::NPC;
	NPCState.CardZoneState.MarkerPlaceholders[0].InternalMarkerCardId = TEXT("npc_beta");
	uint32 TrapRNG = 101;
	uint32 NPCRNG = 101;
	const FWBNPCPhaseResolutionResult TrapResult = WBNPCPhaseResolution::ResolvePhase(TrapState, MakeRepository(), TrapRNG, 0);
	const FWBNPCPhaseResolutionResult NPCResult = WBNPCPhaseResolution::ResolvePhase(NPCState, MakeRepository(), NPCRNG, 0);
	const FWBTraceEvent* TrapPlan = FindFirstTrace(TrapResult.TraceEvents, FName(TEXT("npc_movement_planned")));
	const FWBTraceEvent* NPCPlan = FindFirstTrace(NPCResult.TraceEvents, FName(TEXT("npc_movement_planned")));
	TestNotNull(TEXT("Trap state planned"), TrapPlan);
	TestNotNull(TEXT("NPC state planned"), NPCPlan);
	if (TrapPlan != nullptr && NPCPlan != nullptr)
	{
		TestEqual(TEXT("Same first destination"), TrapPlan->ToTile, NPCPlan->ToTile);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCReplayTraceFieldsTest,
	"Wandbound.Core.NPC.Replay.StructuredFieldsSerialize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCReplayTraceFieldsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(4, 2)));
	uint32 RNG = 222;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	const FString Serialized = WBReplayTrace::SerializeEvents(Result.TraceEvents);
	TestTrue(TEXT("Action sequence serialized"), Serialized.Contains(TEXT("\"action_sequence\"")));
	TestTrue(TEXT("Path step serialized"), Serialized.Contains(TEXT("\"path_step_index\"")));
	TestTrue(TEXT("Spawn order serialized"), Serialized.Contains(TEXT("\"spawn_order\"")));
	TestTrue(TEXT("MP roll serialized"), Serialized.Contains(TEXT("\"mp_roll\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCFrozenAndRootedTest,
	"Wandbound.Core.NPC.Status.FrozenDamageAndRootedMovementUseCoreRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCFrozenAndRootedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData FrozenState = MakeState();
	FrozenState.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	FWBUnitState FrozenTarget = MakePlayerUnit(1, 0, FWBTile(4, 3), 5);
	FrozenTarget.AddStatus(FName(TEXT("Frozen")), 1);
	FrozenState.Units.Add(FrozenTarget);
	uint32 FrozenRNG = 31;
	const FWBNPCPhaseResolutionResult FrozenResult = WBNPCPhaseResolution::ResolvePhase(
		FrozenState,
		MakeRepository(),
		FrozenRNG,
		0);
	TestTrue(TEXT("Frozen phase succeeds"), FrozenResult.bOk);
	TestEqual(TEXT("Frozen prevents attack damage"), FrozenState.GetUnitById(1)->HP, 5);
	TestFalse(TEXT("Frozen removed"), FrozenState.GetUnitById(1)->HasStatus(FName(TEXT("Frozen"))));

	FWBGameStateData RootedState = MakeState();
	FWBUnitState RootedNPC = MakeNPC(10, 0, FWBTile(4, 4));
	RootedNPC.AddStatus(FName(TEXT("Rooted")), 1);
	RootedState.Units.Add(RootedNPC);
	RootedState.Units.Add(MakePlayerUnit(1, 0, FWBTile(4, 2), 5));
	uint32 RootedRNG = 31;
	const FWBNPCPhaseResolutionResult RootedResult = WBNPCPhaseResolution::ResolvePhase(
		RootedState,
		MakeRepository(),
		RootedRNG,
		0);
	TestTrue(TEXT("Rooted phase is normal no-action"), RootedResult.bOk);
	TestEqual(TEXT("Rooted NPC does not move"), RootedState.GetUnitById(10)->Y, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCEquipmentCleanupTest,
	"Wandbound.Core.NPC.Combat.UnitDeathCleansEquipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCEquipmentCleanupTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(4, 4)));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(4, 3), 2));
	FWBEquippedCardEntry Equipped;
	Equipped.Card.InstanceId = TEXT("wand_instance");
	Equipped.Card.CardId = TEXT("wand_card");
	Equipped.Card.OwnerPlayerId = 0;
	Equipped.EquippedToUnitId = 1;
	Equipped.SlotId = TEXT("wand_slot");
	Equipped.EquipOrder = 0;
	State.CardZoneState.EquippedCards.Add(Equipped);
	uint32 RNG = 73;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(State.CardZoneState, 0);
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestFalse(TEXT("Unit removed"), State.GetUnitById(1)->IsUnitOnBoard());
	TestTrue(TEXT("Equipment removed"), State.CardZoneState.EquippedCards.IsEmpty());
	TestNotNull(TEXT("Player zones exist"), Zones);
	if (Zones != nullptr)
	{
		TestTrue(TEXT("Equipment moved to discard"), Zones->Discard.ContainsByPredicate([](const FWBZoneCardEntry& Entry)
		{
			return Entry.Card.InstanceId == TEXT("wand_instance");
		}));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCMultiRetargetTest,
	"Wandbound.Core.NPC.Multiple.EarlierKillChangesLaterTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCMultiRetargetTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeState();
	State.Units.Add(MakeNPC(10, 0, FWBTile(0, 0)));
	State.Units.Add(MakeNPC(20, 1, FWBTile(8, 8), TEXT("npc_beta")));
	State.Units.Add(MakePlayerUnit(1, 0, FWBTile(0, 1), 1));
	State.Units.Add(MakePlayerUnit(2, 1, FWBTile(8, 7), 5));
	uint32 RNG = 81;
	const FWBNPCPhaseResolutionResult Result = WBNPCPhaseResolution::ResolvePhase(State, MakeRepository(), RNG, 0);
	bool bLaterSelectedRemaining = false;
	for (const FWBTraceEvent& Event : Result.TraceEvents)
	{
		bLaterSelectedRemaining |= Event.Kind == FName(TEXT("npc_target_selected"))
			&& Event.SourceUnitId == 20
			&& Event.TargetUnitId == 2;
	}
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestFalse(TEXT("First target removed"), State.GetUnitById(1)->IsUnitOnBoard());
	TestTrue(TEXT("Later NPC retargets current state"), bLaterSelectedRemaining);
	return true;
}

#endif
