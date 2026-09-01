#include "Misc/AutomationTest.h"

#include "WBActionCodec.h"
#include "WBBoardGeometry.h"
#include "WBCardDefinitionRepository.h"
#include "WBEffectRequest.h"
#include "WBEffectRunner.h"
#include "WBNPCPhaseResolution.h"
#include "WBProductionMatchReplay.h"
#include "WBPublicBoardSummary.h"
#include "WBRules.h"
#include "WBTerrainRules.h"
#include "WBUnitStatQuery.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const FWBGridGeometryProfile& Movement,
	const FWBGridGeometryProfile& Attack,
	const int32 AR = 3,
	const bool bAura = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 10;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = AR;
	Definition.CharacterStats.RL = 1;
	Definition.MovementGeometry = Movement;
	Definition.AttackGeometry = Attack;
	if (bAura)
	{
		FWBContinuousStatAuraDefinition Aura;
		Aura.AuraId = TEXT("test_ar_aura");
		Aura.TargetRelation = EWBContinuousAuraTargetRelation::Enemy;
		Aura.TargetStat = EWBContinuousStat::AR;
		Aura.Operation = EWBContinuousStatOperation::Add;
		Aura.Amount = -1;
		Aura.RangeStat = EWBContinuousAuraRangeStat::AR;
		Aura.Geometry = EWBContinuousAuraGeometry::AttackLine;
		Aura.bBlockedByWalls = true;
		Aura.bBlockedByUnits = true;
		Aura.MinimumResult = 0;
		Definition.ContinuousStatAuras.Add(Aura);
	}
	return Definition;
}

FWBCardDefinitionRepository MakeRepository()
{
	const FWBGridGeometryProfile Orthogonal =
		FWBGridGeometryProfile::OrthogonalOnly();
	const FWBGridGeometryProfile Diagonal =
		FWBGridGeometryProfile::DiagonalOnly();
	const FWBGridGeometryProfile Both =
		FWBGridGeometryProfile::OrthogonalAndDiagonal();
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("orth"), Orthogonal, Orthogonal));
	Definitions.Add(MakeCharacter(TEXT("diag"), Diagonal, Diagonal));
	Definitions.Add(MakeCharacter(TEXT("both"), Both, Both));
	Definitions.Add(MakeCharacter(TEXT("diag_ar2"), Diagonal, Diagonal, 2));
	Definitions.Add(MakeCharacter(TEXT("diag_aura"), Diagonal, Diagonal, 3, true));
	Definitions.Add(MakeCharacter(TEXT("target"), Orthogonal, Orthogonal, 3));
	FWBCardDefinition DiagonalNPC =
		MakeCharacter(TEXT("diag_npc"), Diagonal, Diagonal, 2);
	DiagonalNPC.Kind = EWBCardDefinitionKind::NPC;
	Definitions.Add(DiagonalNPC);
	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("geometry_highground_tests"), TEXT("v1"),
		Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile,
	const int32 AR = 3,
	const int32 MP = 6)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 10;
	Unit.MaxHP = 10;
	Unit.ATK = 2;
	Unit.AR = AR;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.MPRemaining = MP;
	Unit.SetCanonicalRL(1, 1, 0);
	return Unit;
}

FWBGameStateData MakeState(
	const FString& SourceCardId = TEXT("diag"),
	const FWBTile& SourceTile = FWBTile(2, 2),
	const int32 SourceOwner = 0,
	const int32 SourceAR = 3,
	const int32 MP = 6)
{
	FWBGameStateData State;
	State.CurrentPlayer = SourceOwner >= 0 ? SourceOwner : 0;
	State.PriorityPlayer = State.CurrentPlayer;
	State.FirstPlayerId = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.RemainingMP = MP;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.RemainingMP = MP;
	State.Players = { Player0, Player1 };
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	State.CardZoneState.PlayerZones = { Zones0, Zones1 };
	State.AddUnitForTest(MakeUnit(
		1, SourceOwner, SourceCardId, SourceTile, SourceAR, MP));
	return State;
}

FWBAction Move(
	const FWBTile& From,
	const FWBTile& To,
	const int32 PlayerId = 0,
	const int32 UnitId = 1)
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = UnitId;
	Action.FromTile = From;
	Action.ToTile = To;
	return Action;
}

