#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeResultSerializer.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBRuntimeVisualControllerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeVisualControllerComponent* MakeVisualController()
{
	return NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());
}

UWBBoardViewStateApplierComponent* MakeStateApplier()
{
	return NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
}

void ExpectDefaultRefreshResult(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestFalse(TEXT("Not rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 0);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 0);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 0);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 0);
}

void ExpectDemoRefreshCounts(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestTrue(TEXT("Rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 81);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 2);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 1);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 2);
}

void ExpectSummaryUnchanged(
	FAutomationTestBase& Test,
	const FWBPublicBoardSummary& Summary,
	const FWBPublicBoardSummary& Before)
{
	Test.TestEqual(TEXT("Board width unchanged"), Summary.BoardWidth, Before.BoardWidth);
	Test.TestEqual(TEXT("Board height unchanged"), Summary.BoardHeight, Before.BoardHeight);
	Test.TestEqual(TEXT("Default terrain unchanged"), Summary.DefaultTerrainId.GetPlainNameString(), Before.DefaultTerrainId.GetPlainNameString());
	Test.TestEqual(TEXT("Unit count unchanged"), Summary.Units.Num(), Before.Units.Num());
	Test.TestEqual(TEXT("Wall count unchanged"), Summary.Walls.Num(), Before.Walls.Num());
	Test.TestEqual(TEXT("Terrain count unchanged"), Summary.TerrainTiles.Num(), Before.TerrainTiles.Num());

	if (Summary.Units.Num() > 0 && Before.Units.Num() > 0)
	{
		Test.TestEqual(TEXT("First unit id unchanged"), Summary.Units[0].UnitId, Before.Units[0].UnitId);
		Test.TestEqual(TEXT("First unit card unchanged"), Summary.Units[0].CardId, Before.Units[0].CardId);
		Test.TestEqual(TEXT("First unit x unchanged"), Summary.Units[0].X, Before.Units[0].X);
		Test.TestEqual(TEXT("First unit y unchanged"), Summary.Units[0].Y, Before.Units[0].Y);
	}
}

FWBUnitState MakeHiddenBoundaryUnit(const int32 UnitId, const int32 OwnerId, const FWBTile& Tile, const FString& CardId)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 4;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBGameStateData MakeHiddenBoundaryState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 6;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.Deck.Add(TEXT("SECRET_VISUAL_CONTROLLER_DECK"));
	Player0.Hand.Add(TEXT("SECRET_VISUAL_CONTROLLER_HAND"));
	Player0.Discard.Add(TEXT("SECRET_VISUAL_CONTROLLER_DISCARD"));

	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.Deck.Add(TEXT("SECRET_VISUAL_CONTROLLER_OPPONENT_DECK"));
	Player1.Hand.Add(TEXT("SECRET_VISUAL_CONTROLLER_OPPONENT_HAND"));
	Player1.Discard.Add(TEXT("SECRET_VISUAL_CONTROLLER_OPPONENT_DISCARD"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);
	State.AddUnitForTest(MakeHiddenBoundaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_visual_controller")));
	return State;
}

FString SerializeSummary(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = Summary;

	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, Json);
	return Json;
}

bool LoadVisualControllerSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeVisualControllerComponent.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeVisualControllerComponent.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerClassExistsTest, "Wandbound.Runtime.VisualController.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Visual controller component class"), UWBRuntimeVisualControllerComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerInitialStateTest, "Wandbound.Runtime.VisualController.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerInitialStateTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	TestNotNull(TEXT("Controller"), Controller);
	if (Controller == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No public summary received initially"), Controller->HasReceivedPublicBoardSummary());
	ExpectDefaultRefreshResult(*this, Controller->GetLastBoardRefreshResult());
	TestNull(TEXT("No applier initially"), Controller->GetBoardViewStateApplier());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerNullApplierSafeTest, "Wandbound.Runtime.VisualController.ApplyPublicSummaryNullApplierSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerNullApplierSafeTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	TestNotNull(TEXT("Controller"), Controller);
	if (Controller == nullptr)
	{
		return false;
	}

	const FWBBoardViewRefreshResult Result = Controller->ApplyPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestTrue(TEXT("Public summary receipt is tracked"), Controller->HasReceivedPublicBoardSummary());
	ExpectDefaultRefreshResult(*this, Result);
	ExpectDefaultRefreshResult(*this, Controller->GetLastBoardRefreshResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerNullBoardActorSafeTest, "Wandbound.Runtime.VisualController.ApplyPublicSummaryNullBoardActorSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerNullBoardActorSafeTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	UWBBoardViewStateApplierComponent* Applier = MakeStateApplier();
	TestNotNull(TEXT("Controller"), Controller);
	TestNotNull(TEXT("Applier"), Applier);
	if (Controller == nullptr || Applier == nullptr)
	{
		return false;
	}

	Controller->SetBoardViewStateApplier(Applier);
	const FWBBoardViewRefreshResult Result = Controller->ApplyPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	TestTrue(TEXT("Public summary receipt is tracked"), Controller->HasReceivedPublicBoardSummary());
	TestTrue(TEXT("Applier received summary"), Applier->HasLatestPublicBoardSummary());
	ExpectDefaultRefreshResult(*this, Result);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerApplyWithActorUpdatesCountsTest, "Wandbound.Runtime.VisualController.ApplyPublicSummaryWithActorUpdatesCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerApplyWithActorUpdatesCountsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	UWBBoardViewStateApplierComponent* Applier = MakeStateApplier();
	AWBBoardViewActor* BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Controller"), Controller);
	TestNotNull(TEXT("Applier"), Applier);
	TestNotNull(TEXT("Board view"), BoardView);
	if (Controller == nullptr || Applier == nullptr || BoardView == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	Applier->SetBoardViewActor(BoardView);
	Controller->SetBoardViewStateApplier(Applier);

	const FWBBoardViewRefreshResult Result = Controller->ApplyPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	ExpectDemoRefreshCounts(*this, Result);
	ExpectDemoRefreshCounts(*this, Controller->GetLastBoardRefreshResult());
	TestEqual(TEXT("Actor tile count"), BoardView->GetRenderedTileCount(), 81);
	TestEqual(TEXT("Actor unit count"), BoardView->GetRenderedUnitCount(), 2);
	TestEqual(TEXT("Actor wall count"), BoardView->GetRenderedWallCount(), 1);
	TestEqual(TEXT("Actor terrain count"), BoardView->GetRenderedTerrainCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerRuntimeResultUsesFinalSummaryTest, "Wandbound.Runtime.VisualController.RuntimeResultUsesFinalPublicBoardSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerRuntimeResultUsesFinalSummaryTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	UWBBoardViewStateApplierComponent* Applier = MakeStateApplier();
	AWBBoardViewActor* BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Controller"), Controller);
	TestNotNull(TEXT("Applier"), Applier);
	TestNotNull(TEXT("Board view"), BoardView);
	if (Controller == nullptr || Applier == nullptr || BoardView == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	Applier->SetBoardViewActor(BoardView);
	Controller->SetBoardViewStateApplier(Applier);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.SelectedActionId = TEXT("SECRET_ACTION_ID_SHOULD_NOT_BE_NEEDED_FOR_RENDER");
	Envelope.ApplyResult.Reason = TEXT("SECRET_APPLY_REASON_SHOULD_NOT_BE_NEEDED_FOR_RENDER");
	Envelope.FinalPublicBoardSummary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();

	FWBTraceEvent Trace;
	Trace.Kind = FName(TEXT("secret_trace_kind_not_needed_for_render"));
	Trace.Reason = TEXT("SECRET_TRACE_REASON_SHOULD_NOT_BE_NEEDED_FOR_RENDER");
	Envelope.ApplyResult.TraceEvents.Add(Trace);

	const FWBBoardViewRefreshResult Result = Controller->ApplyRuntimeSelectedActionResult(Envelope);
	ExpectDemoRefreshCounts(*this, Result);
	TestEqual(TEXT("Applier receives final summary units"), Applier->GetLatestPublicBoardSummary().Units.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerDoesNotMutateSummaryInputTest, "Wandbound.Runtime.VisualController.DoesNotMutatePublicSummaryInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerDoesNotMutateSummaryInputTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	UWBBoardViewStateApplierComponent* Applier = MakeStateApplier();
	TestNotNull(TEXT("Controller"), Controller);
	TestNotNull(TEXT("Applier"), Applier);
	if (Controller == nullptr || Applier == nullptr)
	{
		return false;
	}

	Controller->SetBoardViewStateApplier(Applier);
	FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	const FWBPublicBoardSummary Before = Summary;

	Controller->ApplyPublicBoardSummary(Summary);
	ExpectSummaryUnchanged(*this, Summary, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerNoGameStateApiSourceGuardTest, "Wandbound.Runtime.VisualController.NoGameStateApiSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerNoGameStateApiSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Visual controller source loads"), LoadVisualControllerSource(Source));
	TestFalse(TEXT("Does not expose FWBGameStateData"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Does not call BuildPublicSummaryFromState"), Source.Contains(TEXT("BuildPublicSummaryFromState")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerNoActionGenerationSourceGuardTest, "Wandbound.Runtime.VisualController.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Visual controller source loads"), LoadVisualControllerSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call ApplyMove"), Source.Contains(TEXT("ApplyMove")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not mention WBEffectRunner"), Source.Contains(TEXT("WBEffectRunner")));
	TestFalse(TEXT("Does not mention Deck"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("Does not mention Hand"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("Does not mention Discard"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeVisualControllerHiddenDataExcludedTest, "Wandbound.Runtime.VisualController.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeVisualControllerHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* Controller = MakeVisualController();
	UWBBoardViewStateApplierComponent* Applier = MakeStateApplier();
	TestNotNull(TEXT("Controller"), Controller);
	TestNotNull(TEXT("Applier"), Applier);
	if (Controller == nullptr || Applier == nullptr)
	{
		return false;
	}

	Controller->SetBoardViewStateApplier(Applier);

	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = WBPublicBoardSummary::Build(MakeHiddenBoundaryState());
	Controller->ApplyRuntimeSelectedActionResult(Envelope);

	const FString Serialized = SerializeSummary(Applier->GetLatestPublicBoardSummary());
	TestFalse(TEXT("Player deck secret absent"), Serialized.Contains(TEXT("SECRET_VISUAL_CONTROLLER_DECK")));
	TestFalse(TEXT("Player hand secret absent"), Serialized.Contains(TEXT("SECRET_VISUAL_CONTROLLER_HAND")));
	TestFalse(TEXT("Player discard secret absent"), Serialized.Contains(TEXT("SECRET_VISUAL_CONTROLLER_DISCARD")));
	TestFalse(TEXT("Opponent deck secret absent"), Serialized.Contains(TEXT("SECRET_VISUAL_CONTROLLER_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand secret absent"), Serialized.Contains(TEXT("SECRET_VISUAL_CONTROLLER_OPPONENT_HAND")));
	TestFalse(TEXT("Opponent discard secret absent"), Serialized.Contains(TEXT("SECRET_VISUAL_CONTROLLER_OPPONENT_DISCARD")));
	TestTrue(TEXT("Visible card remains public"), Serialized.Contains(TEXT("char_visible_visual_controller")));
	return true;
}

#endif
