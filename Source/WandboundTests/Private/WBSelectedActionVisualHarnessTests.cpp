#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBMPRollSource.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeVisualControllerComponent.h"
#include "WBSelectedActionVisualHarness.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeHarnessPlayer(
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

FWBUnitState MakeHarnessUnit(
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

FWBGameStateData MakeHarnessState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0 = MakeHarnessPlayer(0, 2, 2);
	Player0.Deck.Add(TEXT("SECRET_HARNESS_DECK"));
	Player0.Hand.Add(TEXT("SECRET_HARNESS_HAND"));
	Player0.Discard.Add(TEXT("SECRET_HARNESS_DISCARD"));

	FWBPlayerStateData Player1 = MakeHarnessPlayer(1);
	Player1.Deck.Add(TEXT("SECRET_HARNESS_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_HARNESS_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_HARNESS_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);

	FWBUnitState PlayerUnit = MakeHarnessUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_harness_player"), 1, 1);
	PlayerUnit.AddStatus(FName(TEXT("Burn")), 2);
	State.AddUnitForTest(PlayerUnit);

	FWBUnitState OpponentUnit = MakeHarnessUnit(2, 1, FWBTile(6, 4), TEXT("char_visible_harness_opponent"), 0, 2);
	OpponentUnit.AddStatus(FName(TEXT("Poison")), 2);
	State.AddUnitForTest(OpponentUnit);

	State.AddWallForTest(FWBWallEdge(FWBTile(0, 0), FWBTile(0, 1)));
	State.SetTerrainForTest(FWBTile(2, 2), FName(TEXT("Mud")));
	State.SetTerrainForTest(FWBTile(6, 6), FName(TEXT("Ice")));
	return State;
}

FWBAction MakeHarnessMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(5, 4);
	return Action;
}

FWBAction MakeHarnessEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
}

bool TraceContainsKind(const TArray<FWBTraceEvent>& TraceEvents, const FName Kind)
{
	for (const FWBTraceEvent& TraceEvent : TraceEvents)
	{
		if (TraceEvent.Kind == Kind)
		{
			return true;
		}
	}

	return false;
}