FWBAction Attack(
	const int32 SourceUnitId = 1,
	const int32 TargetUnitId = 2,
	const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = SourceUnitId;
	Action.TargetUnitId = TargetUnitId;
	return Action;
}

void AddTarget(
	FWBGameStateData& State,
	const FWBTile& Tile,
	const int32 UnitId = 2,
	const int32 OwnerId = 1,
	const FString& CardId = TEXT("target"),
	const int32 AR = 3)
{
	State.AddUnitForTest(MakeUnit(UnitId, OwnerId, CardId, Tile, AR));
}

void AddWall(FWBGameStateData& State, const FWBTile& A, const FWBTile& B)
{
	State.Walls.Add(FWBWallEdge(A, B));
}

bool ContainsMoveTo(const TArray<FWBAction>& Actions, const FWBTile& Tile)
{
	return Actions.ContainsByPredicate([&Tile](const FWBAction& Action)
	{
		return Action.Type == EWBActionType::Move && Action.ToTile == Tile;
	});
}

bool ContainsAttackTarget(const TArray<FWBAction>& Actions, const int32 TargetId)
{
	return Actions.ContainsByPredicate([TargetId](const FWBAction& Action)
	{
		return Action.Type == EWBActionType::Attack
			&& Action.TargetUnitId == TargetId;
	});
}

FWBEffectRequest MakeTerrainRequest(
	const FWBTile& TargetTile,
	const FName TerrainId = FName(TEXT("highground")))
{
	FWBEffectRequest Request;
	Request.Source.PlayerId = 0;
	Request.Source.SourceUnitId = 1;
	Request.Source.SourceCardId = TEXT("orth");
	Request.Source.SourceEffectId = TEXT("survey");
	Request.Target.TargetTile = TargetTile;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::SetTerrain;
	Payload.SetTerrainEffect.TerrainId = TerrainId;
	Payload.SetTerrainEffect.RangeMetric = EWBEffectTileRangeMetric::Manhattan;
	Payload.SetTerrainEffect.RangeStat = EWBEffectRangeStat::AR;
	Payload.SetTerrainEffect.bAllowOccupied = true;
	Payload.SetTerrainEffect.bRequireLineOfSight = false;
	Request.Payloads.Add(Payload);
	return Request;
}
}

#define WB_GEOMETRY_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_GEOMETRY_TEST(FWBGeometryPrimitivesTest,
	"Wandbound.Geometry.Primitives.OrthogonalDiagonalAndDistance")
bool FWBGeometryPrimitivesTest::RunTest(const FString&)
{
	TestTrue(TEXT("Exact diagonal adjacency"),
		WBBoardGeometry::AreDiagonallyAdjacent(FWBTile(2, 2), FWBTile(3, 3)));
	TestFalse(TEXT("Two-tile diagonal is not adjacent"),
		WBBoardGeometry::AreDiagonallyAdjacent(FWBTile(2, 2), FWBTile(4, 4)));
	TestTrue(TEXT("Orthogonal adjacency unchanged"),
		WBBoardGeometry::AreOrthogonallyAdjacent(FWBTile(2, 2), FWBTile(2, 3)));
	TestFalse(TEXT("Diagonal is not orthogonally adjacent"),
		WBBoardGeometry::AreOrthogonallyAdjacent(FWBTile(2, 2), FWBTile(3, 3)));
	for (const FWBTile Target : {
		FWBTile(4, 4), FWBTile(0, 4), FWBTile(4, 0), FWBTile(0, 0) })
	{
		TestTrue(TEXT("All four diagonal directions align"),
			WBBoardGeometry::AreDiagonallyAligned(FWBTile(2, 2), Target));
		TestEqual(TEXT("Diagonal distance counts steps"),
			WBBoardGeometry::DiagonalDistance(FWBTile(2, 2), Target), 2);
	}
	return true;
}

WB_GEOMETRY_TEST(FWBDiagonalWallRoutesTest,
	"Wandbound.Geometry.DiagonalWalls.TwoRouteCanon")
