#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBCardActivationExpansion.h"
#include "WBCardActivationSourceGate.h"
#include "WBCardDefinitionRepository.h"
#include "WBEffectRunner.h"
#include "WBMatchCoordinator.h"
#include "WBProductionActivationDataProvider.h"
#include "WBProductionActivationTargetSelectionBridge.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionMatchReplay.h"
#include "WBProductionTerrainCartographerSmoke.h"
#include "WBPublicBoardSummary.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
struct FCartographerCase
{
	const TCHAR* CardId;
	const TCHAR* PublicName;
	const TCHAR* EffectId;
	const TCHAR* TerrainId;
};

const FCartographerCase CartographerCases[] = {
	{ TEXT("char_mire_cartographer"), TEXT("Mire Cartographer"),
		TEXT("mud_survey"), TEXT("mud") },
	{ TEXT("char_emberfault_cartographer"), TEXT("Emberfault Cartographer"),
		TEXT("lava_survey"), TEXT("lava") },
	{ TEXT("char_tidecall_cartographer"), TEXT("Tidecall Cartographer"),
		TEXT("water_survey"), TEXT("water") },
	{ TEXT("char_rimecall_cartographer"), TEXT("Rimecall Cartographer"),
		TEXT("ice_survey"), TEXT("ice") }
};

FWBGenericEffectPayload MakeTerrainPayload(const FName TerrainId)
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::SetTerrain;
	Payload.SetTerrainEffect.TerrainId = TerrainId;
	Payload.SetTerrainEffect.RangeMetric = EWBEffectTileRangeMetric::Manhattan;
	Payload.SetTerrainEffect.RangeStat = EWBEffectRangeStat::AR;
	Payload.SetTerrainEffect.bAllowOccupied = true;
	Payload.SetTerrainEffect.bRequireLineOfSight = false;
	return Payload;
}

FWBCardEffectDefinition MakeTerrainEffect(
	const FString& EffectId,
	const FName TerrainId)
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = EffectId;
	Effect.PublicLabel = TEXT("Survey");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::Tile;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing =
		EWBCardActivationTimingRequirement::NormalTurnPriority;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.bHasExplicitSourceGate = true;
	Effect.Payloads.Add(MakeTerrainPayload(TerrainId));
	return Effect;
}

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const bool bTerrainEffect,
	const FName TerrainId = NAME_None)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 15;
	Definition.CharacterStats.ATK = 1;
	Definition.CharacterStats.AR = 3;
	Definition.CharacterStats.RL = 1;
	if (bTerrainEffect)
	{
		Definition.ActivatedEffects.Add(MakeTerrainEffect(
			TEXT("survey"), TerrainId));
	}
	return Definition;
}

FWBCardEffectDefinition MakeNegateEffect()
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = TEXT("semantic_negate");
	Effect.PublicLabel = TEXT("Negate");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::None;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = TEXT("semantic_negate_once");
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::NegatePendingEffect;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBContinuousStatAuraDefinition MakeVexAura()
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

