#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "WBActionCodec.h"
#include "WBCardZoneObservation.h"
#include "WBMarkerResolution.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardDefinition MakeTrapDefinition()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("basic_trap");
	Definition.PublicName = TEXT("Basic Trap");
	Definition.Kind = EWBCardDefinitionKind::Trap;
	return Definition;
}

FWBCardDefinition MakeNPCDefinition()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("basic_npc");
	Definition.PublicName = TEXT("Basic NPC");
	Definition.Kind = EWBCardDefinitionKind::NPC;
	Definition.CharacterStats.HP = 5;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 3;
	Definition.CharacterStats.RL = 4;
	return Definition;
}

FWBCardDefinitionRepository MakeRepository()
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("marker_test_repository");
	Repository.SourceVersion = TEXT("marker_test_v1");
	Repository.Definitions = { MakeTrapDefinition(), MakeNPCDefinition() };
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 HP = 8)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = HP;
	Unit.ATK = 1;
	Unit.AR = 1;
	Unit.SetCanonicalRL(1, 1, 0);
	Unit.MPRemaining = 4;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBGameStateData MakeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 1;
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBPlayerStateData Player;
		Player.PlayerId = PlayerId;
		Player.HeroUnitId = PlayerId;
		Player.RemainingMP = 4;
		State.Players.Add(Player);

		FWBPlayerCardZoneState Zones;
		Zones.PlayerId = PlayerId;
		State.CardZoneState.PlayerZones.Add(Zones);
	}
	State.Units.Add(MakeUnit(0, 0, FWBTile(4, 8)));
	State.Units.Add(MakeUnit(1, 1, FWBTile(4, 0)));
	return State;
}

FWBSetupMarkerPlacement MakePlacement(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile& Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Placement;
	Placement.PlayerId = PlayerId;
	Placement.Type = Type;
	Placement.Tile = Tile;
	Placement.DefinitionId = Type == EWBMarkerType::Trap ? TEXT("basic_trap") : TEXT("basic_npc");
	Placement.PlacementOrder = Order;
	return Placement;
}

