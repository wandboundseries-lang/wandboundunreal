#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBRuntimeControllerFacadeComponent.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeVisualControllerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeControllerFacadeComponent* MakeFacade()
{
	return NewObject<UWBRuntimeControllerFacadeComponent>(GetTransientPackage());
}

FWBPlayerStateData MakeFacadePlayer(
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

FWBUnitState MakeFacadeUnit(
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

FWBGameStateData MakeFacadeState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeFacadePlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_FACADE_DECK"));
	Player0.Hand.Add(TEXT("SECRET_FACADE_HAND"));
	Player0.Discard.Add(TEXT("SECRET_FACADE_DISCARD"));

	FWBPlayerStateData Player1 = MakeFacadePlayer(1);
	Player1.Deck.Add(TEXT("SECRET_FACADE_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_FACADE_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_FACADE_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);

	FWBUnitState PlayerUnit = MakeFacadeUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_facade_player"), 1, 1);
	PlayerUnit.AddStatus(FName(TEXT("Burn")), 2);
	State.AddUnitForTest(PlayerUnit);

	FWBUnitState OpponentUnit = MakeFacadeUnit(2, 1, FWBTile(6, 4), TEXT("char_visible_facade_opponent"), 0, 2);
	OpponentUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(OpponentUnit);

	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

FWBAction MakeFacadeMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeFacadeEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
}

void ExpectDefaultRefreshResult(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestFalse(TEXT("Not rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 0);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 0);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 0);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 0);
}

void ExpectFacadeRefreshCounts(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestTrue(TEXT("Rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 81);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 2);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 1);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 2);
}

struct FFacadeVisualPath
{
	AWBBoardViewActor* BoardView = nullptr;
	UWBBoardViewStateApplierComponent* Applier = nullptr;
	UWBRuntimeVisualControllerComponent* Controller = nullptr;
};

FFacadeVisualPath MakeFacadeVisualPath()
{
	FFacadeVisualPath Path;
	Path.BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	Path.Applier = NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
	Path.Controller = NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());

	if (Path.Applier != nullptr)
	{
		Path.Applier->bApplyOnSummarySet = false;
		Path.Applier->SetBoardViewActor(Path.BoardView);
	}

	if (Path.Controller != nullptr)
	{
		Path.Controller->SetBoardViewStateApplier(Path.Applier);
	}

	return Path;
}

FString SerializeRuntimeResult(const FWBRuntimeSelectedActionResult& Result)
{
	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Result, Json);
	return Json;
}

bool LoadFacadeSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeControllerFacadeComponent.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeControllerFacadeComponent.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeClassExistsTest, "Wandbound.Runtime.ControllerFacade.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Runtime controller facade component class"), UWBRuntimeControllerFacadeComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeInitialStateTest, "Wandbound.Runtime.ControllerFacade.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeInitialStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	TestNotNull(TEXT("Facade"), Facade);
	if (Facade == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No action executed initially"), Facade->HasExecutedAction());
	TestNull(TEXT("No visual controller initially"), Facade->GetVisualController());
	ExpectDefaultRefreshResult(*this, Facade->GetLastVisualRefreshResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeMoveNullVisualControllerTest, "Wandbound.Runtime.ControllerFacade.MoveNullVisualControllerSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeMoveNullVisualControllerTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	TestNotNull(TEXT("Facade"), Facade);
	if (Facade == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result = Facade->ExecuteSelectedAction(
		State,
		MakeFacadeMoveAction(),
		Context);

	TestTrue(TEXT("Runtime move succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Facade records execution"), Facade->HasExecutedAction());
	TestEqual(TEXT("Stored selected action id"), Facade->GetLastRuntimeResult().SelectedActionId, FString(TEXT("move:p0:u1:4,4->5,4")));
	TestEqual(TEXT("Queued roll not consumed by move"), RollSource.NumRemainingRolls(), 1);
	TestEqual(TEXT("Unit moved x"), State.GetUnitById(1)->X, 5);
	TestEqual(TEXT("Unit moved y"), State.GetUnitById(1)->Y, 4);
	TestEqual(TEXT("Player MP decremented"), State.GetPlayerById(0)->RemainingMP, 1);
	ExpectDefaultRefreshResult(*this, Result.VisualRefreshResult);
	ExpectDefaultRefreshResult(*this, Facade->GetLastVisualRefreshResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeMoveVisualControllerTest, "Wandbound.Runtime.ControllerFacade.MoveRefreshesVisualController", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeMoveVisualControllerTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FFacadeVisualPath VisualPath = MakeFacadeVisualPath();
	TestNotNull(TEXT("Facade"), Facade);
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (Facade == nullptr || VisualPath.Controller == nullptr)
	{
		return false;
	}

	Facade->SetVisualController(VisualPath.Controller);
	const FWBSelectedActionVisualHarnessResult Result = Facade->ExecuteSelectedAction(
		State,
		MakeFacadeMoveAction(),
		Context);

	TestTrue(TEXT("Runtime move succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Visual controller received summary"), VisualPath.Controller->HasReceivedPublicBoardSummary());
	ExpectFacadeRefreshCounts(*this, Result.VisualRefreshResult);
	ExpectFacadeRefreshCounts(*this, Facade->GetLastVisualRefreshResult());
	TestEqual(TEXT("Final public summary unit x"), Result.RuntimeResult.FinalPublicBoardSummary.Units[0].X, 5);
	TestEqual(TEXT("Rendered unit count matches final summary"), Result.VisualRefreshResult.UnitCount, Result.RuntimeResult.FinalPublicBoardSummary.Units.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeFullEndTurnTest, "Wandbound.Runtime.ControllerFacade.FullEndTurnUsesDeterministicRoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeFullEndTurnTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FFacadeVisualPath VisualPath = MakeFacadeVisualPath();
	TestNotNull(TEXT("Facade"), Facade);
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (Facade == nullptr || VisualPath.Controller == nullptr)
	{
		return false;
	}

	Facade->SetVisualController(VisualPath.Controller);
	const FWBSelectedActionVisualHarnessResult Result = Facade->ExecuteSelectedAction(
		State,
		MakeFacadeEndTurnAction(),
		Context);

	TestTrue(TEXT("Runtime full EndTurn succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Runtime result consumed roll"), Result.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Runtime result consumed roll value"), Result.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Stored consumed roll value"), Facade->GetLastRuntimeResult().ConsumedMPRoll, 4);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	ExpectFacadeRefreshCounts(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeFailureSkipsRefreshByDefaultTest, "Wandbound.Runtime.ControllerFacade.FailureSkipsRefreshByDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeFailureSkipsRefreshByDefaultTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FFacadeVisualPath VisualPath = MakeFacadeVisualPath();
	TestNotNull(TEXT("Facade"), Facade);
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (Facade == nullptr || VisualPath.Controller == nullptr)
	{
		return false;
	}

	Facade->SetVisualController(VisualPath.Controller);
	const FWBSelectedActionVisualHarnessResult Result = Facade->ExecuteSelectedAction(
		State,
		MakeFacadeEndTurnAction(),
		Context);

	TestFalse(TEXT("Runtime action fails"), Result.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Missing roll source reason"), Result.RuntimeResult.ApplyResult.Reason, FString(TEXT("missing_mp_roll_source")));
	TestTrue(TEXT("Facade records failed execution"), Facade->HasExecutedAction());
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestFalse(TEXT("Controller did not receive skipped failure summary"), VisualPath.Controller->HasReceivedPublicBoardSummary());
	ExpectDefaultRefreshResult(*this, Result.VisualRefreshResult);
	ExpectDefaultRefreshResult(*this, Facade->GetLastVisualRefreshResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeFailureCanRefreshTest, "Wandbound.Runtime.ControllerFacade.FailureCanRefreshWhenRequested", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeFailureCanRefreshTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FFacadeVisualPath VisualPath = MakeFacadeVisualPath();
	TestNotNull(TEXT("Facade"), Facade);
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (Facade == nullptr || VisualPath.Controller == nullptr)
	{
		return false;
	}

	Facade->SetVisualController(VisualPath.Controller);
	Facade->bRefreshVisualsOnActionFailure = true;
	const FWBSelectedActionVisualHarnessResult Result = Facade->ExecuteSelectedAction(
		State,
		MakeFacadeEndTurnAction(),
		Context);

	TestFalse(TEXT("Runtime action fails"), Result.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Missing roll source reason"), Result.RuntimeResult.ApplyResult.Reason, FString(TEXT("missing_mp_roll_source")));
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestTrue(TEXT("Controller received failure summary when requested"), VisualPath.Controller->HasReceivedPublicBoardSummary());
	ExpectFacadeRefreshCounts(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeClearLastResultsTest, "Wandbound.Runtime.ControllerFacade.ClearLastResults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeClearLastResultsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	TestNotNull(TEXT("Facade"), Facade);
	if (Facade == nullptr)
	{
		return false;
	}

	Facade->ExecuteSelectedAction(State, MakeFacadeMoveAction(), Context);
	TestTrue(TEXT("Facade records execution before clear"), Facade->HasExecutedAction());
	TestTrue(TEXT("Stored runtime result before clear"), Facade->GetLastRuntimeResult().ApplyResult.bOk);

	Facade->ClearLastResults();
	TestFalse(TEXT("Execution flag cleared"), Facade->HasExecutedAction());
	TestFalse(TEXT("Runtime result cleared"), Facade->GetLastRuntimeResult().ApplyResult.bOk);
	TestTrue(TEXT("Selected action id cleared"), Facade->GetLastRuntimeResult().SelectedActionId.IsEmpty());
	ExpectDefaultRefreshResult(*this, Facade->GetLastVisualRefreshResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeNoGameStateOwnershipSourceGuardTest, "Wandbound.Runtime.ControllerFacade.NoGameStateOwnershipSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeNoGameStateOwnershipSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Facade source loads"), LoadFacadeSource(Source));
	TestTrue(TEXT("State API is by reference"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No state pointer storage"), Source.Contains(TEXT("FWBGameStateData*")));
	TestFalse(TEXT("No state member storage"), Source.Contains(TEXT("FWBGameStateData State;")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeNoActionGenerationSourceGuardTest, "Wandbound.Runtime.ControllerFacade.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Facade source loads"), LoadFacadeSource(Source));
	TestTrue(TEXT("Delegates to selected-action visual harness"), Source.Contains(TEXT("ApplySelectedActionAndRefreshVisuals")));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call CanMove"), Source.Contains(TEXT("CanMove")));
	TestFalse(TEXT("Does not call CanDeclareAttack"), Source.Contains(TEXT("CanDeclareAttack")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeControllerFacadeHiddenDataExcludedTest, "Wandbound.Runtime.ControllerFacade.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeControllerFacadeHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBRuntimeControllerFacadeComponent* Facade = MakeFacade();
	FWBGameStateData State = MakeFacadeState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	TestNotNull(TEXT("Facade"), Facade);
	if (Facade == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result = Facade->ExecuteSelectedAction(
		State,
		MakeFacadeMoveAction(),
		Context);

	TestTrue(TEXT("Runtime move succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	const FString Serialized = SerializeRuntimeResult(Facade->GetLastRuntimeResult());
	TestFalse(TEXT("Player deck secret absent"), Serialized.Contains(TEXT("SECRET_FACADE_DECK")));
	TestFalse(TEXT("Player hand secret absent"), Serialized.Contains(TEXT("SECRET_FACADE_HAND")));
	TestFalse(TEXT("Player discard secret absent"), Serialized.Contains(TEXT("SECRET_FACADE_DISCARD")));
	TestFalse(TEXT("Opponent deck secret absent"), Serialized.Contains(TEXT("SECRET_FACADE_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand secret absent"), Serialized.Contains(TEXT("SECRET_FACADE_OPPONENT_HAND")));
	TestFalse(TEXT("Opponent discard secret absent"), Serialized.Contains(TEXT("SECRET_FACADE_OPPONENT_DISCARD")));
	TestTrue(TEXT("Visible unit remains public"), Serialized.Contains(TEXT("char_visible_facade_player")));
	return true;
}

#endif
