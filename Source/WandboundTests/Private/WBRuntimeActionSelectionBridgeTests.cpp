#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBRuntimeActionSelectionBridge.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeVisualControllerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeBridgePlayer(
	const int32 PlayerId,
	const int32 RemainingMP = 0,
	const int32 LastMPRoll = 0)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = LastMPRoll;
	return Player;
}

FWBUnitState MakeBridgeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId,
	const int32 AttacksLeft = 1,
	const int32 MaxAttacksPerTurn = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = AttacksLeft;
	Unit.MaxAttacksPerTurn = MaxAttacksPerTurn;
	return Unit;
}

FWBGameStateData MakeBridgeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeBridgePlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_BRIDGE_DECK"));
	Player0.Hand.Add(TEXT("SECRET_BRIDGE_HAND"));
	Player0.Discard.Add(TEXT("SECRET_BRIDGE_DISCARD"));

	FWBPlayerStateData Player1 = MakeBridgePlayer(1);
	Player1.Deck.Add(TEXT("SECRET_BRIDGE_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_BRIDGE_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_BRIDGE_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);

	FWBUnitState PlayerUnit = MakeBridgeUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_bridge_player"), 1, 1);
	PlayerUnit.AddStatus(FName(TEXT("Burn")), 2);
	State.AddUnitForTest(PlayerUnit);

	FWBUnitState OpponentUnit = MakeBridgeUnit(2, 1, FWBTile(6, 4), TEXT("char_visible_bridge_opponent"), 0, 2);
	OpponentUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(OpponentUnit);

	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

FWBAction MakeBridgeMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeBridgeAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(6, 4);
	return Action;
}

FWBAction MakeBridgeEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
}

void ExpectActionsEqual(
	FAutomationTestBase& Test,
	const FWBAction& Actual,
	const FWBAction& Expected)
{
	Test.TestEqual(TEXT("Action type"), static_cast<int32>(Actual.Type), static_cast<int32>(Expected.Type));
	Test.TestEqual(TEXT("Action player"), Actual.PlayerId, Expected.PlayerId);
	Test.TestEqual(TEXT("Action source unit"), Actual.SourceUnitId, Expected.SourceUnitId);
	Test.TestEqual(TEXT("Action target unit"), Actual.TargetUnitId, Expected.TargetUnitId);
	Test.TestEqual(TEXT("Action from x"), Actual.FromTile.X, Expected.FromTile.X);
	Test.TestEqual(TEXT("Action from y"), Actual.FromTile.Y, Expected.FromTile.Y);
	Test.TestEqual(TEXT("Action to x"), Actual.ToTile.X, Expected.ToTile.X);
	Test.TestEqual(TEXT("Action to y"), Actual.ToTile.Y, Expected.ToTile.Y);
}

void ExpectStateUnchanged(
	FAutomationTestBase& Test,
	const FWBGameStateData& State,
	const FWBGameStateData& Before)
{
	Test.TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	Test.TestEqual(TEXT("Priority player unchanged"), State.PriorityPlayer, Before.PriorityPlayer);
	Test.TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	Test.TestEqual(TEXT("Player MP unchanged"), State.GetPlayerById(0)->RemainingMP, Before.GetPlayerById(0)->RemainingMP);
	Test.TestEqual(TEXT("Unit x unchanged"), State.GetUnitById(1)->X, Before.GetUnitById(1)->X);
	Test.TestEqual(TEXT("Unit y unchanged"), State.GetUnitById(1)->Y, Before.GetUnitById(1)->Y);
}

void ExpectBridgeRefreshCounts(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestTrue(TEXT("Rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 81);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 2);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 1);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 2);
}

struct FBridgeVisualPath
{
	AWBBoardViewActor* BoardView = nullptr;
	UWBBoardViewStateApplierComponent* Applier = nullptr;
	UWBRuntimeVisualControllerComponent* VisualController = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
};

FBridgeVisualPath MakeBridgeVisualPath()
{
	FBridgeVisualPath Path;
	Path.BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	Path.Applier = NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
	Path.VisualController = NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());
	Path.Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());

	if (Path.Applier != nullptr)
	{
		Path.Applier->bApplyOnSummarySet = false;
		Path.Applier->SetBoardViewActor(Path.BoardView);
	}

	if (Path.VisualController != nullptr)
	{
		Path.VisualController->SetBoardViewStateApplier(Path.Applier);
	}

	if (Path.Facade != nullptr)
	{
		Path.Facade->SetVisualController(Path.VisualController);
	}

	return Path;
}

