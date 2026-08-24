#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Algo/Reverse.h"

#include "WBActionCodec.h"
#include "WBCardDefinitionRepository.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNCrashInSmoke.h"
#include "WBPublicBoardSummary.h"
#include "WBRules.h"
#include "WBUnitStatQuery.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBContinuousStatAuraDefinition MakeARAura()
{
	FWBContinuousStatAuraDefinition Aura;
	Aura.AuraId = TEXT("enemy_ar_penalty_in_source_ar");
	Aura.TargetRelation = EWBContinuousAuraTargetRelation::Enemy;
	Aura.TargetStat = EWBContinuousStat::AR;
	Aura.Operation = EWBContinuousStatOperation::Add;
	Aura.Amount = -1;
	Aura.RangeStat = EWBContinuousAuraRangeStat::AR;
	Aura.Geometry = EWBContinuousAuraGeometry::AttackLine;
	Aura.bBlockedByWalls = true;
	Aura.bBlockedByUnits = true;
	Aura.MinimumResult = 0;
	return Aura;
}

FWBCardDefinition MakeCharacter(const FString& Id, const bool bAura = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = Id;
	Definition.PublicName = Id;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.PublicFactions.Add(TEXT("csn"));
	Definition.CharacterStats.HP = 13;
	Definition.CharacterStats.ATK = 3;
	Definition.CharacterStats.AR = 4;
	Definition.CharacterStats.RL = 2;
	if (bAura)
	{
		Definition.ContinuousStatAuras.Add(MakeARAura());
	}
	return Definition;
}

FWBCardDefinitionRepository MakeRepository()
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("semantic_aura_source"), true));
	Definitions.Add(MakeCharacter(TEXT("second_semantic_aura_source"), true));
	Definitions.Add(MakeCharacter(TEXT("char_csn_vex_like_without_data"), false));
	Definitions.Add(MakeCharacter(TEXT("target"), false));
	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("vex_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(const int32 Id, const int32 Owner, const FString& CardId,
	const FWBTile Tile, const int32 AR = 4)
{
	FWBUnitState Unit;
	Unit.UnitId = Id;
	Unit.OwnerId = Owner;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 10;
	Unit.MaxHP = 10;
	Unit.ATK = 3;
	Unit.AR = AR;
	Unit.AttacksLeft = 1;
	Unit.SetCanonicalRL(2, 2, 0);
	return Unit;
}

FWBGameStateData MakeState(const FWBTile SourceTile = FWBTile(4, 4),
	const FWBTile TargetTile = FWBTile(4, 7), const int32 TargetAR = 4)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	State.Players = { Player0, Player1 };
	State.AddUnitForTest(MakeUnit(10, 0, TEXT("semantic_aura_source"), SourceTile));
	State.AddUnitForTest(MakeUnit(20, 1, TEXT("target"), TargetTile, TargetAR));
	return State;
}

int32 EffectiveAR(const FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository, const int32 UnitId = 20)
{
	return WBUnitStatQuery::GetEffectiveAR(State, Repository, UnitId).EffectiveValue;
}

FWBAction MakeAttack(const int32 PlayerId, const int32 SourceId, const int32 TargetId)
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = SourceId;
	Action.TargetUnitId = TargetId;
	return Action;
}
}

#define WB_VEX_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_VEX_TEST(FWBCSNVexProductionDefinitionTest,
	"Wandbound.CSNVex.CardDB.ProductionDefinition")
bool FWBCSNVexProductionDefinitionTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	TestTrue(TEXT("Production suite loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid()) return false;
	AddInfo(TEXT("Production Vex bundle digest: ") + Loaded.Snapshot->ContentDigest);
	const FWBProductionCardRecord* Record = Loaded.Snapshot->FindRecord(TEXT("char_csn_vex"));
	TestNotNull(TEXT("Vex loads"), Record);
	if (Record == nullptr) return false;
	TestEqual(TEXT("HP"), Record->CoreDefinition.CharacterStats.HP, 13);
	TestEqual(TEXT("ATK"), Record->CoreDefinition.CharacterStats.ATK, 3);
	TestEqual(TEXT("AR"), Record->CoreDefinition.CharacterStats.AR, 4);
	TestEqual(TEXT("RL"), Record->CoreDefinition.CharacterStats.RL, 2);
	TestTrue(TEXT("CSN faction"), Record->CoreDefinition.PublicFactions.Contains(TEXT("csn")));
	TestEqual(TEXT("One continuous aura"), Record->CoreDefinition.ContinuousStatAuras.Num(), 1);
	if (Record->CoreDefinition.ContinuousStatAuras.Num() == 1)
	{
		const FWBContinuousStatAuraDefinition& Aura = Record->CoreDefinition.ContinuousStatAuras[0];
		TestEqual(TEXT("Enemy relation"), Aura.TargetRelation, EWBContinuousAuraTargetRelation::Enemy);
		TestEqual(TEXT("AR stat"), Aura.TargetStat, EWBContinuousStat::AR);
		TestEqual(TEXT("Add operation"), Aura.Operation, EWBContinuousStatOperation::Add);
		TestEqual(TEXT("Penalty"), Aura.Amount, -1);
	}
	return true;
}

WB_VEX_TEST(FWBCSNVexEffectiveARTest,
	"Wandbound.CSNVex.EffectiveAR.StoredStackingAndFloor")
bool FWBCSNVexEffectiveARTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestEqual(TEXT("One aura"), EffectiveAR(State, Repository), 3);
	TestEqual(TEXT("Stored AR unchanged"), State.GetUnitById(20)->AR, 4);
	State.AddUnitForTest(MakeUnit(11, 0, TEXT("second_semantic_aura_source"), FWBTile(4, 8)));
	TestEqual(TEXT("Two sources stack"), EffectiveAR(State, Repository), 2);
	State.GetMutableUnitById(20)->AR = 1;
	TestEqual(TEXT("AR floors at zero"), EffectiveAR(State, Repository), 0);
	TestEqual(TEXT("Repeated query deterministic"), EffectiveAR(State, Repository), 0);
	return true;
}

