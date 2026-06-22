#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeDecisionLoopTestHarness.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeVisualControllerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeDecisionPointCoordinatorComponent* MakeLoopCoordinator()
{
	return NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
}

UWBRuntimeTurnInteractionModelComponent* MakeLoopInteractionModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeVisualControllerComponent* MakeLoopVisualController()
{
	return NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());
}

UWBBoardViewStateApplierComponent* MakeLoopStateApplier()
{
	return NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
}

UWBRuntimeControllerFacadeComponent* MakeLoopFacade(UWBRuntimeVisualControllerComponent* VisualController = nullptr)
{
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	if (Facade != nullptr)
	{
		Facade->SetVisualController(VisualController);
	}
	return Facade;
}

struct FLoopVisualPath
{
	AWBBoardViewActor* BoardView = nullptr;
	UWBBoardViewStateApplierComponent* Applier = nullptr;
	UWBRuntimeVisualControllerComponent* VisualController = nullptr;
};

FLoopVisualPath MakeLoopVisualPath()
{
	FLoopVisualPath Path;
	Path.BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	Path.Applier = MakeLoopStateApplier();
	Path.VisualController = MakeLoopVisualController();

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

FWBPlayerStateData MakeLoopPlayer(
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

FWBUnitState MakeLoopUnit(
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

FWBGameStateData MakeLoopState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeLoopPlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_LOOP_DECK"));
	Player0.Hand.Add(TEXT("SECRET_LOOP_HAND"));
	Player0.Discard.Add(TEXT("SECRET_LOOP_DISCARD"));

	FWBPlayerStateData Player1 = MakeLoopPlayer(1);
	Player1.Deck.Add(TEXT("SECRET_LOOP_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_LOOP_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_LOOP_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeLoopUnit(1, 0, FWBTile(3, 4), TEXT("visible_loop_player")));
	State.AddUnitForTest(MakeLoopUnit(2, 1, FWBTile(5, 4), TEXT("visible_loop_target")));
	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

bool MakeLoopCoordinatorPath(
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutModel,
	UWBRuntimeControllerFacadeComponent*& OutFacade)
{
	OutCoordinator = MakeLoopCoordinator();
	OutModel = MakeLoopInteractionModel();
	OutFacade = MakeLoopFacade();

	if (OutCoordinator == nullptr || OutModel == nullptr || OutFacade == nullptr)
	{
		return false;
	}

	OutCoordinator->bRefreshVisualController = false;
	OutCoordinator->SetTurnInteractionModel(OutModel);
	return true;
}

bool FindFirstSnapshotEntryOfType(
	const FWBRuntimeLegalActionPresentationSnapshot& Snapshot,
	const EWBRuntimeActionPresentationType Type,
	FWBRuntimeActionPresentationEntry& OutEntry)
{
	for (const FWBRuntimeActionPresentationEntry& Entry : Snapshot.Entries)
	{
		if (Entry.Type == Type)
		{
			OutEntry = Entry;
			return true;
		}
	}

	return false;
}

FString CombineLoopSnapshotPublicText(const FWBRuntimeLegalActionPresentationSnapshot& Snapshot)
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

FString CombineLoopExecutionPublicText(const FWBDecisionLoopTestHarnessStepResult& Step)
{
	FString Combined;
	Combined += Step.SelectedActionId;
	Combined += Step.ExecutionResult.Selection.SelectedActionId;
	Combined += Step.ExecutionResult.Selection.Reason;
	Combined += Step.ExecutionResult.Execution.RuntimeResult.ApplyResult.Reason;

	for (const FWBPublicUnitBoardSummary& Unit : Step.ExecutionResult.Execution.RuntimeResult.FinalPublicBoardSummary.Units)
	{
		Combined += Unit.CardId;
	}

	return Combined;
}

bool LoadRuntimeSource(FString& OutSource)
{
	const FString RuntimeRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *RuntimeRoot, TEXT("*.*"), true, false);

	OutSource.Reset();
	for (const FString& File : Files)
	{
		if (!File.EndsWith(TEXT(".h")) && !File.EndsWith(TEXT(".cpp")))
		{
			continue;
		}

		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *File))
		{
			return false;
		}
		OutSource += Contents;
	}

	return true;
}

void ExpectNoHiddenLoopTokens(FAutomationTestBase& Test, const FString& PublicText)
{
	Test.TestFalse(TEXT("Player deck secret absent"), PublicText.Contains(TEXT("SECRET_LOOP_DECK")));
	Test.TestFalse(TEXT("Player hand secret absent"), PublicText.Contains(TEXT("SECRET_LOOP_HAND")));
	Test.TestFalse(TEXT("Player discard secret absent"), PublicText.Contains(TEXT("SECRET_LOOP_DISCARD")));
	Test.TestFalse(TEXT("Opponent deck secret absent"), PublicText.Contains(TEXT("SECRET_LOOP_OPPONENT_DECK")));
	Test.TestFalse(TEXT("Opponent hand secret absent"), PublicText.Contains(TEXT("SECRET_LOOP_OPPONENT_HAND")));
	Test.TestFalse(TEXT("Opponent discard secret absent"), PublicText.Contains(TEXT("SECRET_LOOP_OPPONENT_DISCARD")));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessTestOnlyTest, "Wandbound.Runtime.DecisionLoopHarness.TestHelperIsTestOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessTestOnlyTest::RunTest(const FString& Parameters)
{
	const FString HarnessHeader = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBRuntimeDecisionLoopTestHarness.h"));
	const FString HarnessSource = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundTests"), TEXT("Private"), TEXT("WBRuntimeDecisionLoopTestHarness.cpp"));
	TestTrue(TEXT("Harness header is under WandboundTests"), FPaths::FileExists(HarnessHeader));
	TestTrue(TEXT("Harness source is under WandboundTests"), FPaths::FileExists(HarnessSource));

	FString RuntimeSource;
	TestTrue(TEXT("Runtime source loads"), LoadRuntimeSource(RuntimeSource));
	TestFalse(TEXT("Runtime does not include test harness"), RuntimeSource.Contains(TEXT("WBRuntimeDecisionLoopTestHarness")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessRefreshExposesMoveTest, "Wandbound.Runtime.DecisionLoopHarness.RefreshExposesMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessRefreshExposesMoveTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Coordinator path created"), MakeLoopCoordinatorPath(Coordinator, Model, Facade));

	const TArray<FWBAction> LegalActions = FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(State, 0);
	const FWBPublicBoardSummary Summary = FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(State);
	const FWBRuntimeInteractionRefreshResult RefreshResult = Coordinator->RefreshDecisionPoint(LegalActions, Summary);
	TestTrue(TEXT("Refresh succeeds"), RefreshResult.bOk);

	const FWBRuntimeLegalActionPresentationSnapshot* Snapshot = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Snapshot exists"), Snapshot);
	if (Snapshot == nullptr)
	{
		return false;
	}

	FString MoveId;
	TestTrue(TEXT("Snapshot contains Move"), FWBDecisionLoopTestHarness::FindFirstPresentedActionIdOfType(*Snapshot, EWBRuntimeActionPresentationType::Move, MoveId));
	TestFalse(TEXT("Move id populated"), MoveId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessMoveLeavesStaleSnapshotTest, "Wandbound.Runtime.DecisionLoopHarness.ExecuteMoveLeavesStaleSnapshotUntilRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessMoveLeavesStaleSnapshotTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Coordinator path created"), MakeLoopCoordinatorPath(Coordinator, Model, Facade));

	FWBRuntimeTurnResolutionContext Context;
	const FWBDecisionLoopTestHarnessStepResult Step = FWBDecisionLoopTestHarness::RefreshAndExecuteFirstActionOfType(
		State,
		0,
		EWBRuntimeActionPresentationType::Move,
		Context,
		Coordinator,
		Facade);

	TestTrue(TEXT("Refresh succeeds"), Step.bRefreshOk);
	TestTrue(TEXT("Move executes"), Step.bExecutionOk);
	const FWBUnitState* Unit = State.GetUnitById(1);
	TestNotNull(TEXT("Unit remains"), Unit);
	if (Unit == nullptr)
	{
		return false;
	}

	const FWBRuntimeLegalActionPresentationSnapshot* StaleSnapshot = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Stale snapshot still exists"), StaleSnapshot);
	if (StaleSnapshot == nullptr)
	{
		return false;
	}

	FWBRuntimeActionPresentationEntry StaleMove;
	TestTrue(TEXT("Stale Move still present"), FindFirstSnapshotEntryOfType(*StaleSnapshot, EWBRuntimeActionPresentationType::Move, StaleMove));
	TestEqual(TEXT("State unit moved to snapshot destination x"), Unit->X, StaleMove.ToTile.X);
	TestEqual(TEXT("State unit moved to snapshot destination y"), Unit->Y, StaleMove.ToTile.Y);
	TestEqual(TEXT("Stale move still starts at old x"), StaleMove.FromTile.X, 3);
	TestEqual(TEXT("Stale move still starts at old y"), StaleMove.FromTile.Y, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessExplicitRefreshReplacesSnapshotTest, "Wandbound.Runtime.DecisionLoopHarness.ExplicitPostActionRefreshReplacesSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessExplicitRefreshReplacesSnapshotTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Coordinator path created"), MakeLoopCoordinatorPath(Coordinator, Model, Facade));

	FWBRuntimeTurnResolutionContext Context;
	FWBDecisionLoopTestHarness::RefreshAndExecuteFirstActionOfType(
		State,
		0,
		EWBRuntimeActionPresentationType::Move,
		Context,
		Coordinator,
		Facade);

	const TArray<FWBAction> UpdatedActions = FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(State, 0);
	const FWBPublicBoardSummary UpdatedSummary = FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(State);
	const FWBRuntimeInteractionRefreshResult RefreshResult = Coordinator->RefreshDecisionPoint(UpdatedActions, UpdatedSummary);
	TestTrue(TEXT("Explicit refresh succeeds"), RefreshResult.bOk);

	const FWBRuntimeLegalActionPresentationSnapshot* UpdatedSnapshot = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Updated snapshot exists"), UpdatedSnapshot);
	if (UpdatedSnapshot == nullptr)
	{
		return false;
	}

	FWBRuntimeActionPresentationEntry UpdatedMove;
	TestTrue(TEXT("Updated Move present"), FindFirstSnapshotEntryOfType(*UpdatedSnapshot, EWBRuntimeActionPresentationType::Move, UpdatedMove));
	TestEqual(TEXT("Updated move starts at new unit x"), UpdatedMove.FromTile.X, State.GetUnitById(1)->X);
	TestEqual(TEXT("Updated move starts at new unit y"), UpdatedMove.FromTile.Y, State.GetUnitById(1)->Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessTwoDecisionLoopTest, "Wandbound.Runtime.DecisionLoopHarness.RefreshExecuteRefreshExecute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessTwoDecisionLoopTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Coordinator path created"), MakeLoopCoordinatorPath(Coordinator, Model, Facade));

	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;
	const FWBDecisionLoopTestHarnessStepResult MoveStep = FWBDecisionLoopTestHarness::RefreshAndExecuteFirstActionOfType(
		State,
		0,
		EWBRuntimeActionPresentationType::Move,
		Context,
		Coordinator,
		Facade);
	TestTrue(TEXT("Move step executes"), MoveStep.bExecutionOk);

	const TArray<FWBAction> UpdatedActions = FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(State, 0);
	const FWBPublicBoardSummary UpdatedSummary = FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(State);
	TestTrue(TEXT("Post-move refresh succeeds"), Coordinator->RefreshDecisionPoint(UpdatedActions, UpdatedSummary).bOk);

	FString EndTurnId;
	TestTrue(TEXT("EndTurn exists after refresh"), FWBDecisionLoopTestHarness::FindFirstPresentedActionIdOfType(*Coordinator->GetCurrentPresentationSnapshot(), EWBRuntimeActionPresentationType::EndTurn, EndTurnId));
	const FWBRuntimeActionSelectionExecutionResult EndTurnResult = Coordinator->ExecuteSelectedActionId(
		State,
		EndTurnId,
		Context,
		Facade);

	TestTrue(TEXT("EndTurn selection resolves"), EndTurnResult.Selection.bOk);
	if (!EndTurnResult.Execution.RuntimeResult.ApplyResult.bOk)
	{
		AddError(FString::Printf(
			TEXT("EndTurn failed in two-decision loop: %s"),
			*EndTurnResult.Execution.RuntimeResult.ApplyResult.Reason));
	}
	TestTrue(TEXT("EndTurn executes"), EndTurnResult.Execution.RuntimeResult.ApplyResult.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessFullEndTurnLoopTest, "Wandbound.Runtime.DecisionLoopHarness.FullEndTurnWithMPRollRefreshesNextPlayer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessFullEndTurnLoopTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Coordinator path created"), MakeLoopCoordinatorPath(Coordinator, Model, Facade));

	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	const FWBDecisionLoopTestHarnessStepResult EndTurnStep = FWBDecisionLoopTestHarness::RefreshAndExecuteFirstActionOfType(
		State,
		0,
		EWBRuntimeActionPresentationType::EndTurn,
		Context,
		Coordinator,
		Facade);

	TestTrue(TEXT("EndTurn refresh succeeds"), EndTurnStep.bRefreshOk);
	TestTrue(TEXT("EndTurn executes"), EndTurnStep.bExecutionOk);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player priority"), State.PriorityPlayer, 1);
	TestTrue(TEXT("MP roll consumed"), EndTurnStep.ExecutionResult.Execution.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Consumed roll value"), EndTurnStep.ExecutionResult.Execution.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Player 1 MP setup"), State.GetPlayerById(1)->RemainingMP, 4);

	const TArray<FWBAction> NextActions = FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(State, State.CurrentPlayer);
	const FWBPublicBoardSummary NextSummary = FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(State);
	TestTrue(TEXT("Next-player refresh succeeds"), Coordinator->RefreshDecisionPoint(NextActions, NextSummary).bOk);
	TestTrue(TEXT("Next-player actions exist"), Coordinator->GetCurrentStatus().LegalActionCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessVisualRefreshPathTest, "Wandbound.Runtime.DecisionLoopHarness.VisualRefreshPathParticipates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessVisualRefreshPathTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeLoopCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeLoopInteractionModel();
	const FLoopVisualPath VisualPath = MakeLoopVisualPath();
	UWBRuntimeControllerFacadeComponent* Facade = MakeLoopFacade(VisualPath.VisualController);
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Visual controller"), VisualPath.VisualController);
	TestNotNull(TEXT("Facade"), Facade);
	if (Coordinator == nullptr || Model == nullptr || VisualPath.VisualController == nullptr || Facade == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(Model);
	Coordinator->SetVisualController(VisualPath.VisualController);
	const TArray<FWBAction> Actions = FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(State, 0);
	const FWBPublicBoardSummary Summary = FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(State);
	const FWBRuntimeInteractionRefreshResult RefreshResult = Coordinator->RefreshDecisionPoint(Actions, Summary);

	TestTrue(TEXT("Refresh succeeds"), RefreshResult.bOk);
	TestTrue(TEXT("Visual rendered"), RefreshResult.VisualRefreshResult.bRendered);
	TestEqual(TEXT("Visual units match summary"), RefreshResult.VisualRefreshResult.UnitCount, Summary.Units.Num());
	TestEqual(TEXT("Visual walls match summary"), RefreshResult.VisualRefreshResult.WallCount, Summary.Walls.Num());
	TestEqual(TEXT("Visual terrain matches summary"), RefreshResult.VisualRefreshResult.TerrainCount, Summary.TerrainTiles.Num());

	FWBRuntimeTurnResolutionContext Context;
	FString MoveId;
	TestTrue(TEXT("Move exists"), FWBDecisionLoopTestHarness::FindFirstPresentedActionIdOfType(*Coordinator->GetCurrentPresentationSnapshot(), EWBRuntimeActionPresentationType::Move, MoveId));
	const FWBRuntimeActionSelectionExecutionResult MoveResult = Coordinator->ExecuteSelectedActionId(State, MoveId, Context, Facade);
	TestTrue(TEXT("Move executes"), MoveResult.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Execution visual refresh rendered"), MoveResult.Execution.VisualRefreshResult.bRendered);
	TestEqual(TEXT("Execution visual units"), MoveResult.Execution.VisualRefreshResult.UnitCount, MoveResult.Execution.RuntimeResult.FinalPublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessHiddenDataExcludedTest, "Wandbound.Runtime.DecisionLoopHarness.HiddenDataExcludedAcrossLoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLoopState();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Coordinator path created"), MakeLoopCoordinatorPath(Coordinator, Model, Facade));

	FWBRuntimeTurnResolutionContext Context;
	const FWBDecisionLoopTestHarnessStepResult Step = FWBDecisionLoopTestHarness::RefreshAndExecuteFirstActionOfType(
		State,
		0,
		EWBRuntimeActionPresentationType::Move,
		Context,
		Coordinator,
		Facade);

	TestTrue(TEXT("Move executes"), Step.bExecutionOk);
	const FWBRuntimeLegalActionPresentationSnapshot* Snapshot = Coordinator->GetCurrentPresentationSnapshot();
	TestNotNull(TEXT("Snapshot exists"), Snapshot);
	if (Snapshot == nullptr)
	{
		return false;
	}

	const FString PublicText = CombineLoopSnapshotPublicText(*Snapshot) + CombineLoopExecutionPublicText(Step);
	ExpectNoHiddenLoopTokens(*this, PublicText);
	TestTrue(TEXT("Visible public card present"), PublicText.Contains(TEXT("visible_loop_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionLoopHarnessProductionRuntimeSourceGuardTest, "Wandbound.Runtime.DecisionLoopHarness.ProductionRuntimeSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionLoopHarnessProductionRuntimeSourceGuardTest::RunTest(const FString& Parameters)
{
	FString RuntimeSource;
	TestTrue(TEXT("Runtime source loads"), LoadRuntimeSource(RuntimeSource));
	TestFalse(TEXT("Runtime does not generate legal actions"), RuntimeSource.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Runtime does not include WBRules"), RuntimeSource.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Runtime does not call WBEffectRunner"), RuntimeSource.Contains(TEXT("WBEffectRunner")));
	return true;
}

#endif
