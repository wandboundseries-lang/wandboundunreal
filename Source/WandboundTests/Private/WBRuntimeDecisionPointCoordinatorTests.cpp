#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeVisualControllerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeDecisionPointCoordinatorComponent* MakeDecisionCoordinator()
{
	return NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
}

UWBRuntimeTurnInteractionModelComponent* MakeDecisionInteractionModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeVisualControllerComponent* MakeDecisionVisualController()
{
	return NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());
}

UWBBoardViewStateApplierComponent* MakeDecisionStateApplier()
{
	return NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
}

struct FDecisionVisualPath
{
	AWBBoardViewActor* BoardView = nullptr;
	UWBBoardViewStateApplierComponent* Applier = nullptr;
	UWBRuntimeVisualControllerComponent* VisualController = nullptr;
};

FDecisionVisualPath MakeDecisionVisualPath()
{
	FDecisionVisualPath Path;
	Path.BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	Path.Applier = MakeDecisionStateApplier();
	Path.VisualController = MakeDecisionVisualController();

	if (Path.Applier != nullptr)
	{
		Path.Applier->bApplyOnSummarySet = false;
		Path.Applier->SetBoardViewActor(Path.BoardView);
	}

	if (Path.VisualController != nullptr)
	{
		Path.VisualController->SetBoardViewStateApplier(Path.Applier);
	}

	return Path;
}

FWBAction MakeDecisionMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(3, 4);
	Action.ToTile = FWBTile(4, 4);
	return Action;
}

FWBAction MakeDecisionAttackAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Attack;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(3, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeDecisionEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
}

TArray<FWBAction> MakeDecisionActionList()
{
	return { MakeDecisionMoveAction(), MakeDecisionAttackAction(), MakeDecisionEndTurnAction() };
}

