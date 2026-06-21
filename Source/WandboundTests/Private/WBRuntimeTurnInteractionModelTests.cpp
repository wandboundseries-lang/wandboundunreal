#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeTurnInteractionModelComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeTurnInteractionModelComponent* MakeInteractionModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

FWBPlayerStateData MakeInteractionPlayer(
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

FWBUnitState MakeInteractionUnit(
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

FWBGameStateData MakeInteractionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeInteractionPlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_INTERACTION_DECK"));
	Player0.Hand.Add(TEXT("SECRET_INTERACTION_HAND"));
	Player0.Discard.Add(TEXT("SECRET_INTERACTION_DISCARD"));

	FWBPlayerStateData Player1 = MakeInteractionPlayer(1);
	Player1.Deck.Add(TEXT("SECRET_INTERACTION_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_INTERACTION_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_INTERACTION_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);

	State.AddUnitForTest(MakeInteractionUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_interaction_player")));
	State.AddUnitForTest(MakeInteractionUnit(2, 1, FWBTile(6, 4), TEXT("char_visible_interaction_target")));
	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

FWBPublicUnitBoardSummary MakeInteractionPublicUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId)
{
	FWBPublicUnitBoardSummary Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.CardId = CardId;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBPublicBoardSummary MakeInteractionPublicSummary()
{
	FWBPublicBoardSummary Summary;
	Summary.Units.Add(MakeInteractionPublicUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_interaction_player")));
	Summary.Units.Add(MakeInteractionPublicUnit(2, 1, FWBTile(6, 4), TEXT("char_visible_interaction_target")));
	return Summary;
}

FWBAction MakeInteractionMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeInteractionAttackAction()
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

FWBAction MakeInteractionEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
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

FString CombineSnapshotPublicText(const FWBRuntimeLegalActionPresentationSnapshot& Snapshot)
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

bool LoadInteractionModelSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeTurnInteractionModelComponent.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeTurnInteractionModelComponent.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelClassExistsTest, "Wandbound.Runtime.TurnInteractionModel.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Turn interaction model component class"), UWBRuntimeTurnInteractionModelComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelInitialStateTest, "Wandbound.Runtime.TurnInteractionModel.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelInitialStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No current interaction state initially"), Model->HasCurrentInteractionState());
	TestEqual(TEXT("No legal actions initially"), Model->GetCurrentLegalActions().Num(), 0);
	TestEqual(TEXT("No presentation entries initially"), Model->GetCurrentPresentationSnapshot().Entries.Num(), 0);
	TestFalse(TEXT("No last execution initially"), Model->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelSetStateBuildsSnapshotTest, "Wandbound.Runtime.TurnInteractionModel.SetStateBuildsSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelSetStateBuildsSnapshotTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const TArray<FWBAction> Actions = { MakeInteractionMoveAction(), MakeInteractionEndTurnAction() };
	Model->SetCurrentInteractionState(Actions, MakeInteractionPublicSummary());

	TestTrue(TEXT("Has current state"), Model->HasCurrentInteractionState());
	TestEqual(TEXT("Legal action count"), Model->GetCurrentLegalActions().Num(), Actions.Num());
	TestEqual(TEXT("Presentation entry count"), Model->GetCurrentPresentationSnapshot().Entries.Num(), Actions.Num());
	TestFalse(TEXT("Set clears last execution"), Model->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelPreservesOrderTest, "Wandbound.Runtime.TurnInteractionModel.PreservesInputOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelPreservesOrderTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const TArray<FWBAction> Actions = {
		MakeInteractionEndTurnAction(),
		MakeInteractionMoveAction(),
		MakeInteractionAttackAction()
	};
	Model->SetCurrentInteractionState(Actions, MakeInteractionPublicSummary());

	TestEqual(TEXT("Entry 0"), Model->GetCurrentPresentationSnapshot().Entries[0].ActionId, WBActionCodec::MakeActionId(Actions[0]));
	TestEqual(TEXT("Entry 1"), Model->GetCurrentPresentationSnapshot().Entries[1].ActionId, WBActionCodec::MakeActionId(Actions[1]));
	TestEqual(TEXT("Entry 2"), Model->GetCurrentPresentationSnapshot().Entries[2].ActionId, WBActionCodec::MakeActionId(Actions[2]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelClearStateTest, "Wandbound.Runtime.TurnInteractionModel.ClearCurrentInteractionState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelClearStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeInteractionMoveAction();
	Model->SetCurrentInteractionState({ MoveAction }, MakeInteractionPublicSummary());
	Model->ExecuteSelectedActionId(State, WBActionCodec::MakeActionId(MoveAction), Context, Facade);
	TestTrue(TEXT("Last execution exists before clear"), Model->HasLastExecutionResult());

	Model->ClearCurrentInteractionState();
	TestFalse(TEXT("Current state cleared"), Model->HasCurrentInteractionState());
	TestEqual(TEXT("Legal actions cleared"), Model->GetCurrentLegalActions().Num(), 0);
	TestEqual(TEXT("Snapshot cleared"), Model->GetCurrentPresentationSnapshot().Entries.Num(), 0);
	TestFalse(TEXT("Last execution cleared"), Model->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelTryFindSuccessTest, "Wandbound.Runtime.TurnInteractionModel.TryFindPresentationEntrySuccess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelTryFindSuccessTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeInteractionMoveAction();
	Model->SetCurrentInteractionState({ MoveAction }, MakeInteractionPublicSummary());

	FWBRuntimeActionPresentationEntry Entry;
	TestTrue(TEXT("Find succeeds"), Model->TryFindPresentationEntryByActionId(WBActionCodec::MakeActionId(MoveAction), Entry));
	TestEqual(TEXT("Found id"), Entry.ActionId, WBActionCodec::MakeActionId(MoveAction));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelTryFindMissingTest, "Wandbound.Runtime.TurnInteractionModel.TryFindPresentationEntryMissing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelTryFindMissingTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	Model->SetCurrentInteractionState({ MakeInteractionMoveAction() }, MakeInteractionPublicSummary());
	FWBRuntimeActionPresentationEntry Entry;
	TestFalse(TEXT("Find missing fails"), Model->TryFindPresentationEntryByActionId(TEXT("end_turn:p0"), Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelNoStateExecuteFailsTest, "Wandbound.Runtime.TurnInteractionModel.ExecuteWithoutStateFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelNoStateExecuteFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	const FWBGameStateData Before = State;
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBRuntimeActionSelectionExecutionResult Result = Model->ExecuteSelectedActionId(
		State,
		TEXT("end_turn:p0"),
		Context,
		Facade);

	TestFalse(TEXT("Execution fails"), Result.Selection.bOk);
	TestEqual(TEXT("Reason"), Result.Selection.Reason, FString(TEXT("no_current_interaction_state")));
	ExpectStateUnchanged(*this, State, Before);
	TestEqual(TEXT("Roll not consumed"), RollSource.NumRemainingRolls(), 1);
	TestTrue(TEXT("Last attempt stored"), Model->HasLastExecutionResult());
	TestEqual(TEXT("Last selected id"), Model->GetLastSelectedActionId(), FString(TEXT("end_turn:p0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelMissingIdFailsTest, "Wandbound.Runtime.TurnInteractionModel.ExecuteMissingIdFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelMissingIdFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	Model->SetCurrentInteractionState({ MakeInteractionMoveAction() }, MakeInteractionPublicSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Model->ExecuteSelectedActionId(
		State,
		TEXT("end_turn:p0"),
		Context,
		Facade);

	TestFalse(TEXT("Selection fails"), Result.Selection.bOk);
	TestEqual(TEXT("Missing reason"), Result.Selection.Reason, FString(TEXT("selected_action_id_not_found")));
	ExpectStateUnchanged(*this, State, Before);
	TestTrue(TEXT("Last attempt stored"), Model->HasLastExecutionResult());
	TestEqual(TEXT("Last selection reason"), Model->GetLastSelectionResult().Reason, FString(TEXT("selected_action_id_not_found")));
	TestFalse(TEXT("Last execution default failed"), Model->GetLastExecutionResult().RuntimeResult.ApplyResult.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelMoveExecutesTest, "Wandbound.Runtime.TurnInteractionModel.ExecuteMoveSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelMoveExecutesTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeInteractionMoveAction();
	const FString ActionId = WBActionCodec::MakeActionId(MoveAction);
	Model->SetCurrentInteractionState({ MoveAction, MakeInteractionEndTurnAction() }, MakeInteractionPublicSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Model->ExecuteSelectedActionId(
		State,
		ActionId,
		Context,
		Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("Move succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Unit moved x"), State.GetUnitById(1)->X, 5);
	TestEqual(TEXT("Unit moved y"), State.GetUnitById(1)->Y, 4);
	TestTrue(TEXT("Last execution stored"), Model->HasLastExecutionResult());
	TestEqual(TEXT("Last selected id"), Model->GetLastSelectedActionId(), ActionId);
	TestTrue(TEXT("Last runtime ok"), Model->GetLastExecutionResult().RuntimeResult.ApplyResult.bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelEndTurnExecutesTest, "Wandbound.Runtime.TurnInteractionModel.ExecuteFullEndTurnSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelEndTurnExecutesTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction EndTurnAction = MakeInteractionEndTurnAction();
	Model->SetCurrentInteractionState({ MakeInteractionMoveAction(), EndTurnAction }, MakeInteractionPublicSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Model->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(EndTurnAction),
		Context,
		Facade);

	TestTrue(TEXT("Selection succeeds"), Result.Selection.bOk);
	TestTrue(TEXT("EndTurn succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Consumed roll"), Result.Execution.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Consumed roll value"), Result.Execution.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestTrue(TEXT("Last execution stored"), Model->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelNullFacadeTest, "Wandbound.Runtime.TurnInteractionModel.NullFacadeFollowsBridgePolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelNullFacadeTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeInteractionMoveAction();
	Model->SetCurrentInteractionState({ MoveAction }, MakeInteractionPublicSummary());
	const FWBRuntimeActionSelectionExecutionResult Result = Model->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(MoveAction),
		Context,
		nullptr);

	TestFalse(TEXT("Null facade fails"), Result.Selection.bOk);
	TestEqual(TEXT("Null facade reason"), Result.Selection.Reason, FString(TEXT("runtime_controller_facade_missing")));
	ExpectStateUnchanged(*this, State, Before);
	TestEqual(TEXT("Last selection reason"), Model->GetLastSelectionResult().Reason, FString(TEXT("runtime_controller_facade_missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelNoActionGenerationSourceGuardTest, "Wandbound.Runtime.TurnInteractionModel.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Interaction model source loads"), LoadInteractionModelSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelNoGameStateOwnershipSourceGuardTest, "Wandbound.Runtime.TurnInteractionModel.NoGameStateOwnershipSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelNoGameStateOwnershipSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Interaction model source loads"), LoadInteractionModelSource(Source));
	TestTrue(TEXT("Execution API accepts state by reference"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No game state pointer storage"), Source.Contains(TEXT("FWBGameStateData*")));
	TestFalse(TEXT("No current game state member"), Source.Contains(TEXT("FWBGameStateData Current")));
	TestFalse(TEXT("No stored game state member"), Source.Contains(TEXT("FWBGameStateData Stored")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelHiddenDataExcludedTest, "Wandbound.Runtime.TurnInteractionModel.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData StateWithSecrets = MakeInteractionState();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	Model->SetCurrentInteractionState({ MakeInteractionMoveAction(), MakeInteractionAttackAction() }, MakeInteractionPublicSummary());
	const FString PublicText = CombineSnapshotPublicText(Model->GetCurrentPresentationSnapshot());
	TestFalse(TEXT("Player deck secret absent"), PublicText.Contains(TEXT("SECRET_INTERACTION_DECK")));
	TestFalse(TEXT("Player hand secret absent"), PublicText.Contains(TEXT("SECRET_INTERACTION_HAND")));
	TestFalse(TEXT("Player discard secret absent"), PublicText.Contains(TEXT("SECRET_INTERACTION_DISCARD")));
	TestFalse(TEXT("Opponent deck secret absent"), PublicText.Contains(TEXT("SECRET_INTERACTION_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand secret absent"), PublicText.Contains(TEXT("SECRET_INTERACTION_OPPONENT_HAND")));
	TestFalse(TEXT("Opponent discard secret absent"), PublicText.Contains(TEXT("SECRET_INTERACTION_OPPONENT_DISCARD")));
	TestTrue(TEXT("Visible public card present"), PublicText.Contains(TEXT("char_visible_interaction_player")));
	TestEqual(TEXT("Secret state unused current player"), StateWithSecrets.CurrentPlayer, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeTurnInteractionModelStaleStatePolicyTest, "Wandbound.Runtime.TurnInteractionModel.StaleStatePolicyAfterExecution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeTurnInteractionModelStaleStatePolicyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeInteractionModel();
	FWBGameStateData State = MakeInteractionState();
	FWBRuntimeTurnResolutionContext Context;
	UWBRuntimeControllerFacadeComponent* Facade = NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Facade"), Facade);
	if (Model == nullptr || Facade == nullptr)
	{
		return false;
	}

	const FWBAction MoveAction = MakeInteractionMoveAction();
	Model->SetCurrentInteractionState({ MoveAction }, MakeInteractionPublicSummary());
	const FString SnapshotActionIdBefore = Model->GetCurrentPresentationSnapshot().Entries[0].ActionId;
	const FWBTile SnapshotToTileBefore = Model->GetCurrentPresentationSnapshot().Entries[0].ToTile;

	const FWBRuntimeActionSelectionExecutionResult Result = Model->ExecuteSelectedActionId(
		State,
		WBActionCodec::MakeActionId(MoveAction),
		Context,
		Facade);

	TestTrue(TEXT("Move succeeds"), Result.Execution.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Current state remains present"), Model->HasCurrentInteractionState());
	TestEqual(TEXT("Snapshot count unchanged"), Model->GetCurrentPresentationSnapshot().Entries.Num(), 1);
	TestEqual(TEXT("Snapshot action id unchanged"), Model->GetCurrentPresentationSnapshot().Entries[0].ActionId, SnapshotActionIdBefore);
	TestEqual(TEXT("Snapshot to x unchanged"), Model->GetCurrentPresentationSnapshot().Entries[0].ToTile.X, SnapshotToTileBefore.X);
	TestEqual(TEXT("Snapshot to y unchanged"), Model->GetCurrentPresentationSnapshot().Entries[0].ToTile.Y, SnapshotToTileBefore.Y);
	TestEqual(TEXT("Unit actually moved"), State.GetUnitById(1)->X, 5);
	return true;
}

#endif