WB_VEX_TEST(FWBCSNVexDynamicMovementTest,
	"Wandbound.CSNVex.Dynamic.SourceAndTargetMovement")
bool FWBCSNVexDynamicMovementTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(FWBTile(4, 4), FWBTile(5, 7));
	TestEqual(TEXT("Off-axis unaffected"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(20)->X = 4;
	TestEqual(TEXT("Target enters aura"), EffectiveAR(State, Repository), 3);
	State.GetMutableUnitById(20)->X = 5;
	TestEqual(TEXT("Target leaves aura"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(10)->X = 5;
	TestEqual(TEXT("Source enters alignment"), EffectiveAR(State, Repository), 3);
	State.GetMutableUnitById(10)->X = 2;
	TestEqual(TEXT("Source moves away"), EffectiveAR(State, Repository), 4);
	return true;
}

WB_VEX_TEST(FWBCSNVexSuppressionRemovalTest,
	"Wandbound.CSNVex.Dynamic.SuppressionDestructionAndRemoval")
bool FWBCSNVexSuppressionRemovalTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const FName Statuses[] = { FName(TEXT("Negated")), FName(TEXT("Stunned")), FName(TEXT("Frozen")) };
	for (const FName Status : Statuses)
	{
		FWBGameStateData State = MakeState();
		State.GetMutableUnitById(10)->AddStatus(Status, 1);
		TestEqual(Status.ToString(), EffectiveAR(State, Repository), 4);
		State.GetMutableUnitById(10)->RemoveStatus(Status);
		TestEqual(Status.ToString() + TEXT(" restored"), EffectiveAR(State, Repository), 3);
	}
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(10)->MarkUnitDefeated();
	TestEqual(TEXT("Defeated source inactive"), EffectiveAR(State, Repository), 4);
	State = MakeState();
	State.GetMutableUnitById(10)->RemoveUnitFromBoard();
	TestEqual(TEXT("Removed source inactive"), EffectiveAR(State, Repository), 4);
	return true;
}

WB_VEX_TEST(FWBCSNVexRelationshipImmunityTest,
	"Wandbound.CSNVex.Target.RelationshipNPCAndImmunity")
bool FWBCSNVexRelationshipImmunityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.GetMutableUnitById(20)->OwnerId = 0;
	TestEqual(TEXT("Friendly unaffected"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(20)->OwnerId = 1;
	TestEqual(TEXT("Opponent affected"), EffectiveAR(State, Repository), 3);
	State.GetMutableUnitById(20)->OwnerId = -1;
	TestEqual(TEXT("Neutral NPC is opposing controller"), EffectiveAR(State, Repository), 3);
	State.GetMutableUnitById(20)->CombatCapabilities.Add(EWBCombatCapability::ImmuneToEnemyEffects);
	TestEqual(TEXT("Immune target unaffected"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(20)->CombatCapabilities.Remove(EWBCombatCapability::ImmuneToEnemyEffects);
	TestEqual(TEXT("Removing immunity restores aura"), EffectiveAR(State, Repository), 3);
	return true;
}

WB_VEX_TEST(FWBCSNVexGeometryTest,
	"Wandbound.CSNVex.Geometry.RangeWallsUnitsAndDiagonal")
bool FWBCSNVexGeometryTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState(FWBTile(4, 4), FWBTile(4, 8));
	TestEqual(TEXT("Distance four qualifies"), EffectiveAR(State, Repository), 3);
	State = MakeState(FWBTile(4, 3), FWBTile(4, 8));
	TestEqual(TEXT("Beyond range does not"), EffectiveAR(State, Repository), 4);
	State = MakeState();
	State.Walls.Add(FWBWallEdge(FWBTile(4, 5), FWBTile(4, 6)));
	TestEqual(TEXT("Wall blocks"), EffectiveAR(State, Repository), 4);
	State.Walls.Reset();
	TestEqual(TEXT("Wall removal restores"), EffectiveAR(State, Repository), 3);
	State.AddUnitForTest(MakeUnit(30, 0, TEXT("target"), FWBTile(4, 6)));
	TestEqual(TEXT("Friendly blocker blocks"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(30)->OwnerId = 1;
	TestEqual(TEXT("Enemy blocker blocks"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(30)->RemoveUnitFromBoard();
	TestEqual(TEXT("Blocker removal restores"), EffectiveAR(State, Repository), 3);
	State = MakeState(FWBTile(4, 4), FWBTile(6, 6));
	TestEqual(TEXT("Normal source cannot reach diagonal"), EffectiveAR(State, Repository), 4);
	State.GetMutableUnitById(10)->CombatCapabilities.Add(EWBCombatCapability::AttacksDiagonally);
	TestEqual(TEXT("Diagonal-capable generic source qualifies"), EffectiveAR(State, Repository), 3);
	return true;
}

WB_VEX_TEST(FWBCSNVexRecursionOrderTest,
	"Wandbound.CSNVex.Range.NonRecursiveMutualAndOrderIndependent")
bool FWBCSNVexRecursionOrderTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State;
	State.AddUnitForTest(MakeUnit(20, 1, TEXT("second_semantic_aura_source"), FWBTile(4, 8)));
	State.AddUnitForTest(MakeUnit(10, 0, TEXT("semantic_aura_source"), FWBTile(4, 4)));
	TestEqual(TEXT("First source aura range uses stored AR"), WBUnitStatQuery::GetAuraRangeAR(State, 10), 4);
	TestEqual(TEXT("Second source aura range uses stored AR"), WBUnitStatQuery::GetAuraRangeAR(State, 20), 4);
	TestEqual(TEXT("First effective AR"), EffectiveAR(State, Repository, 10), 3);
	TestEqual(TEXT("Second effective AR"), EffectiveAR(State, Repository, 20), 3);
	Algo::Reverse(State.Units);
	TestEqual(TEXT("Container order does not affect first"), EffectiveAR(State, Repository, 10), 3);
	TestEqual(TEXT("Container order does not affect second"), EffectiveAR(State, Repository, 20), 3);
	return true;
}

WB_VEX_TEST(FWBCSNVexAttackAuthorityTest,
	"Wandbound.CSNVex.Combat.DeclarationAndGeneration")
bool FWBCSNVexAttackAuthorityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State;
	State.CurrentPlayer = 1; State.PriorityPlayer = 1; State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData P0; P0.PlayerId = 0;
	FWBPlayerStateData P1; P1.PlayerId = 1;
	State.Players = { P0, P1 };
	State.AddUnitForTest(MakeUnit(10, 0, TEXT("semantic_aura_source"), FWBTile(4, 4)));
	State.AddUnitForTest(MakeUnit(20, 1, TEXT("target"), FWBTile(4, 7), 4));
	State.AddUnitForTest(MakeUnit(30, 0, TEXT("target"), FWBTile(4, 3), 4));
	const FWBAction InRange = MakeAttack(1, 20, 10);
	const FWBAction RawOnly = MakeAttack(1, 20, 30);
	TestTrue(TEXT("Distance three legal at effective three"), WBRules::CanDeclareAttack(State, Repository, InRange).bOk);
	TestFalse(TEXT("Distance four rejected at effective three"), WBRules::CanDeclareAttack(State, Repository, RawOnly).bOk);
	const TArray<FString> Ids = WBActionCodec::MakeActionIds(
		WBRules::GenerateLegalActionsForPlayer(State, Repository, 1));
	TestTrue(TEXT("Legal action included"), Ids.Contains(WBActionCodec::MakeActionId(InRange)));
	TestFalse(TEXT("Illegal action excluded"), Ids.Contains(WBActionCodec::MakeActionId(RawOnly)));
	return true;
}

WB_VEX_TEST(FWBCSNVexRedirectCounterNPCTest,
	"Wandbound.CSNVex.Combat.RedirectCounterAndNPC")
bool FWBCSNVexRedirectCounterNPCTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Redirect;
	Redirect.AddUnitForTest(MakeUnit(10, 0, TEXT("semantic_aura_source"), FWBTile(4, 4)));
	Redirect.AddUnitForTest(MakeUnit(20, 1, TEXT("target"), FWBTile(4, 7), 4));
	Redirect.AddUnitForTest(MakeUnit(30, 0, TEXT("target"), FWBTile(4, 3), 4));
	Redirect.PendingAttack.bActive = true;
	Redirect.PendingAttack.Stage = EWBAttackContinuationStage::PreHit;
	Redirect.PendingAttack.AttackerUnitId = 20;
	Redirect.PendingAttack.DefenderUnitId = 10;
	Redirect.PendingAttack.ContinuationId = TEXT("attack_1");
	TestFalse(TEXT("Redirect outside effective AR fails"),
		WBRules::CanRedirectPendingAttack(Redirect, Repository, TEXT("attack_1"), 30).bOk);
	Redirect.GetMutableUnitById(30)->Y = 5;
	TestTrue(TEXT("Redirect within effective AR succeeds"),
		WBRules::CanRedirectPendingAttack(Redirect, Repository, TEXT("attack_1"), 30).bOk);

	FWBGameStateData Counter = MakeState(FWBTile(4, 4), FWBTile(4, 6), 2);
	Counter.PendingAttack.bActive = true;
	Counter.PendingAttack.AttackerUnitId = 10;
	Counter.PendingAttack.DefenderUnitId = 20;
	Counter.PendingAttack.AttackingPlayerId = 0;
	TestFalse(TEXT("Counter outside effective one fails"), WBRules::CanResolveCounterattack(Counter, Repository).bOk);
	Counter.GetMutableUnitById(10)->Y = 5;
	TestTrue(TEXT("Counter within effective one succeeds"), WBRules::CanResolveCounterattack(Counter, Repository).bOk);

	FWBGameStateData NPC = MakeState(FWBTile(4, 4), FWBTile(4, 7), 4);
	NPC.GetMutableUnitById(20)->OwnerId = -1;
	const FWBAction NPCAttack = MakeAttack(-1, 20, 10);
	TestTrue(TEXT("NPC attack at effective three succeeds"), WBRules::CanDeclareNPCAttack(NPC, Repository, NPCAttack).bOk);
	NPC.GetMutableUnitById(10)->Y = 3;
	TestFalse(TEXT("NPC attack at raw-only four fails"), WBRules::CanDeclareNPCAttack(NPC, Repository, NPCAttack).bOk);
	return true;
}

WB_VEX_TEST(FWBCSNVexDefinitionAuthorityTest,
	"Wandbound.CSNVex.Authority.DefinitionDrivenNoCardIdBranch")
bool FWBCSNVexDefinitionAuthorityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	TestEqual(TEXT("Alternate identity with semantic data works"), EffectiveAR(State, Repository), 3);
	State.GetMutableUnitById(10)->CardId = TEXT("char_csn_vex_like_without_data");
	TestEqual(TEXT("Vex-like identity without data does not"), EffectiveAR(State, Repository), 4);
	return true;
}

WB_VEX_TEST(FWBCSNVexPublicStateTest,
	"Wandbound.CSNVex.PublicState.EffectiveARWithoutStoredMutation")
bool FWBCSNVexPublicStateTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const FWBGameStateData State = MakeState();
	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State, Repository);
	const FWBPublicUnitBoardSummary* Target = Summary.Units.FindByPredicate(
		[](const FWBPublicUnitBoardSummary& Unit) { return Unit.UnitId == 20; });
	TestNotNull(TEXT("Target public"), Target);
	if (Target != nullptr) TestEqual(TEXT("Public AR is effective"), Target->AR, 3);
	TestEqual(TEXT("Stored state remains raw"), State.GetUnitById(20)->AR, 4);
	return true;
}

WB_VEX_TEST(FWBCSNVexProductionSmokeTest,
	"Wandbound.CSNVex.ProductionSmoke.ReplayDeterministicAndPrivate")
bool FWBCSNVexProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNVexFixture/match_spec.json"));
	const FWBProductionCSNCrashInSmokeResult First =
		WBProductionCSNCrashInSmoke::RunVex(Request);
	const FWBProductionCSNCrashInSmokeResult Second =
		WBProductionCSNCrashInSmoke::RunVex(Request);
	if (!First.bOk) AddError(TEXT("First production smoke reason: ") + First.Reason);
	if (!Second.bOk) AddError(TEXT("Second production smoke reason: ") + Second.Reason);
	TestTrue(TEXT("First production smoke"), First.bOk);
	TestTrue(TEXT("Second production smoke"), Second.bOk);
	TestEqual(TEXT("Archive deterministic"), First.SerializedArchive, Second.SerializedArchive);
	TestEqual(TEXT("Receipt deterministic"), First.SerializedReceipt, Second.SerializedReceipt);
	TestEqual(TEXT("State digest deterministic"), First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("Trace digest deterministic"), First.FinalTraceDigest, Second.FinalTraceDigest);
	TestTrue(TEXT("Replay records verified"), First.RecordsVerified > 0);
	return true;
}

#undef WB_VEX_TEST

#endif