FWBCardDefinitionRepository MakeRepository()
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("semantic_cartographer"), true,
		FName(TEXT("mud"))));
	Definitions.Add(MakeCharacter(TEXT("alternate_identity"), true,
		FName(TEXT("ice"))));
	Definitions.Add(MakeCharacter(TEXT("cartographer_name_only"), false));
	FWBCardDefinition Vex = MakeCharacter(TEXT("semantic_vex"), false);
	Vex.ContinuousStatAuras.Add(MakeVexAura());
	Definitions.Add(MoveTemp(Vex));
	Definitions.Add(MakeCharacter(TEXT("terrain_test_hero"), false));
	FWBCardDefinition EnemyHero = MakeCharacter(
		TEXT("terrain_test_enemy_hero"), false);
	EnemyHero.ActivatedEffects.Add(MakeNegateEffect());
	Definitions.Add(MoveTemp(EnemyHero));
	FWBCardDefinition Filler;
	Filler.CardId = TEXT("terrain_test_filler");
	Filler.PublicName = TEXT("Terrain Test Filler");
	Filler.PublicCategory = TEXT("Action");
	Filler.Kind = EWBCardDefinitionKind::Action;
	Definitions.Add(Filler);
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("terrain_test_trap");
	Trap.PublicName = TEXT("Terrain Test Trap");
	Trap.PublicCategory = TEXT("Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	Trap.TrapDamage = 1;
	Definitions.Add(Trap);
	FWBCardDefinition NPC = MakeCharacter(TEXT("terrain_test_npc"), false);
	NPC.Kind = EWBCardDefinitionKind::NPC;
	Definitions.Add(NPC);

	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("terrain_cartographer_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 AR = 3)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 15;
	Unit.MaxHP = 15;
	Unit.ATK = 1;
	Unit.AR = AR;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.SetArmorForTest(2, 2);
	Unit.SetCanonicalRL(1, 1, 0);
	return Unit;
}

FWBGameStateData MakeState(const FString& SourceCardId = TEXT("semantic_cartographer"))
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = 10;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 20;
	State.Players = { Player0, Player1 };
	State.AddUnitForTest(MakeUnit(10, 0, SourceCardId, FWBTile(4, 4)));
	State.AddUnitForTest(MakeUnit(11, 0, TEXT("friendly"), FWBTile(5, 4)));
	State.AddUnitForTest(MakeUnit(20, 1, TEXT("enemy"), FWBTile(4, 6)));
	State.AddUnitForTest(MakeUnit(30, -1, TEXT("npc"), FWBTile(3, 4)));
	return State;
}

FWBCardActivationCommand MakeCommand(
	const FWBCardDefinitionRepository& Repository,
	const FString& CardId,
	const FString& EffectId,
	const FWBTile Target)
{
	const FWBCardDefinitionRepositoryLookupResult Lookup =
		WBCardDefinitionRepository::FindCardById(Repository, CardId);
	FWBCardActivationExpansionRequest Request;
	Request.PlayerId = 0;
	Request.SourceUnitId = 10;
	Request.CardDefinition = Lookup.Definition;
	Request.EffectId = EffectId;
	Request.Target.TargetTile = Target;
	Request.SourceGateContext.PlayerId = 0;
	Request.SourceGateContext.SourceUnitId = 10;
	Request.SourceGateContext.SourceCardId = CardId;
	Request.SourceGateContext.SourceZone = EWBCardActivationSourceZone::Board;
	Request.SourceGateContext.bHasExplicitSourceGateContext = true;
	return WBCardActivationExpansion::BuildActivationCommand(Request).Command;
}

bool HasTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

FWBCardInstanceRef MakeCardRef(
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

FWBMatchInitializationRequest MakeCoordinatorRequest()
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 48321;
	Request.FirstPlayerId = 0;
	Request.Repository = MakeRepository();
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBMatchPlayerSetup Setup;
		Setup.PlayerId = PlayerId;
		Setup.HeroInstanceId = FString::Printf(
			TEXT("terrain_p%d_hero"), PlayerId);
		Setup.HeroCardId = PlayerId == 0
			? TEXT("terrain_test_hero") : TEXT("terrain_test_enemy_hero");
		Setup.HeroSpawnTile = PlayerId == 0
			? FWBTile(4, 8) : FWBTile(4, 0);
		Setup.OrderedDeck.Add(MakeCardRef(
			Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
		for (int32 Index = 0; Index < 9; ++Index)
		{
			Setup.OrderedDeck.Add(MakeCardRef(
				FString::Printf(TEXT("terrain_p%d_filler_%d"), PlayerId, Index),
				TEXT("terrain_test_filler"), PlayerId));
		}
		Request.Players.Add(MoveTemp(Setup));
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FWBSetupMarkerPlacement Marker;
		Marker.PlayerId = Index < 4 ? 0 : 1;
		Marker.Type = Index % 2 == 0 ? EWBMarkerType::Trap : EWBMarkerType::NPC;
		Marker.DefinitionId = Marker.Type == EWBMarkerType::Trap
			? TEXT("terrain_test_trap") : TEXT("terrain_test_npc");
		Marker.Tile = Index < 4
			? FWBTile(Index, 7) : FWBTile(Index - 4, 1);
		Marker.PlacementOrder = Index;
		Request.MarkerPlacements.Add(Marker);
	}
	return Request;
}

const FWBMatchLegalAction* FindTerrainActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FWBTile Target)
{
	return Actions.FindByPredicate([Target](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == TEXT("survey")
			&& Action.ActivationCommand.EffectRequest.Target.TargetTile == Target;
	});
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}

