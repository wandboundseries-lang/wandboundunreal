#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBGameStateData.h"
#include "WBRuntimeInteractionRefreshAdapter.h"
#include "WBRuntimeTurnInteractionModelComponent.h"
#include "WBRuntimeTurnResolutionAdapter.h"
#include "WBRuntimeVisualControllerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeTurnInteractionModelComponent* MakeRefreshInteractionModel()
{
	return NewObject<UWBRuntimeTurnInteractionModelComponent>(GetTransientPackage());
}

UWBRuntimeVisualControllerComponent* MakeRefreshVisualController()
{
	return NewObject<UWBRuntimeVisualControllerComponent>(GetTransientPackage());
}

UWBBoardViewStateApplierComponent* MakeRefreshStateApplier()
{
	return NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
}

struct FRefreshVisualPath
{
	AWBBoardViewActor* BoardView = nullptr;
	UWBBoardViewStateApplierComponent* Applier = nullptr;
	UWBRuntimeVisualControllerComponent* VisualController = nullptr;
};

FRefreshVisualPath MakeRefreshVisualPath()
{
	FRefreshVisualPath Path;
	Path.BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	Path.Applier = MakeRefreshStateApplier();
	Path.VisualController = MakeRefreshVisualController();

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

FWBAction MakeRefreshMoveAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::Move;
	Action.PlayerId = 0;
	Action.SourceUnitId = 1;
	Action.FromTile = FWBTile(3, 4);
	Action.ToTile = FWBTile(4, 4);
	return Action;
}

FWBAction MakeRefreshAttackAction()
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

FWBAction MakeRefreshEndTurnAction()
{
	FWBAction Action;
	Action.Type = EWBActionType::EndTurn;
	Action.PlayerId = 0;
	return Action;
}

TArray<FWBAction> MakeRefreshActionList()
{
	return { MakeRefreshMoveAction(), MakeRefreshAttackAction(), MakeRefreshEndTurnAction() };
}