bool FWBDiagonalWallRoutesTest::RunTest(const FString&)
{
	const FWBTile A(2, 2);
	const FWBTile B(3, 2);
	const FWBTile C(2, 3);
	const FWBTile D(3, 3);
	FWBGameStateData State;
	AddWall(State, A, C);
	TestFalse(TEXT("One wall leaves the other route open"),
		WBBoardGeometry::IsDiagonalStepBlockedByWalls(State, A, D));
	AddWall(State, B, D);
	TestTrue(TEXT("A-C and B-D block both routes"),
		WBBoardGeometry::IsDiagonalStepBlockedByWalls(State, A, D));
	State.Walls.Reset();
	AddWall(State, A, B);
	AddWall(State, C, D);
	TestTrue(TEXT("A-B and C-D block both routes"),
		WBBoardGeometry::IsDiagonalStepBlockedByWalls(State, A, D));
	State.Walls.Reset();
	AddWall(State, A, B);
	AddWall(State, A, C);
	TestTrue(TEXT("Both source exits blocked blocks diagonal"),
		WBBoardGeometry::IsDiagonalStepBlockedByWalls(State, A, D));
	State.Walls.Reset();
	AddWall(State, A, B);
	AddWall(State, B, D);
	TestFalse(TEXT("Two walls on one route leave other route open"),
		WBBoardGeometry::IsDiagonalStepBlockedByWalls(State, A, D));
	return true;
}

WB_GEOMETRY_TEST(FWBDiagonalMovementProfilesTest,
	"Wandbound.Geometry.Movement.DefinitionDrivenProfilesAndGeneration")
bool FWBDiagonalMovementProfilesTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("diag"));
	TestTrue(TEXT("Diagonal-only unit moves diagonally"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).bOk);
	TestFalse(TEXT("Diagonal-only unit cannot move orthogonally"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 2))).bOk);
	TArray<FWBAction> Generated = WBRules::GenerateLegalMoveActions(
		State, Repository, 0, 1);
	TestTrue(TEXT("Diagonal generated"), ContainsMoveTo(Generated, FWBTile(3, 3)));
	TestFalse(TEXT("Illegal orthogonal omitted"), ContainsMoveTo(Generated, FWBTile(3, 2)));
	State.GetMutableUnitById(1)->CardId = TEXT("orth");
	TestFalse(TEXT("Orthogonal-only unit cannot move diagonally"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).bOk);
	State.GetMutableUnitById(1)->CardId = TEXT("both");
	Generated = WBRules::GenerateLegalMoveActions(State, Repository, 0, 1);
	TestTrue(TEXT("Both profile generates orthogonal"),
		ContainsMoveTo(Generated, FWBTile(3, 2)));
	TestTrue(TEXT("Both profile generates diagonal"),
		ContainsMoveTo(Generated, FWBTile(3, 3)));
	return true;
}

WB_GEOMETRY_TEST(FWBDiagonalMovementGuardsTest,
	"Wandbound.Geometry.Movement.SharedGuards")
bool FWBDiagonalMovementGuardsTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("diag"));
	AddTarget(State, FWBTile(3, 3));
	TestEqual(TEXT("Destination occupancy blocks"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).Reason,
		FString(TEXT("tile_occupied")));
	State.GetMutableUnitById(2)->RemoveUnitFromBoard();
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Rooted")), 1);
	TestFalse(TEXT("Rooted blocks diagonal movement"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).bOk);
	State.GetMutableUnitById(1)->RemoveStatus(FName(TEXT("Rooted")));
	State.GetMutableUnitById(1)->AddStatus(FName(TEXT("Stunned")), 1);
	TestFalse(TEXT("Stunned blocks diagonal movement"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).bOk);
	State.GetMutableUnitById(1)->RemoveStatus(FName(TEXT("Stunned")));
	State.GetMutablePlayerById(0)->RemainingMP = 0;
	TestFalse(TEXT("Insufficient MP blocks"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).bOk);
	State = MakeState(TEXT("diag"), FWBTile(4, 5));
	State.TurnNumber = 1;
	TestEqual(TEXT("Turn-one boundary applies to diagonal movement"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(4, 5), FWBTile(3, 4))).Reason,
		FString(TEXT("first_player_turn_one_protected_boundary_crossing")));
	return true;
}

WB_GEOMETRY_TEST(FWBHighgroundMovementCostTest,
	"Wandbound.Highground.Movement.DestinationCostAndMutation")
bool FWBHighgroundMovementCostTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("both"));
	TestEqual(TEXT("Normal to normal costs one"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 2))).CostMP, 1);
	State.SetTerrainForTest(FWBTile(3, 2), FName(TEXT("highground")));
	TestEqual(TEXT("Normal to Highground costs two"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 2))).CostMP, 2);
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	TestEqual(TEXT("Highground to Highground costs two"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 2))).CostMP, 2);
	State.SetTerrainForTest(FWBTile(3, 2), FName(TEXT("normal")));
	TestEqual(TEXT("Highground to normal costs one"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 2))).CostMP, 1);
	State.SetTerrainForTest(FWBTile(3, 3), FName(TEXT("highground")));
	TestEqual(TEXT("Diagonal into Highground costs two"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).CostMP, 2);
	State.GetMutablePlayerById(0)->RemainingMP = 1;
	TestEqual(TEXT("One MP cannot enter Highground"),
		WBRules::QueryMove(State, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3))).Reason,
		FString(TEXT("insufficient_mp")));
	State.GetMutablePlayerById(0)->RemainingMP = 6;
	const FWBApplyActionResult Applied = WBEffectRunner::ApplyMove(
		State, Repository, Move(FWBTile(2, 2), FWBTile(3, 3)));
	TestTrue(TEXT("Highground move applies"), Applied.bOk);
	TestEqual(TEXT("Exact movement cost spent"),
		State.GetPlayerById(0)->RemainingMP, 4);
	return true;
}

WB_GEOMETRY_TEST(FWBDiagonalAttackAuthorityTest,
	"Wandbound.Geometry.Attack.DiagonalRangeWallsAndUnits")
bool FWBDiagonalAttackAuthorityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("diag_ar2"), FWBTile(2, 2), 0, 2);
	AddTarget(State, FWBTile(4, 4));
	TestTrue(TEXT("Diagonal target at effective AR is legal"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).bOk);
	State.GetMutableUnitById(2)->X = 2;
	TestFalse(TEXT("Diagonal-only attacker rejects orthogonal target"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).bOk);
	State.GetMutableUnitById(2)->X = 5;
	State.GetMutableUnitById(2)->Y = 5;
	TestEqual(TEXT("Diagonal distance respects AR"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).Reason,
		FString(TEXT("out_of_range")));
	State.GetMutableUnitById(2)->X = 4;
	State.GetMutableUnitById(2)->Y = 4;
	AddTarget(State, FWBTile(3, 3), 3, 1);
	TestEqual(TEXT("Intermediate diagonal unit blocks"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).Reason,
		FString(TEXT("blocked_by_unit")));
	State.GetMutableUnitById(3)->X = 3;
	State.GetMutableUnitById(3)->Y = 2;
	AddTarget(State, FWBTile(2, 3), 4, 1);
	TestTrue(TEXT("Side-tile units do not block"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).bOk);
	State.Walls.Reset();
	AddWall(State, FWBTile(2, 2), FWBTile(3, 2));
	AddWall(State, FWBTile(2, 2), FWBTile(2, 3));
	TestEqual(TEXT("Both diagonal routes blocked rejects attack"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).Reason,
		FString(TEXT("blocked_by_wall")));
	State.Walls.Reset();
	AddWall(State, FWBTile(3, 3), FWBTile(4, 3));
	AddWall(State, FWBTile(3, 3), FWBTile(3, 4));
	TestEqual(TEXT("Every multi-step diagonal edge is checked"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).Reason,
		FString(TEXT("blocked_by_wall")));
	const TArray<FWBAction> Actions = WBRules::GenerateLegalActionsForPlayer(
		State, Repository, 0);
	TestFalse(TEXT("Blocked diagonal attack is not generated"),
		ContainsAttackTarget(Actions, 2));
	return true;
}

WB_GEOMETRY_TEST(FWBHighgroundAttackAuthorityTest,
	"Wandbound.Highground.Attack.WallBypassGeometryAndUnits")
bool FWBHighgroundAttackAuthorityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("orth"), FWBTile(2, 2), 0, 2);
	AddTarget(State, FWBTile(2, 5));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	AddWall(State, FWBTile(2, 3), FWBTile(2, 4));
	TestTrue(TEXT("Orthogonal Highground attack ignores wall and gains AR"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).bOk);
	AddTarget(State, FWBTile(2, 4), 3, 1);
	TestEqual(TEXT("Highground attack still blocked by intervening unit"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).Reason,
		FString(TEXT("blocked_by_unit")));
	State.GetMutableUnitById(3)->RemoveUnitFromBoard();
	State.GetMutableUnitById(1)->CardId = TEXT("diag_ar2");
	State.GetMutableUnitById(2)->X = 5;
	State.GetMutableUnitById(2)->Y = 5;
	State.Walls.Reset();
	AddWall(State, FWBTile(2, 2), FWBTile(3, 2));
	AddWall(State, FWBTile(2, 2), FWBTile(2, 3));
	TestTrue(TEXT("Diagonal Highground attack ignores diagonal wall pair"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).bOk);
	State.GetMutableUnitById(2)->X = 2;
	State.GetMutableUnitById(2)->Y = 5;
	TestFalse(TEXT("Highground does not grant orthogonal geometry"),
		WBRules::CanDeclareAttack(State, Repository, Attack()).bOk);
	return true;
}

WB_GEOMETRY_TEST(FWBGeometryRedirectCounterNPCTest,
	"Wandbound.Geometry.Combat.RedirectCounterAndNPC")
bool FWBGeometryRedirectCounterNPCTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Redirect = MakeState(TEXT("diag"));
	AddTarget(Redirect, FWBTile(2, 4));
	AddTarget(Redirect, FWBTile(5, 5), 3, 1);
	Redirect.PendingAttack.bActive = true;
	Redirect.PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	Redirect.PendingAttack.AttackerUnitId = 1;
	Redirect.PendingAttack.DefenderUnitId = 2;
	Redirect.PendingAttack.ContinuationId = TEXT("geometry_attack");
	TestTrue(TEXT("Redirect revalidates diagonal geometry"),
		WBRules::CanRedirectPendingAttack(
			Redirect, Repository, TEXT("geometry_attack"), 3).bOk);
	Redirect.GetMutableUnitById(3)->X = 2;
	TestFalse(TEXT("Redirect cannot bypass diagonal-only geometry"),
		WBRules::CanRedirectPendingAttack(
			Redirect, Repository, TEXT("geometry_attack"), 3).bOk);

	FWBGameStateData Counter = MakeState(TEXT("orth"), FWBTile(2, 2), 0, 3);
	AddTarget(Counter, FWBTile(4, 4), 2, 1, TEXT("diag_ar2"), 2);
	Counter.PendingAttack.bActive = true;
	Counter.PendingAttack.AttackerUnitId = 1;
	Counter.PendingAttack.DefenderUnitId = 2;
	TestTrue(TEXT("Counter uses defender diagonal geometry"),
		WBRules::CanResolveCounterattack(Counter, Repository).bOk);
	Counter.SetTerrainForTest(FWBTile(4, 4), FName(TEXT("highground")));
	AddWall(Counter, FWBTile(4, 4), FWBTile(3, 4));
	AddWall(Counter, FWBTile(4, 4), FWBTile(4, 3));
	TestTrue(TEXT("Counter from Highground ignores walls"),
		WBRules::CanResolveCounterattack(Counter, Repository).bOk);

	FWBGameStateData NPC = MakeState(TEXT("diag"), FWBTile(2, 2), -1, 2);
	AddTarget(NPC, FWBTile(4, 4), 2, 0);
	TestTrue(TEXT("Fixture NPC uses diagonal attack geometry"),
		WBRules::CanDeclareNPCAttack(
			NPC, Repository, Attack(1, 2, -1)).bOk);
	NPC.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	AddWall(NPC, FWBTile(2, 2), FWBTile(3, 2));
	AddWall(NPC, FWBTile(2, 2), FWBTile(2, 3));
	TestTrue(TEXT("NPC Highground attack ignores walls"),
		WBRules::CanDeclareNPCAttack(
			NPC, Repository, Attack(1, 2, -1)).bOk);
	NPC.Walls.Reset();
	NPC.GetMutableUnitById(1)->MPRemaining = 1;
	NPC.SetTerrainForTest(FWBTile(3, 3), FName(TEXT("highground")));
	TestEqual(TEXT("NPC pays Highground surcharge"),
		WBRules::QueryNPCMove(
			NPC, Repository,
			Move(FWBTile(2, 2), FWBTile(3, 3), -1), 1).Reason,
		FString(TEXT("insufficient_mp")));
	return true;
}