const FWBMatchLegalAction* FindResponsePass(
	const TArray<FWBMatchLegalAction>& Actions)
{
	return Actions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::PassResponse;
	});
}

bool InitializeCoordinatorWithSource(
	WBMatchCoordinator& Coordinator,
	FString& OutReason)
{
	const FWBMatchOperationResult Started = Coordinator.InitializeMatch(
		MakeCoordinatorRequest());
	if (!Started.bOk)
	{
		OutReason = Started.Reason;
		return false;
	}
	Coordinator.GetMutableStateForTest().AddUnitForTest(MakeUnit(
		10, 0, TEXT("semantic_cartographer"), FWBTile(4, 4)));
	return true;
}

bool PassUntilAction(WBMatchCoordinator& Coordinator, FString& OutReason)
{
	for (int32 PassIndex = 0;
		PassIndex < 4 && Coordinator.GetMatchPhase() == EWBMatchLoopPhase::Response;
		++PassIndex)
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Pass = Legal.bOk
			? FindResponsePass(Legal.Actions) : nullptr;
		if (Pass == nullptr)
		{
			OutReason = Legal.bOk ? TEXT("pass_response_missing") : Legal.Reason;
			return false;
		}
		const FWBMatchOperationResult Passed = Coordinator.SubmitActionId(
			Pass->PlayerId, Pass->ActionId);
		if (!Passed.bOk)
		{
			OutReason = Passed.Reason;
			return false;
		}
	}
	if (Coordinator.GetMatchPhase() != EWBMatchLoopPhase::Action)
	{
		OutReason = TEXT("response_did_not_close");
		return false;
	}
	return true;
}
}

#define WB_CARTOGRAPHER_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerProductionDefinitionsTest,
	"Wandbound.TerrainCartographers.CardDB.ProductionFamily")
