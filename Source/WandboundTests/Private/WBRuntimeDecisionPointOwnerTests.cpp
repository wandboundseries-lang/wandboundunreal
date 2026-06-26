#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeDecisionLoopTestHarness.h"
#include "WBRuntimeDecisionPointCoordinatorComponent.h"
#include "WBRuntimeDecisionPointOwnerComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeDecisionPointOwnerComponent* MakeOwnerShell()
{
	return NewObject<UWBRuntimeDecisionPointOwnerComponent>(GetTransientPackage());
}

UWBRuntimeDecisionPointCoordinatorComponent* MakeOwnerCoordinator()
{
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator =
		NewObject<UWBRuntimeDecisionPointCoordinatorComponent>(GetTransientPackage());
	if (Coordinator != nullptr)
	{
		Coordinator->bRefreshVisualController = false;
	}
	return Coordinator;
}

UWBRuntimeTurnInteractionModelComponent* MakeOwnerInteractionModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeControllerFacadeComponent* MakeOwnerFacade()
{
	return NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
}

bool MakeOwnerPath(
	UWBRuntimeDecisionPointOwnerComponent*& OutOwner,
	UWBRuntimeDecisionPointCoordinatorComponent*& OutCoordinator,
	UWBRuntimeTurnInteractionModelComponent*& OutModel,
	UWBRuntimeControllerFacadeComponent*& OutFacade)
{
	OutOwner = MakeOwnerShell();
	OutCoordinator = MakeOwnerCoordinator();
	OutModel = MakeOwnerInteractionModel();
	OutFacade = MakeOwnerFacade();

	if (OutOwner == nullptr || OutCoordinator == nullptr || OutModel == nullptr || OutFacade == nullptr)
	{
		return false;
	}

	OutCoordinator->SetTurnInteractionModel(OutModel);
	OutOwner->SetDecisionPointCoordinator(OutCoordinator);
	OutOwner->SetRuntimeControllerFacade(OutFacade);
	return true;
}