void ExpectDefaultRefreshResult(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestFalse(TEXT("Not rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 0);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 0);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 0);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 0);
}

void ExpectHarnessRefreshCounts(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestTrue(TEXT("Rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 81);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 2);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 1);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 2);
}

struct FHarnessVisualPath
{
	AWBBoardViewActor* BoardView = nullptr;
	UWBBoardViewStateApplierComponent* Applier = nullptr;
	UWBRuntimeVisualControllerComponent* Controller = nullptr;
};

FHarnessVisualPath MakeHarnessVisualPath()
{
	FHarnessVisualPath Path;
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

bool LoadHarnessSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBSelectedActionVisualHarness.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBSelectedActionVisualHarness.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessClassExistsTest, "Wandbound.Runtime.SelectedActionVisualHarness.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessClassExistsTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Harness source exists"), LoadHarnessSource(Source));
	TestTrue(TEXT("Harness class declared"), Source.Contains(TEXT("class WANDBOUNDRUNTIME_API WBSelectedActionVisualHarness")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessMoveRefreshesTest, "Wandbound.Runtime.SelectedActionVisualHarness.MoveRefreshesVisuals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessMoveRefreshesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	FWBQueuedMPRollSource RollSource;
	RollSource.EnqueueRoll(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FHarnessVisualPath VisualPath = MakeHarnessVisualPath();
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (VisualPath.Controller == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessMoveAction(),
			Context,
			VisualPath.Controller);

	TestTrue(TEXT("Runtime move succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Move does not consume queued MP roll"), RollSource.NumRemainingRolls(), 1);
	TestEqual(TEXT("Unit moved x"), State.GetUnitById(1)->X, 5);
	TestEqual(TEXT("Unit moved y"), State.GetUnitById(1)->Y, 4);
	TestEqual(TEXT("Player MP decremented"), State.GetPlayerById(0)->RemainingMP, 1);
	ExpectHarnessRefreshCounts(*this, Result.VisualRefreshResult);
	TestEqual(TEXT("Final public summary unit x"), Result.RuntimeResult.FinalPublicBoardSummary.Units[0].X, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessFullEndTurnRefreshesTest, "Wandbound.Runtime.SelectedActionVisualHarness.FullEndTurnRefreshesVisuals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessFullEndTurnRefreshesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	FWBFixedMPRollSource RollSource(4);
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = &RollSource;
	const FHarnessVisualPath VisualPath = MakeHarnessVisualPath();
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (VisualPath.Controller == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessEndTurnAction(),
			Context,
			VisualPath.Controller);

	TestTrue(TEXT("Runtime full EndTurn succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	TestTrue(TEXT("Runtime result consumed roll"), Result.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Runtime result consumed roll value"), Result.RuntimeResult.ConsumedMPRoll, 4);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Next player MP setup"), State.GetPlayerById(1)->RemainingMP, 4);
	TestTrue(TEXT("Transition trace appears"), TraceContainsKind(Result.RuntimeResult.ApplyResult.TraceEvents, FName(TEXT("turn_transition"))));
	ExpectHarnessRefreshCounts(*this, Result.VisualRefreshResult);
	TestTrue(TEXT("Controller received public summary"), VisualPath.Controller->HasReceivedPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessBasicEndTurnRefreshesTest, "Wandbound.Runtime.SelectedActionVisualHarness.BasicEndTurnRefreshesVisuals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessBasicEndTurnRefreshesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = false;
	Context.MPRollSource = nullptr;
	const FHarnessVisualPath VisualPath = MakeHarnessVisualPath();
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (VisualPath.Controller == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessEndTurnAction(),
			Context,
			VisualPath.Controller);

	TestTrue(TEXT("Runtime basic EndTurn succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	TestFalse(TEXT("Basic EndTurn does not consume roll"), Result.RuntimeResult.bConsumedMPRoll);
	TestEqual(TEXT("Current player advances"), State.CurrentPlayer, 1);
	TestEqual(TEXT("Basic trace count"), Result.RuntimeResult.ApplyResult.TraceEvents.Num(), 1);
	if (Result.RuntimeResult.ApplyResult.TraceEvents.Num() == 1)
	{
		TestEqual(TEXT("Basic trace kind"), Result.RuntimeResult.ApplyResult.TraceEvents[0].Kind, FName(TEXT("end_turn")));
	}
	TestFalse(TEXT("No transition trace"), TraceContainsKind(Result.RuntimeResult.ApplyResult.TraceEvents, FName(TEXT("turn_transition"))));
	ExpectHarnessRefreshCounts(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessNullControllerSafeTest, "Wandbound.Runtime.SelectedActionVisualHarness.NullControllerSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessNullControllerSafeTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessMoveAction(),
			Context,
			nullptr);

	TestTrue(TEXT("Runtime action still executes"), Result.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Unit moved despite null visual controller"), State.GetUnitById(1)->X, 5);
	ExpectDefaultRefreshResult(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessFailureSkipsRefreshByDefaultTest, "Wandbound.Runtime.SelectedActionVisualHarness.FailureSkipsRefreshByDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessFailureSkipsRefreshByDefaultTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FHarnessVisualPath VisualPath = MakeHarnessVisualPath();
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (VisualPath.Controller == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessEndTurnAction(),
			Context,
			VisualPath.Controller);

	TestFalse(TEXT("Runtime action fails"), Result.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Missing roll source reason"), Result.RuntimeResult.ApplyResult.Reason, FString(TEXT("missing_mp_roll_source")));
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestFalse(TEXT("Controller did not receive skipped failure summary"), VisualPath.Controller->HasReceivedPublicBoardSummary());
	ExpectDefaultRefreshResult(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessFailureCanRefreshTest, "Wandbound.Runtime.SelectedActionVisualHarness.FailureCanRefreshWhenRequested", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessFailureCanRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	const FWBGameStateData Before = State;
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FHarnessVisualPath VisualPath = MakeHarnessVisualPath();
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (VisualPath.Controller == nullptr)
	{
		return false;
	}

	FWBSelectedActionVisualHarnessOptions Options;
	Options.bRefreshVisualsOnFailure = true;
	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessEndTurnAction(),
			Context,
			VisualPath.Controller,
			Options);

	TestFalse(TEXT("Runtime action fails"), Result.RuntimeResult.ApplyResult.bOk);
	TestEqual(TEXT("Missing roll source reason"), Result.RuntimeResult.ApplyResult.Reason, FString(TEXT("missing_mp_roll_source")));
	TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	TestEqual(TEXT("Turn unchanged"), State.TurnNumber, Before.TurnNumber);
	TestTrue(TEXT("Controller received failure summary when requested"), VisualPath.Controller->HasReceivedPublicBoardSummary());
	ExpectHarnessRefreshCounts(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessNoActionGenerationSourceGuardTest, "Wandbound.Runtime.SelectedActionVisualHarness.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Harness source loads"), LoadHarnessSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessNoGameStateOwnershipSourceGuardTest, "Wandbound.Runtime.SelectedActionVisualHarness.NoGameStateOwnershipSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessNoGameStateOwnershipSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Harness source loads"), LoadHarnessSource(Source));
	TestTrue(TEXT("State API is by reference"), Source.Contains(TEXT("FWBGameStateData& State")));
	TestFalse(TEXT("No state pointer storage"), Source.Contains(TEXT("FWBGameStateData*")));
	TestFalse(TEXT("No state member storage"), Source.Contains(TEXT("FWBGameStateData State;")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBSelectedActionVisualHarnessHiddenDataExcludedTest, "Wandbound.Runtime.SelectedActionVisualHarness.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBSelectedActionVisualHarnessHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeHarnessState();
	FWBRuntimeTurnResolutionContext Context;
	Context.bResolveEndTurnAsFullTransition = true;
	Context.MPRollSource = nullptr;
	const FHarnessVisualPath VisualPath = MakeHarnessVisualPath();
	TestNotNull(TEXT("Visual controller"), VisualPath.Controller);
	if (VisualPath.Controller == nullptr)
	{
		return false;
	}

	const FWBSelectedActionVisualHarnessResult Result =
		WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals(
			State,
			MakeHarnessMoveAction(),
			Context,
			VisualPath.Controller);

	TestTrue(TEXT("Runtime move succeeds"), Result.RuntimeResult.ApplyResult.bOk);
	const FString Serialized = SerializeRuntimeResult(Result.RuntimeResult);
	TestFalse(TEXT("Player deck secret absent"), Serialized.Contains(TEXT("SECRET_HARNESS_DECK")));
	TestFalse(TEXT("Player hand secret absent"), Serialized.Contains(TEXT("SECRET_HARNESS_HAND")));
	TestFalse(TEXT("Player discard secret absent"), Serialized.Contains(TEXT("SECRET_HARNESS_DISCARD")));
	TestFalse(TEXT("Opponent deck secret absent"), Serialized.Contains(TEXT("SECRET_HARNESS_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand secret absent"), Serialized.Contains(TEXT("SECRET_HARNESS_OPPONENT_HAND")));
	TestFalse(TEXT("Opponent discard secret absent"), Serialized.Contains(TEXT("SECRET_HARNESS_OPPONENT_DISCARD")));
	TestTrue(TEXT("Visible unit remains public"), Serialized.Contains(TEXT("char_visible_harness_player")));
	return true;
}

#endif