bool FWBTerrainCartographerProductionDefinitionsTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json")));
	TestTrue(TEXT("Production database loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid()) return false;
	AddInfo(TEXT("Production Cartographer bundle digest: ")
		+ Loaded.Snapshot->ContentDigest);
	for (const FCartographerCase& Case : CartographerCases)
	{
		const FWBProductionCardRecord* Record =
			Loaded.Snapshot->FindRecord(Case.CardId);
		TestNotNull(Case.CardId, Record);
		if (Record == nullptr) continue;
		const FWBCardDefinition& Definition = Record->CoreDefinition;
		TestEqual(TEXT("Public name"), Definition.PublicName, FString(Case.PublicName));
		TestEqual(TEXT("HP"), Definition.CharacterStats.HP, 15);
		TestEqual(TEXT("Owner ATK supersession"), Definition.CharacterStats.ATK, 1);
		TestEqual(TEXT("AR"), Definition.CharacterStats.AR, 3);
		TestEqual(TEXT("RL"), Definition.CharacterStats.RL, 1);
		TestEqual(TEXT("One activation"), Definition.ActivatedEffects.Num(), 1);
		if (Definition.ActivatedEffects.Num() != 1) continue;
		const FWBCardEffectDefinition& Effect = Definition.ActivatedEffects[0];
		TestEqual(TEXT("Effect id"), Effect.EffectId, FString(Case.EffectId));
		TestEqual(TEXT("Tile target"), Effect.TargetRequirement,
			EWBCardEffectTargetRequirement::Tile);
		TestTrue(TEXT("Once per turn"), Effect.SourceGate.bOncePerTurn);
		TestTrue(TEXT("Exact-source key is derived"),
			Effect.SourceGate.OncePerTurnKey.IsEmpty());
		TestEqual(TEXT("One payload"), Effect.Payloads.Num(), 1);
		if (Effect.Payloads.Num() == 1)
		{
			TestEqual(TEXT("Set terrain operation"), Effect.Payloads[0].Operation,
				EWBGenericEffectOp::SetTerrain);
			TestEqual(TEXT("Terrain mapping"),
				Effect.Payloads[0].SetTerrainEffect.TerrainId,
				FName(Case.TerrainId));
		}
	}
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerTargetLegalityTest,
	"Wandbound.TerrainCartographers.Rules.ManhattanOccupiedAndNoLOS")
bool FWBTerrainCartographerTargetLegalityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 5)));
	const TArray<FWBTile> Legal = {
		FWBTile(4, 4), FWBTile(5, 4), FWBTile(4, 6), FWBTile(3, 4),
		FWBTile(4, 7), FWBTile(5, 5), FWBTile(6, 5)
	};
	for (const FWBTile Tile : Legal)
	{
		TestTrue(*FString::Printf(TEXT("%s legal"), *Tile.ToString()),
			WBRules::CanApplyCardActivationCommand(State, Repository,
				MakeCommand(Repository, TEXT("semantic_cartographer"),
					TEXT("survey"), Tile)).bOk);
	}
	for (const FWBTile Tile : { FWBTile(6, 6), FWBTile(4, 8), FWBTile(-1, 4) })
	{
		TestFalse(*FString::Printf(TEXT("%s illegal"), *Tile.ToString()),
			WBRules::CanApplyCardActivationCommand(State, Repository,
				MakeCommand(Repository, TEXT("semantic_cartographer"),
					TEXT("survey"), Tile)).bOk);
	}
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerEffectiveARTest,
	"Wandbound.TerrainCartographers.Rules.EffectiveARDynamicVexComposition")
bool FWBTerrainCartographerEffectiveARTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(40, 1, TEXT("semantic_vex"), FWBTile(4, 1), 4));
	const FWBCardActivationCommand DistanceThree = MakeCommand(
		Repository, TEXT("semantic_cartographer"), TEXT("survey"), FWBTile(7, 4));
	const FWBCardActivationCommand DistanceTwo = MakeCommand(
		Repository, TEXT("semantic_cartographer"), TEXT("survey"), FWBTile(6, 4));
	TestFalse(TEXT("Vex makes distance three illegal"),
		WBRules::CanApplyCardActivationCommand(State, Repository, DistanceThree).bOk);
	TestTrue(TEXT("Distance two remains legal"),
		WBRules::CanApplyCardActivationCommand(State, Repository, DistanceTwo).bOk);
	State.GetMutableUnitById(40)->bRemovedFromBoard = true;
	TestTrue(TEXT("Aura removal restores dynamic distance three"),
		WBRules::CanApplyCardActivationCommand(State, Repository, DistanceThree).bOk);
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerMutationTest,
	"Wandbound.TerrainCartographers.Effect.OverwriteNoOpPersistenceAndPublicState")
bool FWBTerrainCartographerMutationTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	const FWBUnitState OccupantBefore = *State.GetUnitById(20);
	const FWBCardActivationCommand Mud = MakeCommand(
		Repository, TEXT("semantic_cartographer"), TEXT("survey"), FWBTile(4, 6));
	const FWBCardActivationCommand Ice = MakeCommand(
		Repository, TEXT("alternate_identity"), TEXT("survey"), FWBTile(4, 6));
	const FWBCardActivationCommand Lookalike = MakeCommand(
		Repository, TEXT("cartographer_name_only"), TEXT("survey"), FWBTile(4, 6));

	const FWBCardActivationCommandResult MudResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Mud, Repository);
	TestTrue(TEXT("Normal to mud succeeds"), MudResult.bOk);
	TestEqual(TEXT("Mud stored"), State.GetTerrainAt(FWBTile(4, 6)), FName(TEXT("mud")));
	TestTrue(TEXT("Terrain change traced"), HasTrace(
		MudResult.TraceEvents, FName(TEXT("terrain_changed"))));
	const FString MudDigest = WBProductionMatchReplay::BuildGameStateDigest(State);

	State.ClearActivationUsageKeysForPlayer(0);
	State.GetMutableUnitById(10)->CardId = TEXT("alternate_identity");
	const FWBCardActivationCommandResult IceResult =
		WBEffectRunner::ApplyCardActivationCommand(State, Ice, Repository);
	TestTrue(TEXT("Definition-driven alternate identity succeeds"), IceResult.bOk);
	TestEqual(TEXT("Mud overwritten by ice"), State.GetTerrainAt(FWBTile(4, 6)),
		FName(TEXT("ice")));
	TestNotEqual(TEXT("Terrain participates in state digest"),
		WBProductionMatchReplay::BuildGameStateDigest(State), MudDigest);
	TestEqual(TEXT("One terrain identity per tile"), State.TerrainByTileIndex.Num(), 1);

	State.ClearActivationUsageKeysForPlayer(0);
	const FWBCardActivationCommandResult NoOp =
		WBEffectRunner::ApplyCardActivationCommand(State, Ice, Repository);
	TestTrue(TEXT("Same-terrain activation accepted"), NoOp.bOk);
	TestFalse(TEXT("No fabricated terrain transition"), HasTrace(
		NoOp.TraceEvents, FName(TEXT("terrain_changed"))));
	TestTrue(TEXT("No-op still consumes exact-source usage"),
		State.HasActivationUsageKeyThisTurn(0, Ice.UsageCommit.UsageKey));

	const FWBUnitState* OccupantAfter = State.GetUnitById(20);
	TestNotNull(TEXT("Occupant remains"), OccupantAfter);
	TestEqual(TEXT("Occupant tile unchanged"), FWBTile(OccupantAfter->X, OccupantAfter->Y),
		FWBTile(OccupantBefore.X, OccupantBefore.Y));
	TestEqual(TEXT("Occupant HP unchanged"), OccupantAfter->HP, OccupantBefore.HP);
	TestEqual(TEXT("Occupant armor unchanged"), OccupantAfter->CurrentArmor,
		OccupantBefore.CurrentArmor);
	TestEqual(TEXT("No invented status"), OccupantAfter->Statuses.Num(),
		OccupantBefore.Statuses.Num());

	const FWBPublicBoardSummary Public = WBPublicBoardSummary::Build(State, Repository);
	TestEqual(TEXT("One public terrain entry"), Public.TerrainTiles.Num(), 1);
	TestEqual(TEXT("Public terrain updated"), Public.TerrainTiles[0].TerrainId,
		FName(TEXT("ice")));
	TestFalse(TEXT("Lookalike without metadata has no command"),
		WBRules::CanApplyCardActivationCommand(State, Repository, Lookalike).bOk);
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerProviderTest,
	"Wandbound.TerrainCartographers.Runtime.PublicTileOptionsAndSelection")
bool FWBTerrainCartographerProviderTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const FWBGameStateData State = MakeState();
	FWBProductionActivationDataProvider Provider;
	FWBProductionActivationDataProviderInput Input;
	Input.GameState = &State;
	Input.Repository = &Repository;
	Input.ViewerPlayerId = 0;
	Provider.Configure(Input, FWBProductionActivationDataProviderConfig());
	FWBRuntimeActivationDataProviderRequest Request;
	Request.Kind = EWBRuntimeActivationDataRequestKind::CurrentDecisionPoint;
	Request.PlayerId = 0;
	const FWBRuntimeActivationDataProviderResult Result =
		Provider.GetActivationDecisionData(Request);
	TestTrue(TEXT("Provider succeeds"), Result.bOk);
	const FWBCardActivationLegalAction* Action =
		Result.RefreshInput.ActivationActionSet.Actions.FindByPredicate(
			[](const FWBCardActivationLegalAction& Candidate)
			{
				return Candidate.Candidate.SourceCardId == TEXT("semantic_cartographer");
			});
	TestNotNull(TEXT("Terrain action exposed"), Action);
	if (Action == nullptr) return false;
	TestEqual(TEXT("AR-three Manhattan diamond has 25 board tiles"),
		Action->TargetOptions.Num(), 25);
	const FWBCardActivationTargetOption* OwnTile = Action->TargetOptions.FindByPredicate(
		[](const FWBCardActivationTargetOption& Option)
		{
			return Option.Type == EWBCardActivationTargetOptionType::Tile
				&& Option.TargetTile == FWBTile(4, 4);
		});
	TestNotNull(TEXT("Own occupied tile exposed"), OwnTile);
	if (OwnTile == nullptr) return false;

	FWBProductionActivationTargetSelectionBridge Bridge;
	Bridge.ConfigureFromProviderData(Result.RefreshInput.ActivationActionSet.Actions);
	FWBProductionActivationTargetSelectionRequest Selection;
	Selection.ActivationEntryId = Action->ActivationActionId;
	Selection.SourceCardId = TEXT("semantic_cartographer");
	Selection.EffectId = TEXT("survey");
	Selection.bHasSelectedTarget = true;
	Selection.SelectedTargetOption = *OwnTile;
	const FWBProductionActivationTargetSelectionResult Selected =
		Bridge.BuildCommandForSelection(Selection);
	TestTrue(TEXT("Public tile selection resolves"), Selected.bOk);
	TestEqual(TEXT("Chosen tile fixed in command"),
		Selected.Command.EffectRequest.Target.TargetTile, FWBTile(4, 4));
	TestTrue(TEXT("No hidden unit identity attached to tile"),
		Selected.Command.EffectRequest.Target.TargetUnitId == -1);
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerUsageTest,
	"Wandbound.TerrainCartographers.Activation.ExactSourceOncePerTurnAndPersistence")
bool FWBTerrainCartographerUsageTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(
		12, 0, TEXT("semantic_cartographer"), FWBTile(2, 2)));
	const FWBCardActivationCommand SourceA = MakeCommand(
		Repository, TEXT("semantic_cartographer"), TEXT("survey"), FWBTile(4, 4));

	FWBCardActivationExpansionRequest SourceBRequest;
	SourceBRequest.PlayerId = 0;
	SourceBRequest.SourceUnitId = 12;
	SourceBRequest.CardDefinition = WBCardDefinitionRepository::FindCardById(
		Repository, TEXT("semantic_cartographer")).Definition;
	SourceBRequest.EffectId = TEXT("survey");
	SourceBRequest.Target.TargetTile = FWBTile(2, 2);
	SourceBRequest.SourceGateContext.PlayerId = 0;
	SourceBRequest.SourceGateContext.SourceUnitId = 12;
	SourceBRequest.SourceGateContext.SourceCardId = TEXT("semantic_cartographer");
	SourceBRequest.SourceGateContext.SourceZone = EWBCardActivationSourceZone::Board;
	SourceBRequest.SourceGateContext.bHasExplicitSourceGateContext = true;
	const FWBCardActivationCommand SourceB =
		WBCardActivationExpansion::BuildActivationCommand(SourceBRequest).Command;

	TestTrue(TEXT("First exact source accepted"),
		WBEffectRunner::ApplyCardActivationCommand(State, SourceA, Repository).bOk);
	TestFalse(TEXT("Same exact source denied in same turn"),
		WBRules::CanApplyCardActivationCommand(State, Repository, SourceA).bOk);
	TestTrue(TEXT("Second exact instance remains independent"),
		WBRules::CanApplyCardActivationCommand(State, Repository, SourceB).bOk);
	TestTrue(TEXT("Second exact instance applies"),
		WBEffectRunner::ApplyCardActivationCommand(State, SourceB, Repository).bOk);

	State.ClearActivationUsageKeysForPlayer(0);
	TestTrue(TEXT("Turn reset restores first exact source"),
		WBRules::CanApplyCardActivationCommand(State, Repository, SourceA).bOk);
	TestEqual(TEXT("Persistent terrain survives usage reset"),
		State.GetTerrainAt(FWBTile(4, 4)), FName(TEXT("mud")));
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerCoordinatorReplayTest,
	"Wandbound.TerrainCartographers.Coordinator.ResponseReplayAndPublicTerrain")