TArray<FWBSetupMarkerPlacement> MakePlacements()
{
	return {
		MakePlacement(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakePlacement(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakePlacement(0, EWBMarkerType::NPC, FWBTile(2, 8), 2),
		MakePlacement(0, EWBMarkerType::NPC, FWBTile(3, 7), 3),
		MakePlacement(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakePlacement(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakePlacement(1, EWBMarkerType::NPC, FWBTile(2, 0), 6),
		MakePlacement(1, EWBMarkerType::NPC, FWBTile(3, 1), 7)
	};
}

bool ApplySetup(FWBGameStateData& State, const FWBCardDefinitionRepository& Repository)
{
	return WBMarkerResolution::ApplyCanonicalSetup(State, Repository, MakePlacements()).bOk;
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

FString MarkerFingerprint(const FWBGameStateData& State)
{
	FString Result;
	for (const FWBMarkerPlaceholderEntry& Marker : State.GetCardZoneState().MarkerPlaceholders)
	{
		Result += FString::Printf(
			TEXT("m%d:p%d:t%d:o%d:%d,%d:%s|"),
			Marker.MarkerId,
			Marker.OwnerPlayerId,
			static_cast<int32>(Marker.Type),
			Marker.PlacementOrder,
			Marker.Tile.X,
			Marker.Tile.Y,
			*Marker.InternalMarkerCardId);
	}
	for (const FWBPendingNPCSpawnState& Pending : State.PendingNPCSpawns)
	{
		Result += FString::Printf(
			TEXT("n%d:o%d:%d,%d:r%d:%s|"),
			Pending.PendingSpawnId,
			Pending.SpawnOrder,
			Pending.OriginTile.X,
			Pending.OriginTile.Y,
			Pending.RetryCount,
			*Pending.NPCDefinitionId);
	}
	for (const FWBUnitState& Unit : State.Units)
	{
		Result += FString::Printf(
			TEXT("u%d:p%d:%d,%d:h%d:a%d|"),
			Unit.UnitId,
			Unit.OwnerId,
			Unit.X,
			Unit.Y,
			Unit.HP,
			Unit.CurrentArmor);
	}
	return Result;
}

FWBMarkerPlaceholderEntry* FindMutableMarker(FWBGameStateData& State, const int32 MarkerId)
{
	return State.GetMutableCardZoneStateForTest().MarkerPlaceholders.FindByPredicate(
		[MarkerId](const FWBMarkerPlaceholderEntry& Marker)
		{
			return Marker.MarkerId == MarkerId;
		});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMarkerCanonicalSetupTest,
	"Wandbound.Core.Marker.Setup.CanonicalCountsStableIdsAndInsertionOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMarkerCanonicalSetupTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData First = MakeState();
	FWBGameStateData Second = MakeState();
	TArray<FWBSetupMarkerPlacement> Reversed = MakePlacements();
	Algo::Reverse(Reversed);
	const FWBMarkerResolutionResult FirstResult =
		WBMarkerResolution::ApplyCanonicalSetup(First, Repository, MakePlacements());
	const FWBMarkerResolutionResult SecondResult =
		WBMarkerResolution::ApplyCanonicalSetup(Second, Repository, Reversed);

	TestTrue(TEXT("Canonical setup succeeds"), FirstResult.bOk);
	TestTrue(TEXT("Reversed input succeeds"), SecondResult.bOk);
	TestEqual(TEXT("Exactly eight markers"), First.GetCardZoneState().MarkerPlaceholders.Num(), 8);
	TestEqual(TEXT("Normalized state is insertion-order independent"), MarkerFingerprint(First), MarkerFingerprint(Second));
	TestEqual(TEXT("Stable first marker ID"), First.GetCardZoneState().MarkerPlaceholders[0].MarkerId, 0);
	TestEqual(TEXT("Stable last marker ID"), First.GetCardZoneState().MarkerPlaceholders[7].MarkerId, 7);
	TestTrue(TEXT("Placement trace emitted"), HasTrace(FirstResult.TraceEvents, FName(TEXT("marker_placed"))));
	TestTrue(TEXT("Completion trace emitted"), HasTrace(FirstResult.TraceEvents, FName(TEXT("marker_setup_completed"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMarkerSetupValidationAtomicTest,
	"Wandbound.Core.Marker.Setup.ValidationFailuresAreAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMarkerSetupValidationAtomicTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	auto ExpectFailure = [this, &Repository](
		const TCHAR* Label,
		TArray<FWBSetupMarkerPlacement> Placements,
		const FString& ExpectedReason)
	{
		FWBGameStateData State = MakeState();
		const FString Before = MarkerFingerprint(State);
		const FWBMarkerResolutionResult Result =
			WBMarkerResolution::ApplyCanonicalSetup(State, Repository, Placements);
		TestFalse(*FString::Printf(TEXT("%s fails"), Label), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s reason"), Label), Result.Reason, ExpectedReason);
		TestEqual(*FString::Printf(TEXT("%s unchanged"), Label), MarkerFingerprint(State), Before);
		TestEqual(*FString::Printf(TEXT("%s no trace"), Label), Result.TraceEvents.Num(), 0);
	};

	TArray<FWBSetupMarkerPlacement> Missing = MakePlacements();
	Missing.Pop();
	ExpectFailure(TEXT("missing"), Missing, TEXT("expected_eight_markers"));

	TArray<FWBSetupMarkerPlacement> Duplicate = MakePlacements();
	Duplicate[1].Tile = Duplicate[0].Tile;
	ExpectFailure(TEXT("duplicate tile"), Duplicate, TEXT("duplicate_marker_tile"));

	TArray<FWBSetupMarkerPlacement> WrongHalf = MakePlacements();
	WrongHalf[0].Tile = FWBTile(0, 0);
	ExpectFailure(TEXT("wrong half"), WrongHalf, TEXT("marker_outside_player_half"));

	TArray<FWBSetupMarkerPlacement> HeroOverlap = MakePlacements();
	HeroOverlap[0].Tile = FWBTile(4, 8);
	ExpectFailure(TEXT("Hero overlap"), HeroOverlap, TEXT("marker_setup_tile_occupied"));

	TArray<FWBSetupMarkerPlacement> Distribution = MakePlacements();
	Distribution[2].Type = EWBMarkerType::Trap;
	Distribution[2].DefinitionId = TEXT("basic_trap");
	ExpectFailure(TEXT("distribution"), Distribution, TEXT("invalid_marker_type_distribution"));

	TArray<FWBSetupMarkerPlacement> MissingDefinition = MakePlacements();
	MissingDefinition[0].DefinitionId = TEXT("missing_trap");
	ExpectFailure(TEXT("definition"), MissingDefinition, TEXT("marker_definition_not_found"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMarkerHiddenObservationTest,
	"Wandbound.Core.Marker.Observation.TypesAndDefinitionsRemainHidden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMarkerHiddenObservationTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(0)->MPRemaining = 2;
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	const TArray<FString> LegalWithMarkers = WBActionCodec::MakeActionIds(
		WBRules::GenerateLegalActionsForPlayer(State, 0));

	FWBGameStateData WithoutMarkers = State;
	WithoutMarkers.GetMutableCardZoneStateForTest().MarkerPlaceholders.Reset();
	const TArray<FString> LegalWithoutMarkers = WBActionCodec::MakeActionIds(
		WBRules::GenerateLegalActionsForPlayer(WithoutMarkers, 0));
	const FWBCardZonePlayerObservation Owner = WBCardZoneObservation::BuildObservationForPlayer(State, 0);
	const FWBCardZonePlayerObservation Opponent = WBCardZoneObservation::BuildObservationForPlayer(State, 1);

	TestEqual(TEXT("Authoritative state has marker type"), State.GetCardZoneState().MarkerPlaceholders[0].Type, EWBMarkerType::Trap);
	TestEqual(TEXT("Owner sees concealed presence only"), Owner.PublicSummary.Markers.Num(), 8);
	TestEqual(TEXT("Opponent sees concealed presence only"), Opponent.PublicSummary.Markers.Num(), 8);
	TestFalse(TEXT("Owner cannot see Trap identity"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Owner, TEXT("basic_trap")));
	TestFalse(TEXT("Opponent cannot see NPC identity"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Opponent, TEXT("basic_npc")));
	TestEqual(TEXT("Markers do not change legal action count"), LegalWithMarkers.Num(), LegalWithoutMarkers.Num());
	for (int32 Index = 0; Index < FMath::Min(LegalWithMarkers.Num(), LegalWithoutMarkers.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Legal action %d unchanged"), Index), LegalWithMarkers[Index], LegalWithoutMarkers[Index]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBTrapDamageResolutionTest,
	"Wandbound.Core.Marker.Trap.ArmorDamageRemovalAndSingleTrigger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTrapDamageResolutionTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	FWBUnitState* Unit = State.GetMutableUnitById(0);
	Unit->X = 0;
	Unit->Y = 8;
	Unit->HP = 5;
	Unit->MaxHP = 5;
	Unit->SetArmorForTest(1, 1);

	const FWBMarkerResolutionResult First =
		WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0);
	const FWBMarkerResolutionResult Second =
		WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0);
	TestTrue(TEXT("Trap resolves"), First.bOk);
	TestTrue(TEXT("Trap damage flagged"), First.bTrapDamageApplied);
	TestEqual(TEXT("Armor absorbs one"), State.GetUnitById(0)->CurrentArmor, 0);
	TestEqual(TEXT("One HP damage remains"), State.GetUnitById(0)->HP, 4);
	TestEqual(TEXT("Marker consumed"), State.GetCardZoneState().MarkerPlaceholders.Num(), 7);
	TestTrue(TEXT("Trap damage trace emitted"), HasTrace(First.TraceEvents, FName(TEXT("trap_damage_resolved"))));
	TestTrue(TEXT("Second pass succeeds with no marker"), Second.bOk);
	TestFalse(TEXT("Marker cannot trigger twice"), Second.bMarkerFound);
	TestEqual(TEXT("HP unchanged on second pass"), State.GetUnitById(0)->HP, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBTrapHeroDeathCleanupTest,
	"Wandbound.Core.Marker.Trap.HeroDeathEquipmentCleanupAndGameOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBTrapHeroDeathCleanupTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	FWBUnitState* Hero = State.GetMutableUnitById(0);
	Hero->X = 0;
	Hero->Y = 8;
	Hero->HP = 2;
	Hero->SetArmorForTest(0, 0);

	FWBEquippedCardEntry Equipped;
	Equipped.Card.InstanceId = TEXT("wand_instance");
	Equipped.Card.CardId = TEXT("wand_card");
	Equipped.Card.OwnerPlayerId = 0;
	Equipped.EquippedToUnitId = 0;
	Equipped.SlotId = TEXT("wand");
	Equipped.EquipOrder = 0;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Equipped);

	const FWBMarkerResolutionResult Result =
		WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0);
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), 0);
	TestTrue(TEXT("Trap resolves"), Result.bOk);
	TestTrue(TEXT("Hero defeated"), Result.bUnitDefeated);
	TestTrue(TEXT("Hero removed"), State.GetUnitById(0)->bRemovedFromBoard);
	TestTrue(TEXT("Match terminal"), State.bGameOver);
	TestEqual(TEXT("Opponent wins"), State.WinnerPlayerId, 1);
	TestEqual(TEXT("Equipment removed"), State.GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("Equipment discarded"), Zones->Discard.Num(), 1);
	TestTrue(TEXT("Marker death traced"), HasTrace(Result.TraceEvents, FName(TEXT("marker_triggered_death"))));
	TestTrue(TEXT("Marker game over traced"), HasTrace(Result.TraceEvents, FName(TEXT("marker_triggered_game_over"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCMarkerScheduleTest,
	"Wandbound.Core.Marker.NPC.SchedulesStablePendingWithoutImmediateSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCMarkerScheduleTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	State.GetMutableUnitById(0)->X = 2;
	State.GetMutableUnitById(0)->Y = 8;
	const int32 UnitsBefore = State.Units.Num();

	const FWBMarkerResolutionResult Result =
		WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0);
	TestTrue(TEXT("NPC marker resolves"), Result.bOk);
	TestTrue(TEXT("Spawn scheduled"), Result.bNPCSpawnScheduled);
	TestEqual(TEXT("One pending spawn"), State.PendingNPCSpawns.Num(), 1);
	TestEqual(TEXT("Stable pending ID"), State.PendingNPCSpawns[0].PendingSpawnId, 0);
	TestEqual(TEXT("Stable spawn order"), State.PendingNPCSpawns[0].SpawnOrder, 0);
	TestEqual(TEXT("NPC not spawned immediately"), State.Units.Num(), UnitsBefore);
	TestEqual(TEXT("Marker consumed"), State.GetCardZoneState().MarkerPlaceholders.Num(), 7);
	TestTrue(TEXT("Schedule traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_spawn_scheduled"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCPhaseSpawnTest,
	"Wandbound.Core.Marker.NPC.PhaseSpawnsNeutralWithCanonicalTileAndStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCPhaseSpawnTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	State.GetMutableUnitById(0)->X = 2;
	State.GetMutableUnitById(0)->Y = 8;
	TestTrue(TEXT("NPC marker schedules"), WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0).bOk);

	const FWBNPCPhaseResult Result =
		WBMarkerResolution::ProcessPendingNPCSpawns(State, Repository, 0);
	const FWBUnitState* NPC = State.GetUnitById(2);
	TestTrue(TEXT("NPC phase succeeds"), Result.bOk);
	TestEqual(TEXT("One NPC spawned"), Result.SpawnedCount, 1);
	TestNotNull(TEXT("NPC exists"), NPC);
	if (NPC == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("East candidate selected first"), FWBTile(NPC->X, NPC->Y), FWBTile(3, 8));
	TestEqual(TEXT("NPC is neutral"), NPC->OwnerId, -1);
	TestEqual(TEXT("Definition HP"), NPC->HP, 5);
	TestEqual(TEXT("Definition ATK"), NPC->ATK, 2);
	TestEqual(TEXT("Definition AR"), NPC->AR, 3);
	TestEqual(TEXT("Definition RL"), NPC->GetCurrentRLForRules(), 4);
	TestEqual(TEXT("NPC has no actions this pass"), NPC->AttacksLeft, 0);
	TestTrue(TEXT("NPC occupies spawn tile"), State.IsTileOccupied(FWBTile(3, 8)));
	TestEqual(TEXT("Pending consumed"), State.PendingNPCSpawns.Num(), 0);
	TestTrue(TEXT("Spawn traced"), HasTrace(Result.TraceEvents, FName(TEXT("npc_spawn_succeeded"))));
	TestFalse(TEXT("Retired action deferral is absent"), HasTrace(Result.TraceEvents, FName(TEXT("npc_action_deferred"))));
	TestEqual(TEXT("Spawn order retained on unit"), NPC->NPCSpawnOrder, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCBlockedRetryTest,
	"Wandbound.Core.Marker.NPC.BlockedSpawnPersistsAndRetries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCBlockedRetryTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	State.GetMutableUnitById(0)->X = 2;
	State.GetMutableUnitById(0)->Y = 8;
	TestTrue(TEXT("NPC marker schedules"), WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0).bOk);
	State.Units.Add(MakeUnit(10, -1, FWBTile(3, 8)));
	State.Units.Add(MakeUnit(11, -1, FWBTile(1, 8)));
	State.Units.Add(MakeUnit(12, -1, FWBTile(2, 7)));

	const FWBNPCPhaseResult Blocked =
		WBMarkerResolution::ProcessPendingNPCSpawns(State, Repository, 0);
	TestTrue(TEXT("Blocked phase succeeds"), Blocked.bOk);
	TestEqual(TEXT("One spawn blocked"), Blocked.BlockedCount, 1);
	TestEqual(TEXT("Pending retained"), State.PendingNPCSpawns.Num(), 1);
	TestEqual(TEXT("Retry count increments"), State.PendingNPCSpawns[0].RetryCount, 1);
	TestTrue(TEXT("Blocked trace emitted"), HasTrace(Blocked.TraceEvents, FName(TEXT("npc_spawn_blocked"))));

	State.GetMutableUnitById(10)->RemoveUnitFromBoard();
	const FWBNPCPhaseResult Retried =
		WBMarkerResolution::ProcessPendingNPCSpawns(State, Repository, 1);
	TestTrue(TEXT("Retry succeeds"), Retried.bOk);
	TestEqual(TEXT("Pending consumed after retry"), State.PendingNPCSpawns.Num(), 0);
	TestEqual(TEXT("NPC uses newly open east tile"), State.UnitIdAt(FWBTile(3, 8)), 13);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCPendingOrderDeterminismTest,
	"Wandbound.Core.Marker.NPC.PendingInsertionOrderDoesNotChangeSpawnOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCPendingOrderDeterminismTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData First = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(First, Repository));
	FWBPendingNPCSpawnState A;
	A.PendingSpawnId = 20;
	A.SourceMarkerId = 2;
	A.MarkerOwnerPlayerId = 0;
	A.NPCDefinitionId = TEXT("basic_npc");
	A.OriginTile = FWBTile(4, 4);
	A.SpawnOrder = 0;
	A.CreatedTurnNumber = 1;
	FWBPendingNPCSpawnState B = A;
	B.PendingSpawnId = 10;
	B.SourceMarkerId = 6;
	B.MarkerOwnerPlayerId = 1;
	B.OriginTile = FWBTile(7, 7);
	B.SpawnOrder = 1;
	First.PendingNPCSpawns = { A, B };
	FWBGameStateData Second = First;
	Algo::Reverse(Second.PendingNPCSpawns);

	const FWBNPCPhaseResult FirstResult = WBMarkerResolution::ProcessPendingNPCSpawns(First, Repository, 0);
	const FWBNPCPhaseResult SecondResult = WBMarkerResolution::ProcessPendingNPCSpawns(Second, Repository, 0);
	TestTrue(TEXT("First phase succeeds"), FirstResult.bOk);
	TestTrue(TEXT("Second phase succeeds"), SecondResult.bOk);
	TestEqual(TEXT("Normalized state matches"), MarkerFingerprint(First), MarkerFingerprint(Second));
	TestEqual(TEXT("Trace order matches"), WBReplayTrace::SerializeEvents(FirstResult.TraceEvents), WBReplayTrace::SerializeEvents(SecondResult.TraceEvents));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBNPCBlockedDoesNotStopLaterTest,
	"Wandbound.Core.Marker.NPC.BlockedSpawnDoesNotStopLaterPendingSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBNPCBlockedDoesNotStopLaterTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	FWBPendingNPCSpawnState Blocked;
	Blocked.PendingSpawnId = 0;
	Blocked.SourceMarkerId = 2;
	Blocked.MarkerOwnerPlayerId = 0;
	Blocked.NPCDefinitionId = TEXT("basic_npc");
	Blocked.OriginTile = FWBTile(2, 8);
	Blocked.SpawnOrder = 0;
	Blocked.CreatedTurnNumber = 1;
	FWBPendingNPCSpawnState Open = Blocked;
	Open.PendingSpawnId = 1;
	Open.SourceMarkerId = 6;
	Open.MarkerOwnerPlayerId = 1;
	Open.OriginTile = FWBTile(7, 7);
	Open.SpawnOrder = 1;
	State.PendingNPCSpawns = { Blocked, Open };
	State.Units.Add(MakeUnit(10, -1, FWBTile(3, 8)));
	State.Units.Add(MakeUnit(11, -1, FWBTile(1, 8)));
	State.Units.Add(MakeUnit(12, -1, FWBTile(2, 7)));

	const FWBNPCPhaseResult Result = WBMarkerResolution::ProcessPendingNPCSpawns(State, Repository, 0);
	TestTrue(TEXT("Phase succeeds"), Result.bOk);
	TestEqual(TEXT("One blocked"), Result.BlockedCount, 1);
	TestEqual(TEXT("Later spawn succeeds"), Result.SpawnedCount, 1);
	TestEqual(TEXT("Only blocked record remains"), State.PendingNPCSpawns.Num(), 1);
	TestEqual(TEXT("Blocked record retained"), State.PendingNPCSpawns[0].PendingSpawnId, 0);
	TestTrue(TEXT("Later NPC exists"), State.IsTileOccupied(FWBTile(8, 7)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMarkerMalformedStateAtomicTest,
	"Wandbound.Core.Marker.Atomic.MalformedMarkerAndPendingFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMarkerMalformedStateAtomicTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Duplicate = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(Duplicate, Repository));
	Duplicate.GetMutableCardZoneStateForTest().MarkerPlaceholders[1].Tile =
		Duplicate.GetCardZoneState().MarkerPlaceholders[0].Tile;
	Duplicate.GetMutableUnitById(0)->X = 0;
	Duplicate.GetMutableUnitById(0)->Y = 8;
	const FString DuplicateBefore = MarkerFingerprint(Duplicate);
	const FWBMarkerResolutionResult DuplicateResult =
		WBMarkerResolution::ResolveMarkerAtUnitTile(Duplicate, Repository, 0);
	TestFalse(TEXT("Duplicate marker state fails"), DuplicateResult.bOk);
	TestTrue(TEXT("Duplicate reason explicit"), DuplicateResult.Reason.Contains(TEXT("duplicate_marker_tile")));
	TestEqual(TEXT("Duplicate state unchanged"), MarkerFingerprint(Duplicate), DuplicateBefore);

	FWBGameStateData MissingDefinition = MakeState();
	TestTrue(TEXT("Second setup succeeds"), ApplySetup(MissingDefinition, Repository));
	FWBPendingNPCSpawnState Pending;
	Pending.PendingSpawnId = 0;
	Pending.SourceMarkerId = 2;
	Pending.MarkerOwnerPlayerId = 0;
	Pending.NPCDefinitionId = TEXT("missing_npc");
	Pending.OriginTile = FWBTile(2, 8);
	Pending.SpawnOrder = 0;
	Pending.CreatedTurnNumber = 1;
	MissingDefinition.PendingNPCSpawns.Add(Pending);
	const FString PendingBefore = MarkerFingerprint(MissingDefinition);
	const FWBNPCPhaseResult PendingResult =
		WBMarkerResolution::ProcessPendingNPCSpawns(MissingDefinition, Repository, 0);
	TestFalse(TEXT("Missing definition fails"), PendingResult.bOk);
	TestEqual(TEXT("Missing definition reason"), PendingResult.Reason, FString(TEXT("npc_definition_not_found")));
	TestEqual(TEXT("Pending state unchanged"), MarkerFingerprint(MissingDefinition), PendingBefore);
	TestEqual(TEXT("Failed phase emits no trace"), PendingResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBMarkerTraceSerializationTest,
	"Wandbound.Core.Marker.Replay.TraceSerializationIncludesStableMarkerFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBMarkerTraceSerializationTest::RunTest(const FString& Parameters)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestTrue(TEXT("Setup succeeds"), ApplySetup(State, Repository));
	State.GetMutableUnitById(0)->X = 2;
	State.GetMutableUnitById(0)->Y = 8;
	const FWBMarkerResolutionResult Result =
		WBMarkerResolution::ResolveMarkerAtUnitTile(State, Repository, 0);
	const FString Serialized = WBReplayTrace::SerializeEvents(Result.TraceEvents);
	TestTrue(TEXT("Marker ID field serialized"), Serialized.Contains(TEXT("\"marker_id\"")));
	TestTrue(TEXT("Marker type field serialized"), Serialized.Contains(TEXT("\"marker_type\"")));
	TestTrue(TEXT("Pending ID field serialized"), Serialized.Contains(TEXT("\"pending_spawn_id\"")));
	TestTrue(TEXT("Spawn order field serialized"), Serialized.Contains(TEXT("\"spawn_order\"")));
	const FWBTraceEvent* Scheduled = Result.TraceEvents.FindByPredicate([](const FWBTraceEvent& Event)
	{
		return Event.Kind == FName(TEXT("npc_spawn_scheduled"));
	});
	TestNotNull(TEXT("Scheduled event present"), Scheduled);
	if (Scheduled != nullptr)
	{
		TestEqual(TEXT("Typed marker ID stable"), Scheduled->MarkerId, 2);
		TestEqual(TEXT("Typed marker type stable"), Scheduled->MarkerType, FName(TEXT("NPC")));
		TestEqual(TEXT("Typed pending ID stable"), Scheduled->PendingSpawnId, 0);
		TestEqual(TEXT("Typed spawn order stable"), Scheduled->SpawnOrder, 0);
	}
	TestFalse(TEXT("Definition remains hidden before spawn"), Serialized.Contains(TEXT("basic_npc")));
	return true;
}

#endif