FString SerializeRuntimeResult(const FWBRuntimeSelectedActionResult& Result)
{
	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Result, Json);
	return Json;
}

bool LoadBridgeSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeActionSelectionBridge.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeActionSelectionBridge.cpp"));

	FString Header;
	FString Source;
	if (!FFileHelper::LoadFileToString(Header, *HeaderPath) || !FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		return false;
	}

	OutSource = Header + Source;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeResolveMoveTest, "Wandbound.Runtime.ActionSelectionBridge.ResolveMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeResolveMoveTest::RunTest(const FString& Parameters)
{
	const FWBAction MoveAction = MakeBridgeMoveAction();
	const TArray<FWBAction> LegalActions = { MoveAction, MakeBridgeEndTurnAction() };

	const FWBResolvedRuntimeActionSelection Selection =
		WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
			LegalActions,
			WBActionCodec::MakeActionId(MoveAction));

	TestTrue(TEXT("Move selection resolves"), Selection.bOk);
	TestTrue(TEXT("Failure reason empty"), Selection.Reason.IsEmpty());
	TestEqual(TEXT("Selected action id retained"), Selection.SelectedActionId, FString(TEXT("move:p0:u1:4,4->5,4")));
	ExpectActionsEqual(*this, Selection.ResolvedAction, MoveAction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeResolveAttackTest, "Wandbound.Runtime.ActionSelectionBridge.ResolveAttack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeResolveAttackTest::RunTest(const FString& Parameters)
{
	const FWBAction AttackAction = MakeBridgeAttackAction();
	const TArray<FWBAction> LegalActions = { MakeBridgeMoveAction(), AttackAction, MakeBridgeEndTurnAction() };

	const FWBResolvedRuntimeActionSelection Selection =
		WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
			LegalActions,
			WBActionCodec::MakeActionId(AttackAction));

	TestTrue(TEXT("Attack selection resolves"), Selection.bOk);
	TestEqual(TEXT("Attack action id"), Selection.SelectedActionId, FString(TEXT("attack:p0:u1:t2")));
	ExpectActionsEqual(*this, Selection.ResolvedAction, AttackAction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeResolveEndTurnTest, "Wandbound.Runtime.ActionSelectionBridge.ResolveEndTurn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeResolveEndTurnTest::RunTest(const FString& Parameters)
{
	const FWBAction EndTurnAction = MakeBridgeEndTurnAction();
	const TArray<FWBAction> LegalActions = { MakeBridgeMoveAction(), EndTurnAction };

	const FWBResolvedRuntimeActionSelection Selection =
		WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
			LegalActions,
			WBActionCodec::MakeActionId(EndTurnAction));

	TestTrue(TEXT("EndTurn selection resolves"), Selection.bOk);
	TestEqual(TEXT("EndTurn action id"), Selection.SelectedActionId, FString(TEXT("end_turn:p0")));
	ExpectActionsEqual(*this, Selection.ResolvedAction, EndTurnAction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeMissingIdTest, "Wandbound.Runtime.ActionSelectionBridge.MissingActionIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeMissingIdTest::RunTest(const FString& Parameters)
{
	const TArray<FWBAction> LegalActions = { MakeBridgeMoveAction() };
	const FWBResolvedRuntimeActionSelection Selection =
		WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
			LegalActions,
			TEXT("end_turn:p0"));

	TestFalse(TEXT("Missing selection fails"), Selection.bOk);
	TestEqual(TEXT("Missing reason"), Selection.Reason, FString(TEXT("selected_action_id_not_found")));
	TestEqual(TEXT("Selected action id retained"), Selection.SelectedActionId, FString(TEXT("end_turn:p0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeAmbiguousIdTest, "Wandbound.Runtime.ActionSelectionBridge.AmbiguousActionIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeAmbiguousIdTest::RunTest(const FString& Parameters)
{
	const FWBAction MoveAction = MakeBridgeMoveAction();
	const TArray<FWBAction> LegalActions = { MoveAction, MoveAction };
	const FWBResolvedRuntimeActionSelection Selection =
		WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
			LegalActions,
			WBActionCodec::MakeActionId(MoveAction));

	TestFalse(TEXT("Ambiguous selection fails"), Selection.bOk);
	TestEqual(TEXT("Ambiguous reason"), Selection.Reason, FString(TEXT("selected_action_id_ambiguous")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeEmptyListTest, "Wandbound.Runtime.ActionSelectionBridge.EmptyLegalListFailsSafely", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeEmptyListTest::RunTest(const FString& Parameters)
{
	const TArray<FWBAction> LegalActions;
	const FWBResolvedRuntimeActionSelection Selection =
		WBRuntimeActionSelectionBridge::ResolveSelectedActionId(
			LegalActions,
			TEXT("move:p0:u1:4,4->5,4"));

	TestFalse(TEXT("Empty list fails"), Selection.bOk);
	TestEqual(TEXT("Empty list reason"), Selection.Reason, FString(TEXT("selected_action_id_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeNullFacadeNoMutationTest, "Wandbound.Runtime.ActionSelectionBridge.NullFacadeFailsWithoutMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeNullFacadeNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	const FWBGameStateData Before = State;
	const FWBAction MoveAction = MakeBridgeMoveAction();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;

	const FWBRuntimeActionSelectionExecutionResult Result =
		WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
			State,
			{ MoveAction },
			WBActionCodec::MakeActionId(MoveAction),
			Context,
			nullptr);

	TestFalse(TEXT("Null facade fails"), Result.Selection.bOk);
	TestEqual(TEXT("Null facade reason"), Result.Selection.Reason, FString(TEXT("runtime_controller_facade_missing")));
	TestFalse(TEXT("No execution result"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	ExpectStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeExecuteMoveTest, "Wandbound.Runtime.ActionSelectionBridge.ExecuteMoveThroughFacade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeExecuteMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	const FWBAction MoveAction = MakeBridgeMoveAction();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FBridgeVisualPath VisualPath = MakeBridgeVisualPath();
	TestNotNull(TEXT("Facade"), VisualPath.Facade);
	if (VisualPath.Facade == nullptr)
	{
		return false;
	}

	const FWBRuntimeActionSelectionExecutionResult Result =
		WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
			State,
			{ MoveAction, MakeBridgeEndTurnAction() },
			WBActionCodec::MakeActionId(MoveAction),
			Context,
			VisualPath.Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Runtime move succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Unit moved x"), State.GetUnitById(1)->X, 5);
	TestEqual(TEXT("Unit moved y"), State.GetUnitById(1)->Y, 4);
	TestEqual(TEXT("Player MP decremented"), State.GetPlayerById(0)->RemainingMP, 1);
	ExpectBridgeRefreshCounts(*this, Result.Execution.VisualRefreshResult);
	TestEqual(TEXT("Facade retained selected id"), VisualPath.Facade->GetLastRuntimeResult().SelectedActionId, FString(TEXT("move:p0:u1:4,4->5,4")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeExecuteEndTurnTest, "Wandbound.Runtime.ActionSelectionBridge.ExecuteFullEndTurnThroughFacade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeExecuteEndTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	const FWBAction EndTurnAction = MakeBridgeEndTurnAction();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FBridgeVisualPath VisualPath = MakeBridgeVisualPath();
	TestNotNull(TEXT("Facade"), VisualPath.Facade);
	if (VisualPath.Facade == nullptr)
	{
		return false;
	}

	const FWBRuntimeActionSelectionExecutionResult Result =
		WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
			State,
			{ MakeBridgeMoveAction(), EndTurnAction },
			WBActionCodec::MakeActionId(EndTurnAction),
			Context,
			VisualPath.Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Runtime full EndTurn succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Runtime result consumed roll"), Result.Execution.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Runtime result consumed roll value"), Result.Execution.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	ExpectBridgeRefreshCounts(*this, Result.Execution.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeFailedResolutionNoRollTest, "Wandbound.Runtime.ActionSelectionBridge.FailedResolutionDoesNotConsumeRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeFailedResolutionNoRollTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	const FWBGameStateData Before = State;
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FBridgeVisualPath VisualPath = MakeBridgeVisualPath();
	TestNotNull(TEXT("Facade"), VisualPath.Facade);
	if (VisualPath.Facade == nullptr)
	{
		return false;
	}

	const FWBRuntimeActionSelectionExecutionResult Result =
		WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
			State,
			{ MakeBridgeEndTurnAction() },
			TEXT("move:p0:u1:4,4->5,4"),
			Context,
			VisualPath.Facade);

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Missing reason"), Result.Selection.Reason, FString(TEXT("selected_action_id_not_found")));
	TestEqual(TEXT("Roll remains queued"), RollSource.NumRemainingRolls(), 1);
	ExpectStateUnchanged(*this, State, Before);
	TestFalse(TEXT("Facade did not execute"), VisualPath.Facade->HasExecutedAction());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeNoActionGenerationSourceGuardTest, "Wandbound.Runtime.ActionSelectionBridge.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeNoLegalityCallsSourceGuardTest, "Wandbound.Runtime.ActionSelectionBridge.NoLegalityCallsSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeNoLegalityCallsSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Bridge source loads"), LoadBridgeSource(Source));
	TestFalse(TEXT("Does not call CanMoveUnit"), Source.Contains(TEXT("CanMoveUnit")));
	TestFalse(TEXT("Does not call CanDeclareAttack"), Source.Contains(TEXT("CanDeclareAttack")));
	TestFalse(TEXT("Does not call CanEndTurn"), Source.Contains(TEXT("CanEndTurn")));
	TestFalse(TEXT("Does not call CanApply"), Source.Contains(TEXT("CanApply")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeActionIdsStableTest, "Wandbound.Runtime.ActionSelectionBridge.ActionIdsStable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeActionIdsStableTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move ID stable"), WBActionCodec::MakeActionId(MakeBridgeMoveAction()), FString(TEXT("move:p0:u1:4,4->5,4")));
	TestEqual(TEXT("Attack ID stable"), WBActionCodec::MakeActionId(MakeBridgeAttackAction()), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn ID stable"), WBActionCodec::MakeActionId(MakeBridgeEndTurnAction()), FString(TEXT("end_turn:p0")));

	FWBAction PassAction;
	PassAction.Type = EWBActionType::Pass;
	PassAction.PlayerId = 0;
	TestEqual(TEXT("Pass ID stable"), WBActionCodec::MakeActionId(PassAction), FString(TEXT("pass:p0")));

	FWBAction PassResponseAction;
	PassResponseAction.Type = EWBActionType::PassResponse;
	PassResponseAction.PlayerId = 1;
	TestEqual(TEXT("PassResponse ID stable"), WBActionCodec::MakeActionId(PassResponseAction), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeActionSelectionBridgeHiddenDataExcludedTest, "Wandbound.Runtime.ActionSelectionBridge.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeActionSelectionBridgeHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeState();
	const FWBAction MoveAction = MakeBridgeMoveAction();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FBridgeVisualPath VisualPath = MakeBridgeVisualPath();
	TestNotNull(TEXT("Facade"), VisualPath.Facade);
	if (VisualPath.Facade == nullptr)
	{
		return false;
	}

	const FWBRuntimeActionSelectionExecutionResult Result =
		WBRuntimeActionSelectionBridge::ExecuteSelectedActionId(
			State,
			{ MoveAction },
			WBActionCodec::MakeActionId(MoveAction),
			Context,
			VisualPath.Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Runtime move succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	const FString Serialized = SerializeRuntimeResult(Result.Execution.RuntimeResult);
	TestFalse(TEXT("Player deck secret absent"), Serialized.Contains(TEXT("SECRET_BRIDGE_DECK")));
	TestFalse(TEXT("Player hand secret absent"), Serialized.Contains(TEXT("SECRET_BRIDGE_HAND")));
	TestFalse(TEXT("Player discard secret absent"), Serialized.Contains(TEXT("SECRET_BRIDGE_DISCARD")));
	TestFalse(TEXT("Opponent deck secret absent"), Serialized.Contains(TEXT("SECRET_BRIDGE_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand secret absent"), Serialized.Contains(TEXT("SECRET_BRIDGE_OPPONENT_HAND")));
	TestFalse(TEXT("Opponent discard secret absent"), Serialized.Contains(TEXT("SECRET_BRIDGE_OPPONENT_DISCARD")));
	TestTrue(TEXT("Visible unit remains public"), Serialized.Contains(TEXT("char_visible_bridge_player")));
	return true;
}

#endif