FWBPlayerStateData MakeOwnerPlayer(
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

FWBUnitState MakeOwnerUnit(
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

FWBGameStateData MakeOwnerState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeOwnerPlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_OWNER_DECK"));
	Player0.Hand.Add(TEXT("SECRET_OWNER_HAND"));
	Player0.Discard.Add(TEXT("SECRET_OWNER_DISCARD"));

	FWBPlayerStateData Player1 = MakeOwnerPlayer(1);
	Player1.Deck.Add(TEXT("SECRET_OWNER_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_OWNER_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_OWNER_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeOwnerUnit(1, 0, FWBTile(3, 4), TEXT("visible_owner_player")));
	State.AddUnitForTest(MakeOwnerUnit(2, 1, FWBTile(5, 4), TEXT("visible_owner_target")));
	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

void ExpectOwnerStateUnchanged(
	FAutomationTestBase& Test,
	const FWBGameStateData& State,
	const FWBGameStateData& Before)
{
	Test.TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	Test.TestEqual(TEXT("Priority player unchanged"), State.PriorityPlayer, Before.PriorityPlayer);
	Test.TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	Test.TestEqual(TEXT("Player 0 MP unchanged"), State.GetPlayerById(0)->RemainingMP, Before.GetPlayerById(0)->RemainingMP);
	Test.TestEqual(TEXT("Unit x unchanged"), State.GetUnitById(1)->X, Before.GetUnitById(1)->X);
	Test.TestEqual(TEXT("Unit y unchanged"), State.GetUnitById(1)->Y, Before.GetUnitById(1)->Y);
}

TArray<FWBAction> MakeOwnerExternalActions(const FWBGameStateData& State, const int32 PlayerId)
{
	return FWBDecisionLoopTestHarness::GenerateExternalLegalActionsForTest(State, PlayerId);
}

FWBPublicBoardSummary MakeOwnerExternalSummary(const FWBGameStateData& State)
{
	return FWBDecisionLoopTestHarness::BuildExternalPublicSummaryForTest(State);
}

bool FindOwnerActionIdOfType(
	UWBRuntimeDecisionPointOwnerComponent* Owner,
	const EWBRuntimeActionPresentationType Type,
	FString& OutActionId)
{
	if (Owner == nullptr || Owner->GetDecisionPointCoordinator() == nullptr)
	{
		return false;
	}

	const FWBRuntimeLegalActionPresentationSnapshot* Snapshot =
		Owner->GetDecisionPointCoordinator()->GetCurrentPresentationSnapshot();
	if (Snapshot == nullptr)
	{
		return false;
	}

	return FWBDecisionLoopTestHarness::FindFirstPresentedActionIdOfType(*Snapshot, Type, OutActionId);
}

FString CombineOwnerPublicText(
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

void ExpectNoOwnerHiddenTokens(FAutomationTestBase& Test, const FString& PublicText)
{
	Test.TestFalse(TEXT("Player deck secret absent"), PublicText.Contains(TEXT("SECRET_OWNER_DECK")));
	Test.TestFalse(TEXT("Player hand secret absent"), PublicText.Contains(TEXT("SECRET_OWNER_HAND")));
	Test.TestFalse(TEXT("Player discard secret absent"), PublicText.Contains(TEXT("SECRET_OWNER_DISCARD")));
	Test.TestFalse(TEXT("Opponent deck secret absent"), PublicText.Contains(TEXT("SECRET_OWNER_OPPONENT_DECK")));
	Test.TestFalse(TEXT("Opponent hand secret absent"), PublicText.Contains(TEXT("SECRET_OWNER_OPPONENT_HAND")));
	Test.TestFalse(TEXT("Opponent discard secret absent"), PublicText.Contains(TEXT("SECRET_OWNER_OPPONENT_DISCARD")));
}

bool LoadOwnerSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Public"),
		TEXT("WBRuntimeDecisionPointOwnerComponent.h"));
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Source"),
		TEXT("WandboundRuntime"),
		TEXT("Private"),
		TEXT("WBRuntimeDecisionPointOwnerComponent.cpp"));

	FString Header;
	FString Source;
	if (!FFileHelper::LoadFileToString(Header, *HeaderPath)
		|| !FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		return false;
	}

	OutSource = Header + Source;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerClassExistsTest, "Wandbound.Runtime.DecisionPointOwner.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Decision point owner class"), UWBRuntimeDecisionPointOwnerComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerInitialStateTest, "Wandbound.Runtime.DecisionPointOwner.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerInitialStateTest::RunTest(const FString& Parameters)
{
	const UWBRuntimeDecisionPointOwnerComponent* Owner = MakeOwnerShell();
	TestNotNull(TEXT("Owner"), Owner);
	if (Owner == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No current decision point"), Owner->HasCurrentDecisionPoint());
	TestFalse(TEXT("No last execution result"), Owner->HasLastExecutionResult());
	TestFalse(TEXT("Current status not ready"), Owner->GetCurrentStatus().bReady);
	TestFalse(TEXT("Last refresh not ok"), Owner->GetLastRefreshResult().bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerRefreshSucceedsTest, "Wandbound.Runtime.DecisionPointOwner.RefreshFromExternalDataSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerRefreshSucceedsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));

	const TArray<FWBAction> Actions = MakeOwnerExternalActions(State, 0);
	const FWBPublicBoardSummary Summary = MakeOwnerExternalSummary(State);
	const FWBRuntimeInteractionRefreshResult Result =
		Owner->RefreshDecisionPointFromExternalData(Actions, Summary);

	TestTrue(TEXT("Refresh succeeds"), Result.bOk);
	TestTrue(TEXT("Owner has current decision point"), Owner->HasCurrentDecisionPoint());
	TestTrue(TEXT("Status ready"), Owner->GetCurrentStatus().bReady);
	TestEqual(TEXT("Legal action count"), Owner->GetCurrentStatus().LegalActionCount, Actions.Num());
	TestEqual(TEXT("Public unit count"), Owner->GetCurrentStatus().PublicUnitCount, Summary.Units.Num());
	TestEqual(TEXT("Public wall count"), Owner->GetCurrentStatus().PublicWallCount, Summary.Walls.Num());
	TestEqual(TEXT("Public terrain count"), Owner->GetCurrentStatus().PublicTerrainCount, Summary.TerrainTiles.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerRefreshMissingCoordinatorFailsTest, "Wandbound.Runtime.DecisionPointOwner.RefreshMissingCoordinatorFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerRefreshMissingCoordinatorFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeOwnerShell();
	TestNotNull(TEXT("Owner"), Owner);
	if (Owner == nullptr)
	{
		return false;
	}

	const FWBRuntimeInteractionRefreshResult Result = Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State));
	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("decision_point_coordinator_missing")));
	TestFalse(TEXT("No current decision point"), Owner->HasCurrentDecisionPoint());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteMissingCoordinatorFailsTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteMissingCoordinatorFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteMissingCoordinatorFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	const FWBGameStateData Before = State;
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeOwnerShell();
	FWBRuntimeTurnResolutionContext Context;
	TestNotNull(TEXT("Owner"), Owner);
	if (Owner == nullptr)
	{
		return false;
	}

	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionId(State, TEXT("move:p0:u1:3,4->4,4"), Context);
	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("decision_point_coordinator_missing")));
	TestTrue(TEXT("Execution result stored"), Owner->HasLastExecutionResult());
	ExpectOwnerStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteMissingFacadeFailsTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteMissingFacadeFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteMissingFacadeFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	const FWBGameStateData Before = State;
	UWBRuntimeDecisionPointOwnerComponent* Owner = MakeOwnerShell();
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = MakeOwnerCoordinator();
	UWBRuntimeTurnInteractionModelComponent* Model = MakeOwnerInteractionModel();
	TestNotNull(TEXT("Owner"), Owner);
	TestNotNull(TEXT("Coordinator"), Coordinator);
	TestNotNull(TEXT("Model"), Model);
	if (Owner == nullptr || Coordinator == nullptr || Model == nullptr)
	{
		return false;
	}

	Coordinator->SetTurnInteractionModel(Model);
	Owner->SetDecisionPointCoordinator(Coordinator);
	TestTrue(TEXT("Refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionId(State, MoveId, Context);

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("runtime_controller_facade_missing")));
	ExpectOwnerStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteWithoutDecisionPointFailsTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteWithoutCurrentDecisionPointFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteWithoutDecisionPointFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	const FWBGameStateData Before = State;
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));

	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionId(State, TEXT("move:p0:u1:3,4->4,4"), Context);
	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("no_current_decision_point")));
	ExpectOwnerStateUnchanged(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteMoveSucceedsTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteMoveSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteMoveSucceedsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionId(State, MoveId, Context);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Move executes"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Execution result stored"), Owner->HasLastExecutionResult());
	TestEqual(TEXT("Unit moved x"), State.GetUnitById(1)->X, 4);
	TestTrue(TEXT("Owner still has stale decision point"), Owner->HasCurrentDecisionPoint());
	TestNotNull(TEXT("Stale snapshot still exists"), Coordinator->GetCurrentPresentationSnapshot());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteAndRefreshSucceedsTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshFromExternalDataSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteAndRefreshSucceedsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	FWBRuntimeTurnResolutionContext Context;
	TArray<FWBAction> SuppliedPostActions;
	FWBPublicBoardSummary SuppliedPostSummary;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionIdAndRefreshFromExternalData(
			State,
			MoveId,
			Context,
			SuppliedPostActions,
			SuppliedPostSummary);

	TestTrue(TEXT("Move executes"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Post-action refresh succeeds"), Owner->GetLastRefreshResult().bOk);
	TestEqual(TEXT("Status legal action count reflects supplied data"), Owner->GetCurrentStatus().LegalActionCount, 0);
	TestEqual(TEXT("Status public unit count reflects supplied data"), Owner->GetCurrentStatus().PublicUnitCount, 0);
	TestTrue(TEXT("Execution result stored"), Owner->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteAndRefreshUsesSuppliedPostDataTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshUsesSuppliedPostData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteAndRefreshUsesSuppliedPostDataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	FWBRuntimeTurnResolutionContext Context;
	TArray<FWBAction> SuppliedPostActions;
	SuppliedPostActions.Add(MakeOwnerExternalActions(State, 0).Last());
	FWBPublicBoardSummary SuppliedPostSummary;
	FWBPublicUnitBoardSummary SuppliedUnit;
	SuppliedUnit.UnitId = 99;
	SuppliedUnit.OwnerId = 0;
	SuppliedUnit.CardId = TEXT("visible_supplied_post_action_unit");
	SuppliedUnit.X = 1;
	SuppliedUnit.Y = 1;
	SuppliedPostSummary.Units.Add(SuppliedUnit);
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionIdAndRefreshFromExternalData(
			State,
			MoveId,
			Context,
			SuppliedPostActions,
			SuppliedPostSummary);

	TestTrue(TEXT("Move executes"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Post-action refresh succeeds"), Owner->GetLastRefreshResult().bOk);
	TestEqual(TEXT("Status reflects supplied legal action count"), Owner->GetCurrentStatus().LegalActionCount, SuppliedPostActions.Num());
	TestEqual(TEXT("Status reflects supplied public unit count"), Owner->GetCurrentStatus().PublicUnitCount, SuppliedPostSummary.Units.Num());
	TestTrue(TEXT("Execution result stored"), Owner->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteAndRefreshSkipsOnFailureTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshSkipsRefreshOnFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteAndRefreshSkipsOnFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);
	const FWBRuntimeInteractionRefreshResult LastRefreshBefore = Owner->GetLastRefreshResult();

	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionIdAndRefreshFromExternalData(
			State,
			TEXT("missing_action_id"),
			Context,
			TArray<FWBAction>(),
			FWBPublicBoardSummary());

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Failure reason"), Result.Selection.Reason, FString(TEXT("selected_action_id_not_found")));
	TestTrue(TEXT("Execution result stored"), Owner->HasLastExecutionResult());
	TestEqual(TEXT("Last refresh ok unchanged"), Owner->GetLastRefreshResult().bOk, LastRefreshBefore.bOk);
	TestEqual(TEXT("Last refresh legal count unchanged"), Owner->GetLastRefreshResult().LegalActionCount, LastRefreshBefore.LegalActionCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerExecuteAndRefreshStoresRefreshFailureTest, "Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshStoresRefreshFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerExecuteAndRefreshStoresRefreshFailureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	Coordinator->bRefreshTurnInteractionModel = false;
	Coordinator->bRefreshVisualController = false;

	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionIdAndRefreshFromExternalData(
			State,
			MoveId,
			Context,
			MakeOwnerExternalActions(State, 0),
			MakeOwnerExternalSummary(State));

	TestTrue(TEXT("Move selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Move executes"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Execution result remains stored"), Owner->HasLastExecutionResult());
	TestFalse(TEXT("Post-action refresh fails"), Owner->GetLastRefreshResult().bOk);
	TestEqual(TEXT("Refresh failure reason"), Owner->GetLastRefreshResult().Reason, FString(TEXT("no_refresh_targets_enabled")));
	TestFalse(TEXT("Current decision point follows coordinator failure policy"), Owner->HasCurrentDecisionPoint());
	TestEqual(TEXT("Status stores refresh failure reason"), Owner->GetCurrentStatus().LastRefreshReason, FString(TEXT("no_refresh_targets_enabled")));
	TestEqual(TEXT("State still moved"), State.GetUnitById(1)->X, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerFullEndTurnTest, "Wandbound.Runtime.DecisionPointOwner.FullEndTurnRefreshesNextPlayer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerFullEndTurnTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString EndTurnId;
	TestTrue(TEXT("EndTurn exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::EndTurn, EndTurnId));
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;

	FWBGameStateData SuppliedPostState = State;
	SuppliedPostState.CurrentPlayer = 1;
	SuppliedPostState.PriorityPlayer = 1;
	SuppliedPostState.TurnNumber = State.TurnNumber + 1;
	if (FWBPlayerStateData* Player1 = SuppliedPostState.GetMutablePlayerById(1))
	{
		Player1->RemainingMP = 4;
		Player1->LastMPRoll = 4;
	}
	const TArray<FWBAction> NextActions = MakeOwnerExternalActions(SuppliedPostState, 1);
	const FWBPublicBoardSummary NextSummary = MakeOwnerExternalSummary(SuppliedPostState);

	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionIdAndRefreshFromExternalData(
			State,
			EndTurnId,
			Context,
			NextActions,
			NextSummary);
	TestTrue(TEXT("EndTurn selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Full EndTurn succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("MP roll consumed"), Result.Execution.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Consumed roll"), Result.Execution.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestTrue(TEXT("Next-player refresh succeeds"), Owner->GetLastRefreshResult().bOk);
	TestEqual(TEXT("Next action count"), Owner->GetCurrentStatus().LegalActionCount, NextActions.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerNoActionGenerationSourceGuardTest, "Wandbound.Runtime.DecisionPointOwner.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Owner source loads"), LoadOwnerSource(Source));
	TestFalse(TEXT("Owner does not generate legal actions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Owner does not include WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Owner does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Owner does not call CanMoveUnit"), Source.Contains(TEXT("CanMoveUnit")));
	TestFalse(TEXT("Owner does not call CanDeclareAttack"), Source.Contains(TEXT("CanDeclareAttack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerNoEffectRunnerSourceGuardTest, "Wandbound.Runtime.DecisionPointOwner.NoEffectRunnerSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerNoEffectRunnerSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Owner source loads"), LoadOwnerSource(Source));
	TestFalse(TEXT("Owner does not call WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestTrue(TEXT("Owner delegates through coordinator"), Source.Contains(TEXT("DecisionPointCoordinator->ExecuteSelectedActionId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerNoGameStateOwnershipSourceGuardTest, "Wandbound.Runtime.DecisionPointOwner.NoGameStateOwnershipSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerNoGameStateOwnershipSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Owner source loads"), LoadOwnerSource(Source));
	TestTrue(TEXT("Execution accepts external state by reference"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No game state pointer storage"), Source.Contains(TEXT("FWBGameStateData*")));
	TestFalse(TEXT("No current game state member"), Source.Contains(TEXT("FWBGameStateData Current")));
	TestFalse(TEXT("No stored game state member"), Source.Contains(TEXT("FWBGameStateData Stored")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand member access"), Source.Contains(TEXT(".Hand")) || Source.Contains(TEXT("->Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerHiddenDataExcludedTest, "Wandbound.Runtime.DecisionPointOwner.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	FWBRuntimeTurnResolutionContext Context;
	const FWBRuntimeActionSelectionExecutionResult Result =
		Owner->ExecuteSelectedActionId(State, MoveId, Context);
	TestTrue(TEXT("Move executes"), Result.Execution.RuntimeResult.ApplyResult.bOk);

	const FString PublicText = CombineOwnerPublicText(Owner->GetCurrentStatus(), Result);
	ExpectNoOwnerHiddenTokens(*this, PublicText);
	TestTrue(TEXT("Visible public card present"), PublicText.Contains(TEXT("visible_owner_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeDecisionPointOwnerClearTest, "Wandbound.Runtime.DecisionPointOwner.ClearClearsLocalAndCoordinatorState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeDecisionPointOwnerClearTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOwnerState();
	UWBRuntimeDecisionPointOwnerComponent* Owner = nullptr;
	UWBRuntimeDecisionPointCoordinatorComponent* Coordinator = nullptr;
	UWBRuntimeTurnInteractionModelComponent* Model = nullptr;
	UWBRuntimeControllerFacadeComponent* Facade = nullptr;
	TestTrue(TEXT("Owner path created"), MakeOwnerPath(Owner, Coordinator, Model, Facade));
	TestTrue(TEXT("Initial refresh succeeds"), Owner->RefreshDecisionPointFromExternalData(
		MakeOwnerExternalActions(State, 0),
		MakeOwnerExternalSummary(State)).bOk);

	FString MoveId;
	TestTrue(TEXT("Move exists"), FindOwnerActionIdOfType(Owner, EWBRuntimeActionPresentationType::Move, MoveId));
	FWBRuntimeTurnResolutionContext Context;
	Owner->ExecuteSelectedActionId(State, MoveId, Context);
	TestTrue(TEXT("Owner has current decision point"), Owner->HasCurrentDecisionPoint());
	TestTrue(TEXT("Owner has execution result"), Owner->HasLastExecutionResult());
	TestTrue(TEXT("Coordinator has current decision point"), Coordinator->HasCurrentDecisionPoint());

	Owner->Clear();
	TestFalse(TEXT("Owner current decision point cleared"), Owner->HasCurrentDecisionPoint());
	TestFalse(TEXT("Owner execution result cleared"), Owner->HasLastExecutionResult());
	TestFalse(TEXT("Coordinator current decision point cleared"), Coordinator->HasCurrentDecisionPoint());
	TestNull(TEXT("Coordinator snapshot cleared"), Coordinator->GetCurrentPresentationSnapshot());
	return true;
}

#endif