bool FWBTerrainCartographerCoordinatorReplayTest::RunTest(const FString&)
{
	const FWBTile ChosenTile(4, 6);
	FString Reason;
	WBMatchCoordinator Original;
	TestTrue(TEXT("Original initializes"),
		InitializeCoordinatorWithSource(Original, Reason));
	if (!Reason.IsEmpty()) AddError(Reason);
	if (!Reason.IsEmpty()) return false;
	const FWBMatchLegalActionGenerationResult InitialLegal =
		Original.EnumerateLegalActions();
	const FWBMatchLegalAction* Activation = InitialLegal.bOk
		? FindTerrainActivation(InitialLegal.Actions, ChosenTile) : nullptr;
	TestNotNull(TEXT("Chosen public tile action exists"), Activation);
	if (Activation == nullptr) return false;
	const FString ActivationId = Activation->ActionId;
	TestTrue(TEXT("Activation accepted"),
		Original.SubmitActionId(0, ActivationId).bOk);
	TestEqual(TEXT("Ordinary effect response opens"), Original.GetMatchPhase(),
		EWBMatchLoopPhase::Response);
	TestEqual(TEXT("Chosen tile remains unchanged before response"),
		Original.GetState().GetTerrainAt(ChosenTile), FName(TEXT("normal")));
	TestTrue(TEXT("Usage spent when activation is accepted"),
		Original.GetState().HasActivationUsageKeyThisTurn(
			0, WBCardActivationSourceGate::BuildDefaultUsageKey(
				0, 10, TEXT("semantic_cartographer"), TEXT("survey"))));
	TestTrue(TEXT("Passes close ordinary response"),
		PassUntilAction(Original, Reason));
	if (!Reason.IsEmpty()) AddError(Reason);
	TestEqual(TEXT("Resolved terrain changes fixed tile"),
		Original.GetState().GetTerrainAt(ChosenTile), FName(TEXT("mud")));
	TestTrue(TEXT("Public terrain trace emitted"), HasTrace(
		Original.GetTraceLog(), FName(TEXT("terrain_changed"))));
	const FWBMatchObservation Public = Original.BuildObservation(1);
	TestTrue(TEXT("Opponent public summary includes terrain"),
		Public.PublicBoard.TerrainTiles.ContainsByPredicate(
			[ChosenTile](const FWBPublicTerrainTileSummary& Tile)
			{
				return Tile.X == ChosenTile.X && Tile.Y == ChosenTile.Y
					&& Tile.TerrainId == FName(TEXT("mud"));
			}));

	const TArray<FWBMatchCommittedActionRecord> Records =
		Original.GetCommittedActionRecords();
	TestTrue(TEXT("Accepted declaration and passes recorded"), Records.Num() >= 2);
	WBMatchCoordinator Fresh;
	Reason.Reset();
	TestTrue(TEXT("Fresh initializes"), InitializeCoordinatorWithSource(Fresh, Reason));
	for (const FWBMatchCommittedActionRecord& Record : Records)
	{
		const FWBMatchOperationResult Applied = Fresh.SubmitActionId(
			Record.ActingPlayer, Record.ChosenActionId);
		TestTrue(TEXT("Fresh accepted replay action"), Applied.bOk);
		if (!Applied.bOk)
		{
			AddError(Applied.Reason);
			return false;
		}
	}
	TestEqual(TEXT("Replay reproduces terrain"),
		Fresh.GetState().GetTerrainAt(ChosenTile), FName(TEXT("mud")));
	TestEqual(TEXT("Replay state digest matches"),
		Fresh.GetCurrentStateDigest(), Original.GetCurrentStateDigest());
	TestEqual(TEXT("Replay trace digest matches"),
		Fresh.GetCurrentTraceDigest(), Original.GetCurrentTraceDigest());
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerNegationTest,
	"Wandbound.TerrainCartographers.Reaction.NegatePreservesTerrainAndConsumesUsage")