WB_GEOMETRY_TEST(FWBNPCHighgroundPathAffordabilityTest,
	"Wandbound.Highground.NPC.PathingSkipsUnaffordableEntry")
bool FWBNPCHighgroundPathAffordabilityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(
		TEXT("diag_npc"), FWBTile(0, 0), -1, 2);
	FWBUnitState* NPC = State.GetMutableUnitById(1);
	NPC->NPCSpawnOrder = 0;
	NPC->NPCCreationTurnNumber = 1;
	AddTarget(State, FWBTile(3, 3), 2, 0);
	State.SetTerrainForTest(FWBTile(1, 1), FName(TEXT("highground")));

	uint32 RandomState = 1;
	const FWBNPCPhaseResolutionResult Result =
		WBNPCPhaseResolution::ResolvePhase(State, Repository, RandomState, 0);
	TestTrue(TEXT("NPC phase succeeds"), Result.bOk);
	TestEqual(TEXT("NPC rolls one MP"), Result.MPRolls.Num(), 1);
	if (Result.MPRolls.Num() == 1)
	{
		TestEqual(TEXT("Deterministic roll is one"), Result.MPRolls[0], 1);
	}
	const FWBUnitState* ResolvedNPC = State.GetUnitById(1);
	TestNotNull(TEXT("NPC remains available"), ResolvedNPC);
	if (ResolvedNPC != nullptr)
	{
		TestEqual(TEXT("NPC X remains unchanged"), ResolvedNPC->X, 0);
		TestEqual(TEXT("NPC Y remains unchanged"), ResolvedNPC->Y, 0);
	}
	TestFalse(TEXT("No unaffordable movement is planned"),
		Result.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("npc_movement_planned"));
		}));
	TestTrue(TEXT("NPC records no legal path"),
		Result.TraceEvents.ContainsByPredicate([](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("npc_skipped"))
				&& Event.Reason == TEXT("no_legal_path");
		}));
	return true;
}

WB_GEOMETRY_TEST(FWBAuraSharedGeometryTest,
	"Wandbound.Geometry.Aura.CanonicalDiagonalWallsNoHighgroundBypass")
bool FWBAuraSharedGeometryTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("diag_aura"), FWBTile(2, 2), 0, 3);
	AddTarget(State, FWBTile(4, 4), 2, 1, TEXT("target"), 4);
	AddWall(State, FWBTile(2, 2), FWBTile(3, 2));
	TestEqual(TEXT("One wall leaves Vex-like diagonal aura open"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 2).EffectiveValue, 3);
	AddWall(State, FWBTile(2, 2), FWBTile(2, 3));
	TestEqual(TEXT("Both routes block Vex-like aura"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 2).EffectiveValue, 4);
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	TestEqual(TEXT("Highground does not grant aura wall bypass"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 2).EffectiveValue, 4);
	TestEqual(TEXT("Intrinsic aura range includes Highground without recursion"),
		WBUnitStatQuery::GetAuraRangeAR(State, 1), 4);
	return true;
}

WB_GEOMETRY_TEST(FWBHighgroundARAndTerrainEffectTest,
	"Wandbound.Highground.AR.EffectiveCompositionAndImmediateTerrainMutation")
bool FWBHighgroundARAndTerrainEffectTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("orth"), FWBTile(2, 2), 0, 2);
	TestEqual(TEXT("Normal effective AR equals raw"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 1).EffectiveValue, 2);
	const int32 MPBefore = State.GetPlayerById(0)->RemainingMP;
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	TestEqual(TEXT("Raw AR unchanged"), State.GetUnitById(1)->AR, 2);
	TestEqual(TEXT("Highground grants one effective AR"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 1).EffectiveValue, 3);
	TestEqual(TEXT("Terrain under stationary unit charges no MP"),
		State.GetPlayerById(0)->RemainingMP, MPBefore);
	State.GetMutableUnitById(1)->CombatCapabilities.Add(
		EWBCombatCapability::ImmuneToEnemyEffects);
	TestEqual(TEXT("Enemy-effect immunity preserves terrain benefit"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 1).EffectiveValue, 3);
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("mud")));
	TestEqual(TEXT("Terrain overwrite removes benefit"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 1).EffectiveValue, 2);
	State.GetMutableUnitById(1)->X = 3;
	State.GetMutableUnitById(1)->Y = 2;
	TestEqual(TEXT("Leaving Highground has no stale modifier"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 1).EffectiveValue, 2);

	State.GetMutableUnitById(1)->X = 2;
	State.GetMutableUnitById(1)->Y = 2;
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	State.GetMutableUnitById(1)->CombatCapabilities.Reset();
	TestTrue(TEXT("AR-ranged Cartographer-style target gains Highground range"),
		WBRules::CanApplyEffectRequest(
			State, Repository, MakeTerrainRequest(FWBTile(5, 2))).bOk);
	const FWBEffectRequestResult Changed = WBEffectRunner::ApplyEffectRequest(
		State, MakeTerrainRequest(FWBTile(2, 2), FName(TEXT("normal"))),
		Repository);
	TestTrue(TEXT("Generic terrain overwrite applies"), Changed.bOk);
	TestEqual(TEXT("Overwrite immediately removes effective AR"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 1).EffectiveValue, 2);
	TestEqual(TEXT("Terrain mutation preserves HP"), State.GetUnitById(1)->HP, 10);
	TestEqual(TEXT("Terrain mutation preserves position X"), State.GetUnitById(1)->X, 2);
	TestEqual(TEXT("Terrain mutation preserves position Y"), State.GetUnitById(1)->Y, 2);
	return true;
}

WB_GEOMETRY_TEST(FWBHighgroundVexCompositionTest,
	"Wandbound.Highground.AR.VexComposition")
bool FWBHighgroundVexCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("diag_aura"), FWBTile(2, 2), 0, 3);
	AddTarget(State, FWBTile(4, 4), 2, 1, TEXT("target"), 2);
	State.SetTerrainForTest(FWBTile(4, 4), FName(TEXT("highground")));
	TestEqual(TEXT("Raw two plus Highground one minus aura one"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, 2).EffectiveValue, 2);
	TestEqual(TEXT("Stored AR remains two"), State.GetUnitById(2)->AR, 2);
	return true;
}

WB_GEOMETRY_TEST(FWBHighgroundPublicStateTest,
	"Wandbound.Highground.PublicState.SingleTerrainIdentity")
bool FWBHighgroundPublicStateTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(TEXT("orth"));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("mud")));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("highground")));
	TestEqual(TEXT("Highground overwrites prior terrain"),
		State.GetTerrainAt(FWBTile(2, 2)).GetPlainNameString().ToLower(),
		FString(TEXT("highground")));
	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State, Repository);
	const FWBPublicTerrainTileSummary* PublicTerrain =
		Summary.TerrainTiles.FindByPredicate([](const FWBPublicTerrainTileSummary& Tile)
		{
			return Tile.X == 2 && Tile.Y == 2;
		});
	TestNotNull(TEXT("Highground is present in public terrain"), PublicTerrain);
	if (PublicTerrain != nullptr)
	{
		TestEqual(TEXT("Public identity is canonical"),
			PublicTerrain->TerrainId.GetPlainNameString(), FString(TEXT("highground")));
	}
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("ice")));
	TestEqual(TEXT("Prior terrain can overwrite Highground"),
		State.GetTerrainAt(FWBTile(2, 2)).GetPlainNameString().ToLower(),
		FString(TEXT("ice")));
	TestTrue(TEXT("Highground metadata is generic"),
		WBTerrainRules::IsSupportedTerrain(FName(TEXT("highground"))));
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	FWBAction Pass;
	Pass.Type = EWBActionType::PassResponse;
	Pass.PlayerId = 1;
	TestEqual(TEXT("Stable action codec remains compatible"),
		WBActionCodec::MakeActionId(Pass), FString(TEXT("pass_response:p1")));
	return true;
}

#undef WB_GEOMETRY_TEST

#endif