FWBPlayerStateData MakeDecisionPlayer(
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

FWBUnitState MakeDecisionUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId)
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
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeDecisionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeDecisionPlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_DECISION_HANDOFF_DECK"));
	Player0.Hand.Add(TEXT("SECRET_DECISION_HANDOFF_HAND"));
	Player0.Discard.Add(TEXT("SECRET_DECISION_HANDOFF_DISCARD"));

	FWBPlayerStateData Player1 = MakeDecisionPlayer(1);
	Player1.Deck.Add(TEXT("SECRET_DECISION_HANDOFF_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_DECISION_HANDOFF_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_DECISION_HANDOFF_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeDecisionUnit(1, 0, FWBTile(3, 4), TEXT("visible_decision_handoff_player")));
	State.AddUnitForTest(MakeDecisionUnit(2, 1, FWBTile(5, 4), TEXT("visible_decision_handoff_target")));
	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

void ExpectDecisionStateUnchanged(
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

void ExpectDefaultDecisionStatus(FAutomationTestBase& Test, const FWBRuntimeDecisionPointStatus& Status)
{
	Test.TestFalse(TEXT("Status not ready"), Status.bReady);
	Test.TestFalse(TEXT("Status has no actions"), Status.bHasActions);
	Test.TestEqual(TEXT("Legal action count"), Status.LegalActionCount, 0);
	Test.TestEqual(TEXT("Presentation entry count"), Status.PresentationEntryCount, 0);
	Test.TestEqual(TEXT("Public unit count"), Status.PublicUnitCount, 0);
	Test.TestEqual(TEXT("Public wall count"), Status.PublicWallCount, 0);
	Test.TestEqual(TEXT("Public terrain count"), Status.PublicTerrainCount, 0);
	Test.TestFalse(TEXT("No last selected action"), Status.bHasLastSelectedAction);
	Test.TestFalse(TEXT("No last selected action resolved"), Status.bLastSelectedActionResolved);
}

void ExpectDemoVisualRefresh(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestTrue(TEXT("Rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 81);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 2);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 1);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 2);
}

FString CombineDecisionSnapshotPublicText(const FWBRuntimeLegalActionPresentationSnapshot& Snapshot)
{
	FString Combined;
	for (const FWBRuntimeActionPresentationEntry& Entry : Snapshot.Entries)
	{
		Combined += Entry.ActionId;
		Combined += Entry.PublicLabel;
		Combined += Entry.SourcePublicCardId;
		Combined += Entry.TargetPublicCardId;
	}
	return Combined;
}

FString CombineDecisionExecutionPublicText(
	const FWBRuntimeDecisionPointStatus& Status,
	const FWBRuntimeActionSelectionExecutionResult& Result)
{
	FString Combined;
	Combined += Status.LastSelectedActionId;
	Combined += Status.LastSelectedActionReason;
	Combined += Result.Selection.SelectedActionId;
	Combined += Result.Selection.Reason;
	Combined += Result.Execution.RuntimeResult.ApplyResult.Reason;

	for (const FWBPublicUnitBoardSummary& Unit : Result.Execution.RuntimeResult.FinalPublicBoardSummary.Units)
	{
		Combined += Unit.CardId;
	}

	return Combined;
}

bool LoadDecisionCoordinatorSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeDecisionPointCoordinatorComponent.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeDecisionPointCoordinatorComponent.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorClassExistsTest, "Wandbound.Runtime.DecisionPointCoordinator.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Decision point coordinator class"), UWBRuntimeDecisionPointCoordinatorComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorInitialStateTest, "Wandbound.Runtime.DecisionPointCoordinator.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorInitialStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	if (Coordinator == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No current decision point"), Coordinator->HasCurrentDecisionPoint());
	ExpectDefaultDecisionStatus(*this, Coordinator->GetCurrentStatus());
	TestNull(TEXT("No presentation snapshot"), Coordinator->GetCurrentPresentationSnapshot());
	TestFalse(TEXT("Last refresh not ok"), Coordinator->GetLastRefreshResult().bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorBothTargetsTest, "Wandbound.Runtime.DecisionPointCoordinator.RefreshBothTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorBothTargetsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	const FDecisionVisualPath VisualPath = MakeDecisionVisualPath();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Visual controller"), VisualPath.VisualController);
	if (Coordinator == nullptr || Model == nullptr || VisualPath.VisualController == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->SetVisualController(VisualPath.VisualController);
	const TArray<FWBAction> Actions = MakeDecisionActionList();
	const FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	const FWBRuntimeInteractionRefreshResult Result = Coordinator->RefreshDecisionPoint(Actions, Summary);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Has current decision point"), Coordinator->HasCurrentDecisionPoint());
	const FWBRuntimeDecisionPointStatus Status = Coordinator->GetCurrentStatus();
	TestTrue(TEXT("Status ready"), Status.bReady);
	TestTrue(TEXT("Status has actions"), Status.bHasActions);
	TestEqual(TEXT("Legal action count"), Status.LegalActionCount, Actions.Num());
	TestEqual(TEXT("Presentation entry count"), Status.PresentationEntryCount, Actions.Num());
	TestEqual(TEXT("Public unit count"), Status.PublicUnitCount, Summary.Units.Num());
	TestEqual(TEXT("Public wall count"), Status.PublicWallCount, Summary.Walls.Num());
	TestEqual(TEXT("Public terrain count"), Status.PublicTerrainCount, Summary.TerrainTiles.Num());
	TestNotNull(TEXT("Presentation snapshot exists"), Coordinator->GetCurrentPresentationSnapshot());
	ExpectDemoVisualRefresh(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorModelOnlyTest, "Wandbound.Runtime.DecisionPointCoordinator.RefreshModelOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorModelOnlyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	const TArray<FWBAction> Actions = { MakeDecisionMoveAction(), MakeDecisionEndTurnAction() };
	const FWBRuntimeInteractionRefreshResult Result = Coordinator->RefreshDecisionPoint(
		Actions,
		WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Current decision point"), Coordinator->HasCurrentDecisionPoint());
	TestNotNull(TEXT("Snapshot exists"), Coordinator->GetCurrentPresentationSnapshot());
	TestEqual(TEXT("Snapshot count"), Coordinator->GetCurrentPresentationSnapshot()->Entries.Num(), Actions.Num());
	TestEqual(TEXT("Status presentation count"), Coordinator->GetCurrentStatus().PresentationEntryCount, Actions.Num());
	TestFalse(TEXT("Visual not refreshed"), Result.bVisualControllerRefreshed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorVisualOnlyTest, "Wandbound.Runtime.DecisionPointCoordinator.RefreshVisualOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorVisualOnlyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeVisualControllerComponent* VisualController = MakeDecisionVisualController();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Visual controller"), VisualController);
	if (Coordinator == nullptr || VisualController == nullptr)
	{
		return false;
	}

	Coordinator->bRefreshTurnInteractionModel = false;
	Coordinator->SetVisualController(VisualController);
	const TArray<FWBAction> Actions = { MakeDecisionMoveAction() };
	const FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	const FWBRuntimeInteractionRefreshResult Result = Coordinator->RefreshDecisionPoint(Actions, Summary);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Current decision point"), Coordinator->HasCurrentDecisionPoint());
	TestTrue(TEXT("Status ready"), Coordinator->GetCurrentStatus().bReady);
	TestTrue(TEXT("Has actions"), Coordinator->GetCurrentStatus().bHasActions);
	TestEqual(TEXT("Legal action count"), Coordinator->GetCurrentStatus().LegalActionCount, Actions.Num());
	TestEqual(TEXT("Public unit count"), Coordinator->GetCurrentStatus().PublicUnitCount, Summary.Units.Num());
	TestEqual(TEXT("Presentation count is zero"), Coordinator->GetCurrentStatus().PresentationEntryCount, 0);
	TestNull(TEXT("No snapshot without model"), Coordinator->GetCurrentPresentationSnapshot());
	TestTrue(TEXT("Visual received summary"), VisualController->HasReceivedPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorMissingModelFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.MissingModelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorMissingModelFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeVisualControllerComponent* VisualController = MakeDecisionVisualController();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Visual controller"), VisualController);
	if (Coordinator == nullptr || VisualController == nullptr)
	{
		return false;
	}

	Coordinator->SetVisualController(VisualController);
	const FWBRuntimeInteractionRefreshResult Result = Coordinator->RefreshDecisionPoint(
		{ MakeDecisionMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("turn_interaction_model_missing")));
	TestFalse(TEXT("No current decision point"), Coordinator->HasCurrentDecisionPoint());
	ExpectDefaultDecisionStatus(*this, Coordinator->GetCurrentStatus());
	TestEqual(TEXT("Status reason retained"), Coordinator->GetCurrentStatus().LastRefreshReason, FString(TEXT("turn_interaction_model_missing")));
	TestFalse(TEXT("Visual not partially refreshed"), VisualController->HasReceivedPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorMissingVisualFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.MissingVisualFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorMissingVisualFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(Model);
	const FWBRuntimeInteractionRefreshResult Result = Coordinator->RefreshDecisionPoint(
		{ MakeDecisionMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("visual_controller_missing")));
	TestFalse(TEXT("No current decision point"), Coordinator->HasCurrentDecisionPoint());
	ExpectDefaultDecisionStatus(*this, Coordinator->GetCurrentStatus());
	TestEqual(TEXT("Status reason retained"), Coordinator->GetCurrentStatus().LastRefreshReason, FString(TEXT("visual_controller_missing")));
	TestFalse(TEXT("Model not partially refreshed"), Model->HasCurrentInteractionState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorNoTargetsFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.NoTargetsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorNoTargetsFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	UWBRuntimeVisualControllerComponent* VisualController = MakeDecisionVisualController();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Visual controller"), VisualController);
	if (Coordinator == nullptr || Model == nullptr || VisualController == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->SetVisualController(VisualController);
	Coordinator->bRefreshTurnInteractionModel = false;
	Coordinator->bRefreshVisualController = false;
	const FWBRuntimeInteractionRefreshResult Result = Coordinator->RefreshDecisionPoint(
		{ MakeDecisionMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("no_refresh_targets_enabled")));
	TestFalse(TEXT("No current decision point"), Coordinator->HasCurrentDecisionPoint());
	ExpectDefaultDecisionStatus(*this, Coordinator->GetCurrentStatus());
	TestEqual(TEXT("Status reason retained"), Coordinator->GetCurrentStatus().LastRefreshReason, FString(TEXT("no_refresh_targets_enabled")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorClearTest, "Wandbound.Runtime.DecisionPointCoordinator.ClearDecisionPointClearsModel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorClearTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MakeDecisionMoveAction() }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestTrue(TEXT("Current before clear"), Coordinator->HasCurrentDecisionPoint());
	TestTrue(TEXT("Model current before clear"), Model->HasCurrentInteractionState());

	Coordinator->ClearDecisionPoint();
	TestFalse(TEXT("No current decision point"), Coordinator->HasCurrentDecisionPoint());
	ExpectDefaultDecisionStatus(*this, Coordinator->GetCurrentStatus());
	TestFalse(TEXT("Model cleared"), Model->HasCurrentInteractionState());
	TestNull(TEXT("Snapshot unavailable"), Coordinator->GetCurrentPresentationSnapshot());
	TestFalse(TEXT("Last refresh reset"), Coordinator->GetLastRefreshResult().bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorTryFindDelegatesTest, "Wandbound.Runtime.DecisionPointCoordinator.TryFindPresentationEntryDelegates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorTryFindDelegatesTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeDecisionMoveAction();
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MoveAction }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	FWBRuntimeActionPresentationEntry Entry;
	TestTrue(TEXT("Existing action found"), Coordinator->TryFindPresentationEntryByActionId(WBActionCodec::MakeActionId(MoveAction), Entry));
	TestEqual(TEXT("Found action id"), Entry.ActionId, WBActionCodec::MakeActionId(MoveAction));
	TestFalse(TEXT("Missing action not found"), Coordinator->TryFindPresentationEntryByActionId(TEXT("end_turn:p0"), Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorFailedRefreshResetsStatusTest, "Wandbound.Runtime.DecisionPointCoordinator.FailedRefreshResetsStaleStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorFailedRefreshResetsStatusTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint(MakeDecisionActionList(), WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestTrue(TEXT("Successful current decision point"), Coordinator->HasCurrentDecisionPoint());
	TestNotNull(TEXT("Snapshot available"), Coordinator->GetCurrentPresentationSnapshot());

	Coordinator->bRefreshVisualController = true;
	const FWBRuntimeInteractionRefreshResult FailedResult = Coordinator->RefreshDecisionPoint(
		{ MakeDecisionMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	TestFalse(TEXT("Failed refresh"), FailedResult.bOk);
	TestFalse(TEXT("Current decision point cleared"), Coordinator->HasCurrentDecisionPoint());
	ExpectDefaultDecisionStatus(*this, Coordinator->GetCurrentStatus());
	TestEqual(TEXT("Failure reason retained"), Coordinator->GetCurrentStatus().LastRefreshReason, FString(TEXT("visual_controller_missing")));
	TestNull(TEXT("Stale entries not exposed"), Coordinator->GetCurrentPresentationSnapshot());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteNoDecisionPointFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteWithoutCurrentDecisionPointFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteNoDecisionPointFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(Model);
	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		TEXT("move:p0:u1:3,4->4,4"),
		Context,
		Facade);

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("no_current_decision_point")));
	ExpectDecisionStateUnchanged(*this, State, Before);
	TestTrue(TEXT("Attempt stored"), Coordinator->HasLastSelectedActionExecution());
	TestTrue(TEXT("Status has attempted id"), Coordinator->GetCurrentStatus().bHasLastSelectedAction);
	TestEqual(TEXT("Status selected id"), Coordinator->GetCurrentStatus().LastSelectedActionId, FString(TEXT("move:p0:u1:3,4->4,4")));
	TestEqual(TEXT("Status selected reason"), Coordinator->GetCurrentStatus().LastSelectedActionReason, FString(TEXT("no_current_decision_point")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteMissingModelFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteMissingModelFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteMissingModelFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MakeDecisionMoveAction() }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestTrue(TEXT("Current decision point exists"), Coordinator->HasCurrentDecisionPoint());
	Coordinator->SetTurnInteractionModel(nullptr);

	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(MakeDecisionMoveAction()),
		Context,
		Facade);

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("turn_interaction_model_missing")));
	ExpectDecisionStateUnchanged(*this, State, Before);
	TestTrue(TEXT("Attempt stored"), Coordinator->HasLastSelectedActionExecution());
	TestEqual(TEXT("Status selected reason"), Coordinator->GetCurrentStatus().LastSelectedActionReason, FString(TEXT("turn_interaction_model_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteMissingActionFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteMissingActionIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteMissingActionFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MakeDecisionMoveAction() }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		TEXT("end_turn:p0"),
		Context,
		Facade);

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("selected_action_id_not_found")));
	ExpectDecisionStateUnchanged(*this, State, Before);
	TestTrue(TEXT("Attempt stored"), Coordinator->HasLastSelectedActionExecution());
	TestTrue(TEXT("Status has last selected action"), Coordinator->GetCurrentStatus().bHasLastSelectedAction);
	TestEqual(TEXT("Status selected id"), Coordinator->GetCurrentStatus().LastSelectedActionId, FString(TEXT("end_turn:p0")));
	TestFalse(TEXT("Status selection unresolved"), Coordinator->GetCurrentStatus().bLastSelectedActionResolved);
	TestEqual(TEXT("Status selected reason"), Coordinator->GetCurrentStatus().LastSelectedActionReason, FString(TEXT("selected_action_id_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteMoveSucceedsTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteMoveSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteMoveSucceedsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeDecisionMoveAction();
	const FString MoveId = WBActionCodec::MakeActionId(MoveAction);
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MoveAction }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		MoveId,
		Context,
		Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Move succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Unit moved x"), State.GetUnitById(1)->X, 4);
	TestEqual(TEXT("Unit moved y"), State.GetUnitById(1)->Y, 4);
	TestTrue(TEXT("Last result stored"), Coordinator->HasLastSelectedActionExecution());
	TestEqual(TEXT("Stored selected id"), Coordinator->GetLastSelectedActionExecutionResult().Selection.SelectedActionId, MoveId);
	TestTrue(TEXT("Status selection resolved"), Coordinator->GetCurrentStatus().bLastSelectedActionResolved);
	TestTrue(TEXT("Decision point remains ready"), Coordinator->GetCurrentStatus().bReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteFullEndTurnSucceedsTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteFullEndTurnSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteFullEndTurnSucceedsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction EndTurnAction = MakeDecisionEndTurnAction();
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ EndTurnAction }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(EndTurnAction),
		Context,
		Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Full EndTurn succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Roll consumed"), Result.Execution.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Roll value"), Result.Execution.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestTrue(TEXT("Status selection resolved"), Coordinator->GetCurrentStatus().bLastSelectedActionResolved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteInvalidFullEndTurnContextFailsTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteInvalidFullEndTurnContextFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteInvalidFullEndTurnContextFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction EndTurnAction = MakeDecisionEndTurnAction();
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ EndTurnAction }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(EndTurnAction),
		Context,
		Facade);

	TestTrue(TEXT("Selection resolves"), Result.Selection.bOk);
	TestFalse(TEXT("Runtime execution fails"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Failure reason"), Result.Execution.RuntimeResult.ApplyResult.Reason, FString(TEXT("missing_mp_roll_source")));
	ExpectDecisionStateUnchanged(*this, State, Before);
	TestTrue(TEXT("Status selection resolved"), Coordinator->GetCurrentStatus().bLastSelectedActionResolved);
	TestEqual(TEXT("Status selected reason"), Coordinator->GetCurrentStatus().LastSelectedActionReason, FString(TEXT("missing_mp_roll_source")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorClearLastSelectedActionTest, "Wandbound.Runtime.DecisionPointCoordinator.ClearLastSelectedActionExecution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorClearLastSelectedActionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeDecisionMoveAction();
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MoveAction }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	Coordinator->ExecuteSelectedActionId(State, WBActionCodec::MakeActionId(MoveAction), Context, Facade);
	TestTrue(TEXT("Last execution exists"), Coordinator->HasLastSelectedActionExecution());

	Coordinator->ClearLastSelectedActionExecution();
	TestFalse(TEXT("Last execution cleared"), Coordinator->HasLastSelectedActionExecution());
	TestFalse(TEXT("Status selected action cleared"), Coordinator->GetCurrentStatus().bHasLastSelectedAction);
	TestTrue(TEXT("Current decision point remains"), Coordinator->HasCurrentDecisionPoint());
	TestNotNull(TEXT("Snapshot still available"), Coordinator->GetCurrentPresentationSnapshot());
	TestEqual(TEXT("Snapshot count unchanged"), Coordinator->GetCurrentPresentationSnapshot()->Entries.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorExecuteDoesNotRegenerateSnapshotTest, "Wandbound.Runtime.DecisionPointCoordinator.ExecuteDoesNotRegeneratePresentationSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorExecuteDoesNotRegenerateSnapshotTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeDecisionMoveAction();
	const FString MoveId = WBActionCodec::MakeActionId(MoveAction);
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MoveAction }, WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	const FWBRuntimeLegalActionPresentationSnapshot* SnapshotBefore = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Snapshot before execute"), SnapshotBefore);
	if (SnapshotBefore == nullptr)
	{
		return false;
	}
	const FString SnapshotActionIdBefore = SnapshotBefore->Entries[0].ActionId;
	const FWBTile SnapshotToTileBefore = SnapshotBefore->Entries[0].ToTile;

	Coordinator->ExecuteSelectedActionId(State, MoveId, Context, Facade);

	const FWBRuntimeLegalActionPresentationSnapshot* SnapshotAfter = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Snapshot after execute"), SnapshotAfter);
	if (SnapshotAfter == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Snapshot count unchanged"), SnapshotAfter->Entries.Num(), 1);
	TestEqual(TEXT("Snapshot action id unchanged"), SnapshotAfter->Entries[0].ActionId, SnapshotActionIdBefore);
	TestEqual(TEXT("Snapshot to x unchanged"), SnapshotAfter->Entries[0].ToTile.X, SnapshotToTileBefore.X);
	TestEqual(TEXT("Snapshot to y unchanged"), SnapshotAfter->Entries[0].ToTile.Y, SnapshotToTileBefore.Y);
	TestEqual(TEXT("State moved"), State.GetUnitById(1)->X, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorNoActionGenerationSourceGuardTest, "Wandbound.Runtime.DecisionPointCoordinator.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Coordinator source loads"), LoadDecisionCoordinatorSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call CanMoveUnit"), Source.Contains(TEXT("CanMoveUnit")));
	TestFalse(TEXT("Does not call CanDeclareAttack"), Source.Contains(TEXT("CanDeclareAttack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorNoActionExecutionSourceGuardTest, "Wandbound.Runtime.DecisionPointCoordinator.NoActionExecutionSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorNoActionExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Coordinator source loads"), LoadDecisionCoordinatorSource(Source));
	TestTrue(TEXT("Delegates selected action ids through interaction model"), Source.Contains(TEXT("TurnInteractionModel->ExecuteSelectedActionId")));
	TestFalse(TEXT("Does not apply selected actions"), Source.Contains(TEXT("ApplySelectedAction")));
	TestFalse(TEXT("Does not apply runtime selected actions"), Source.Contains(TEXT("ApplyRuntimeSelectedAction")));
	TestFalse(TEXT("Does not mention effect runner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not call action-selection bridge directly"), Source.Contains(TEXT("WBRuntimeActionSelectionBridge::")));
	TestFalse(TEXT("Does not call selected-action visual harness"), Source.Contains(TEXT("WBSelectedActionVisualHarness")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorNoGameStateOwnershipSourceGuardTest, "Wandbound.Runtime.DecisionPointCoordinator.NoGameStateOwnershipSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorNoGameStateOwnershipSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Coordinator source loads"), LoadDecisionCoordinatorSource(Source));
	TestTrue(TEXT("Execution handoff accepts external state by reference"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No game state pointer storage"), Source.Contains(TEXT("GameStateData*")));
	TestFalse(TEXT("No current game state member"), Source.Contains(TEXT("FWBGameStateData Current")));
	TestFalse(TEXT("No stored game state member"), Source.Contains(TEXT("FWBGameStateData Stored")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorHandoffHiddenDataExcludedTest, "Wandbound.Runtime.DecisionPointCoordinator.SelectedActionHandoffHiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorHandoffHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	FWBGameStateData State = MakeDecisionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	FWBPublicBoardSummary PublicSummary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	PublicSummary.Units[0].CardId = TEXT("visible_decision_handoff_player");
	PublicSummary.Units[1].CardId = TEXT("visible_decision_handoff_target");

	const FWBAction MoveAction = MakeDecisionMoveAction();
	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MoveAction }, PublicSummary);
	const FWBRuntimeActionSelectionExecutionResult Result = Coordinator->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(MoveAction),
		Context,
		Facade);

	TestTrue(TEXT("Move succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	const FString PublicText = CombineDecisionExecutionPublicText(Coordinator->GetCurrentStatus(), Result);
	TestFalse(TEXT("Player deck secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HANDOFF_DECK")));
	TestFalse(TEXT("Player hand secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HANDOFF_HAND")));
	TestFalse(TEXT("Player discard secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HANDOFF_DISCARD")));
	TestFalse(TEXT("Opponent deck secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HANDOFF_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HANDOFF_OPPONENT_HAND")));
	TestFalse(TEXT("Opponent discard secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HANDOFF_OPPONENT_DISCARD")));
	TestTrue(TEXT("Visible card remains public"), PublicText.Contains(TEXT("visible_decision_handoff_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointCoordinatorHiddenDataExcludedTest, "Wandbound.Runtime.DecisionPointCoordinator.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointCoordinatorHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeDecisionCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeDecisionInteractionModel();
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	FWBPublicBoardSummary PublicSummary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	PublicSummary.Units[0].CardId = TEXT("visible_decision_player");
	PublicSummary.Units[1].CardId = TEXT("visible_decision_target");

	Coordinator->bRefreshVisualController = false;
	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->RefreshDecisionPoint({ MakeDecisionMoveAction(), MakeDecisionAttackAction() }, PublicSummary);

	const FWBRuntimeLegalActionPresentationSnapshot* Snapshot = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Snapshot exists"), Snapshot);
	if (Snapshot == nullptr)
	{
		return false;
	}

	const FString PublicText = CombineDecisionSnapshotPublicText(*Snapshot);
	TestFalse(TEXT("Deck secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_DECK")));
	TestFalse(TEXT("Hand secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_HAND")));
	TestFalse(TEXT("Discard secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_DISCARD")));
	TestFalse(TEXT("Hidden marker secret absent"), PublicText.Contains(TEXT("SECRET_DECISION_MARKER_ID")));
	TestTrue(TEXT("Visible player card present"), PublicText.Contains(TEXT("visible_decision_player")));
	TestTrue(TEXT("Visible target card present"), PublicText.Contains(TEXT("visible_decision_target")));
	return true;
}

#endif