bool FWBTerrainCartographerNegationTest::RunTest(const FString&)
{
	const FWBTile ChosenTile(4, 6);
	FString Reason;
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Coordinator initializes"),
		InitializeCoordinatorWithSource(Coordinator, Reason));
	if (!Reason.IsEmpty()) AddError(Reason);
	const FWBMatchLegalActionGenerationResult InitialLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Activation = InitialLegal.bOk
		? FindTerrainActivation(InitialLegal.Actions, ChosenTile) : nullptr;
	if (Activation == nullptr) return false;
	TestTrue(TEXT("Terrain activation enters response"),
		Coordinator.SubmitActionId(0, Activation->ActionId).bOk);
	const FWBMatchLegalActionGenerationResult Responses =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Negate = Responses.bOk
		? FindActivation(Responses.Actions, TEXT("semantic_negate")) : nullptr;
	TestNotNull(TEXT("Generic negate is exposed"), Negate);
	if (Negate == nullptr) return false;
	TestTrue(TEXT("Generic negate accepted"),
		Coordinator.SubmitActionId(1, Negate->ActionId).bOk);
	TestEqual(TEXT("Negated activation leaves terrain unchanged"),
		Coordinator.GetState().GetTerrainAt(ChosenTile), FName(TEXT("normal")));
	TestFalse(TEXT("Negated activation emits no terrain transition"), HasTrace(
		Coordinator.GetTraceLog(), FName(TEXT("terrain_changed"))));
	TestTrue(TEXT("Accepted activation usage remains spent"),
		Coordinator.GetState().HasActivationUsageKeyThisTurn(
			0, WBCardActivationSourceGate::BuildDefaultUsageKey(
				0, 10, TEXT("semantic_cartographer"), TEXT("survey"))));
	return true;
}

WB_CARTOGRAPHER_TEST(FWBTerrainCartographerProductionSmokeTest,
	"Wandbound.TerrainCartographers.Runtime.ProductionFamilyAndFreshReplay")
bool FWBTerrainCartographerProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/TerrainCartographerFixture/match_spec.json"));
	const FWBProductionTerrainCartographerSmokeResult Result =
		WBProductionTerrainCartographerSmoke::Run(Request);
	TestTrue(*FString::Printf(TEXT("Production smoke succeeds: %s"),
		*Result.Reason), Result.bOk);
	TestTrue(TEXT("Fresh replay verifies records"), Result.RecordsVerified > 0);
	TestFalse(TEXT("State digest is present"), Result.FinalStateDigest.IsEmpty());
	TestFalse(TEXT("Trace digest is present"), Result.FinalTraceDigest.IsEmpty());
	TestTrue(TEXT("Archive serialized"),
		Result.SerializedArchive.Contains(TEXT("WandboundProductionMatchReplay")));
	TestTrue(TEXT("Receipt serialized"),
		Result.SerializedReceipt.Contains(TEXT("schema_version")));
	return Result.bOk;
}

#endif