void ExpectDefaultVisualRefresh(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestFalse(TEXT("Not rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 0);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 0);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 0);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 0);
}

void ExpectDemoVisualRefresh(FAutomationTestBase& Test, const FWBBoardViewRefreshResult& Result)
{
	Test.TestTrue(TEXT("Rendered"), Result.bRendered);
	Test.TestEqual(TEXT("Tile count"), Result.TileCount, 81);
	Test.TestEqual(TEXT("Unit count"), Result.UnitCount, 2);
	Test.TestEqual(TEXT("Wall count"), Result.WallCount, 1);
	Test.TestEqual(TEXT("Terrain count"), Result.TerrainCount, 2);
}

FString CombineRefreshSnapshotPublicText(const FWBRuntimeLegalActionPresentationSnapshot& Snapshot)
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

bool LoadRefreshAdapterSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBRuntimeInteractionRefreshAdapter.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBRuntimeInteractionRefreshAdapter.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterClassExistsTest, "Wandbound.Runtime.InteractionRefreshAdapter.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterClassExistsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Adapter type exists"), sizeof(WBRuntimeInteractionRefreshAdapter) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterBothTargetsTest, "Wandbound.Runtime.InteractionRefreshAdapter.RefreshBothTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterBothTargetsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	const FRefreshVisualPath VisualPath = MakeRefreshVisualPath();
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Visual controller"), VisualPath.VisualController);
	if (Model == nullptr || VisualPath.VisualController == nullptr)
	{
		return false;
	}

	const TArray<FWBAction> Actions = MakeRefreshActionList();
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		Actions,
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		Model,
		VisualPath.VisualController);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Interaction model refreshed"), Result.bInteractionModelRefreshed);
	TestTrue(TEXT("Visual controller rendered"), Result.bVisualControllerRefreshed);
	TestEqual(TEXT("Legal action count"), Result.LegalActionCount, Actions.Num());
	TestEqual(TEXT("Presentation entry count"), Result.PresentationEntryCount, Actions.Num());
	TestEqual(TEXT("Model action count"), Model->GetCurrentLegalActions().Num(), Actions.Num());
	ExpectDemoVisualRefresh(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterModelOnlyTest, "Wandbound.Runtime.InteractionRefreshAdapter.RefreshInteractionModelOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterModelOnlyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	FWBRuntimeInteractionRefreshOptions Options;
	Options.bRefreshVisualController = false;
	const TArray<FWBAction> Actions = { MakeRefreshMoveAction(), MakeRefreshEndTurnAction() };
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		Actions,
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		Model,
		nullptr,
		Options);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestTrue(TEXT("Interaction model refreshed"), Result.bInteractionModelRefreshed);
	TestFalse(TEXT("Visual not refreshed"), Result.bVisualControllerRefreshed);
	TestEqual(TEXT("Presentation count"), Result.PresentationEntryCount, Actions.Num());
	TestEqual(TEXT("Model snapshot count"), Model->GetCurrentPresentationSnapshot().Entries.Num(), Actions.Num());
	ExpectDefaultVisualRefresh(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterVisualOnlyTest, "Wandbound.Runtime.InteractionRefreshAdapter.RefreshVisualControllerOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterVisualOnlyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* VisualController = MakeRefreshVisualController();
	TestNotNull(TEXT("Visual controller"), VisualController);
	if (VisualController == nullptr)
	{
		return false;
	}

	FWBRuntimeInteractionRefreshOptions Options;
	Options.bRefreshTurnInteractionModel = false;
	const TArray<FWBAction> Actions = { MakeRefreshMoveAction() };
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		Actions,
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		nullptr,
		VisualController,
		Options);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestFalse(TEXT("Interaction model not refreshed"), Result.bInteractionModelRefreshed);
	TestFalse(TEXT("Controller accepted but did not render without applier"), Result.bVisualControllerRefreshed);
	TestTrue(TEXT("Visual controller received summary"), VisualController->HasReceivedPublicBoardSummary());
	TestEqual(TEXT("Legal action count reported"), Result.LegalActionCount, Actions.Num());
	TestEqual(TEXT("Presentation count remains zero"), Result.PresentationEntryCount, 0);
	ExpectDefaultVisualRefresh(*this, Result.VisualRefreshResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterNoTargetsFailsTest, "Wandbound.Runtime.InteractionRefreshAdapter.NoTargetsFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterNoTargetsFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	UWBRuntimeVisualControllerComponent* VisualController = MakeRefreshVisualController();
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Visual controller"), VisualController);
	if (Model == nullptr || VisualController == nullptr)
	{
		return false;
	}

	FWBRuntimeInteractionRefreshOptions Options;
	Options.bRefreshTurnInteractionModel = false;
	Options.bRefreshVisualController = false;
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		{ MakeRefreshMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		Model,
		VisualController,
		Options);

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("no_refresh_targets_enabled")));
	TestFalse(TEXT("Model not mutated"), Model->HasCurrentInteractionState());
	TestFalse(TEXT("Visual not mutated"), VisualController->HasReceivedPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterMissingModelFailsTest, "Wandbound.Runtime.InteractionRefreshAdapter.MissingInteractionModelFailsValidationFirst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterMissingModelFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeVisualControllerComponent* VisualController = MakeRefreshVisualController();
	TestNotNull(TEXT("Visual controller"), VisualController);
	if (VisualController == nullptr)
	{
		return false;
	}

	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		{ MakeRefreshMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		nullptr,
		VisualController);

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("turn_interaction_model_missing")));
	TestFalse(TEXT("Visual not partially refreshed"), VisualController->HasReceivedPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterMissingVisualFailsTest, "Wandbound.Runtime.InteractionRefreshAdapter.MissingVisualControllerFailsValidationFirst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterMissingVisualFailsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		{ MakeRefreshMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		Model,
		nullptr);

	TestFalse(TEXT("Refresh fails"), Result.bOk);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("visual_controller_missing")));
	TestFalse(TEXT("Model not partially refreshed"), Model->HasCurrentInteractionState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterReplacesStaleActionsTest, "Wandbound.Runtime.InteractionRefreshAdapter.ReplacesStaleLegalActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterReplacesStaleActionsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	FWBRuntimeInteractionRefreshOptions Options;
	Options.bRefreshVisualController = false;
	WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		{ MakeRefreshMoveAction() },
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		Model,
		nullptr,
		Options);

	FWBGameStateData DummyState;
	FWBRuntimeTurnResolutionContext Context;
	Model->ExecuteSelectedActionId(DummyState, TEXT("missing:p0"), Context, nullptr);
	TestTrue(TEXT("Last execution attempt recorded"), Model->HasLastExecutionResult());

	const TArray<FWBAction> ReplacementActions = { MakeRefreshMoveAction(), MakeRefreshAttackAction() };
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		ReplacementActions,
		WBBoardViewDemoData::MakeSmallDemoBoardSummary(),
		Model,
		nullptr,
		Options);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestEqual(TEXT("Snapshot replaced"), Model->GetCurrentPresentationSnapshot().Entries.Num(), 2);
	TestEqual(TEXT("Legal actions replaced"), Model->GetCurrentLegalActions().Num(), 2);
	TestFalse(TEXT("Last execution cleared"), Model->HasLastExecutionResult());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterSameSummaryFeedsBothTargetsTest, "Wandbound.Runtime.InteractionRefreshAdapter.SamePublicSummaryFeedsModelAndVisuals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterSameSummaryFeedsBothTargetsTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	const FRefreshVisualPath VisualPath = MakeRefreshVisualPath();
	TestNotNull(TEXT("Model"), Model);
	TestNotNull(TEXT("Visual controller"), VisualPath.VisualController);
	if (Model == nullptr || VisualPath.VisualController == nullptr)
	{
		return false;
	}

	const FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		{ MakeRefreshMoveAction(), MakeRefreshAttackAction() },
		Summary,
		Model,
		VisualPath.VisualController);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	TestEqual(TEXT("Visual unit count matches summary"), Result.VisualRefreshResult.UnitCount, Summary.Units.Num());
	TestEqual(TEXT("Visual wall count matches summary"), Result.VisualRefreshResult.WallCount, Summary.Walls.Num());
	TestEqual(TEXT("Visual terrain count matches summary"), Result.VisualRefreshResult.TerrainCount, Summary.TerrainTiles.Num());

	const FWBRuntimeLegalActionPresentationSnapshot& Snapshot = Model->GetCurrentPresentationSnapshot();
	TestEqual(TEXT("Snapshot count"), Snapshot.Entries.Num(), 2);
	TestTrue(TEXT("Move source public unit found"), Snapshot.Entries[0].bHasSourcePublicUnit);
	TestTrue(TEXT("Attack source public unit found"), Snapshot.Entries[1].bHasSourcePublicUnit);
	TestTrue(TEXT("Attack target public unit found"), Snapshot.Entries[1].bHasTargetPublicUnit);
	TestEqual(TEXT("Source public card from same summary"), Snapshot.Entries[0].SourcePublicCardId, Summary.Units[0].CardId);
	TestEqual(TEXT("Target public card from same summary"), Snapshot.Entries[1].TargetPublicCardId, Summary.Units[1].CardId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterNoActionGenerationSourceGuardTest, "Wandbound.Runtime.InteractionRefreshAdapter.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Adapter source loads"), LoadRefreshAdapterSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not mention WBRules"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call CanMoveUnit"), Source.Contains(TEXT("CanMoveUnit")));
	TestFalse(TEXT("Does not call CanDeclareAttack"), Source.Contains(TEXT("CanDeclareAttack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterNoActionExecutionSourceGuardTest, "Wandbound.Runtime.InteractionRefreshAdapter.NoActionExecutionSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterNoActionExecutionSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Adapter source loads"), LoadRefreshAdapterSource(Source));
	TestFalse(TEXT("Does not execute selected actions"), Source.Contains(TEXT("ExecuteSelectedAction")));
	TestFalse(TEXT("Does not execute selected action ids"), Source.Contains(TEXT("ExecuteSelectedActionId")));
	TestFalse(TEXT("Does not apply selected actions"), Source.Contains(TEXT("ApplySelectedAction")));
	TestFalse(TEXT("Does not apply runtime selected action results"), Source.Contains(TEXT("ApplyRuntimeSelectedAction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterNoGameStateOwnershipSourceGuardTest, "Wandbound.Runtime.InteractionRefreshAdapter.NoGameStateOwnershipSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterNoGameStateOwnershipSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Adapter source loads"), LoadRefreshAdapterSource(Source));
	TestFalse(TEXT("API does not accept game state"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("No game state pointer storage"), Source.Contains(TEXT("GameStateData*")));
	TestFalse(TEXT("No game state reference storage"), Source.Contains(TEXT("GameStateData&")));
	TestFalse(TEXT("No deck access"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("No hand access"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("No discard access"), Source.Contains(TEXT("Discard")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBRuntimeInteractionRefreshAdapterHiddenDataExcludedTest, "Wandbound.Runtime.InteractionRefreshAdapter.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeInteractionRefreshAdapterHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBRuntimeTurnInteractionModelComponent* Model = MakeRefreshInteractionModel();
	TestNotNull(TEXT("Model"), Model);
	if (Model == nullptr)
	{
		return false;
	}

	FWBPublicBoardSummary PublicSummary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	PublicSummary.Units[0].CardId = TEXT("visible_refresh_player");
	PublicSummary.Units[1].CardId = TEXT("visible_refresh_target");

	FWBRuntimeInteractionRefreshOptions Options;
	Options.bRefreshVisualController = false;
	const FWBRuntimeInteractionRefreshResult Result = WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint(
		{ MakeRefreshMoveAction(), MakeRefreshAttackAction() },
		PublicSummary,
		Model,
		nullptr,
		Options);

	TestTrue(TEXT("Refresh ok"), Result.bOk);
	const FString PublicText = CombineRefreshSnapshotPublicText(Model->GetCurrentPresentationSnapshot());
	TestFalse(TEXT("Deck secret absent"), PublicText.Contains(TEXT("SECRET_REFRESH_DECK")));
	TestFalse(TEXT("Hand secret absent"), PublicText.Contains(TEXT("SECRET_REFRESH_HAND")));
	TestFalse(TEXT("Discard secret absent"), PublicText.Contains(TEXT("SECRET_REFRESH_DISCARD")));
	TestFalse(TEXT("Hidden marker secret absent"), PublicText.Contains(TEXT("SECRET_REFRESH_MARKER_ID")));
	TestTrue(TEXT("Visible player card present"), PublicText.Contains(TEXT("visible_refresh_player")));
	TestTrue(TEXT("Visible target card present"), PublicText.Contains(TEXT("visible_refresh_target")));
	return true;
}

#endif
