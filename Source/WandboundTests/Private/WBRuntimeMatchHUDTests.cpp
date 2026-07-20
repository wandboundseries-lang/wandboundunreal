#include "Misc/AutomationTest.h"

#include "Algo/IsSorted.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeMatchHUDWidget.h"
#include "WBRuntimePlayerController.h"
#include "WBRuntimeUnitPresentationActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBRuntimeMatchHostComponent* MakeHost(const bool bFragileFirstHero = false)
{
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	return Host->InitializeDevelopmentMatch(0, bFragileFirstHero).bOk ? Host : nullptr;
}

UWBRuntimeMatchHUDWidget* MakeHUD(UWBRuntimeMatchHostComponent* Host)
{
	UWBRuntimeMatchHUDWidget* HUD = NewObject<UWBRuntimeMatchHUDWidget>(GetTransientPackage());
	HUD->EnsureWidgetTreeBuilt();
	return HUD->BindToMatchHost(Host) ? HUD : nullptr;
}

FWBRuntimeLegalActionPresentation FindFamily(
	const TArray<FWBRuntimeLegalActionPresentation>& Actions,
	const EWBRuntimeMatchActionFamily Family)
{
	const FWBRuntimeLegalActionPresentation* Found = Actions.FindByPredicate([Family](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.Family == Family;
	});
	return Found != nullptr ? *Found : FWBRuntimeLegalActionPresentation();
}

FWBRuntimeUnitPresentation FindOwnedHero(
	const UWBRuntimeMatchHostComponent* Host,
	const int32 OwnerId)
{
	const TArray<FWBRuntimeUnitPresentation> Units = Host->GetCurrentUnits();
	const FWBRuntimeUnitPresentation* Found = Units.FindByPredicate([OwnerId](const FWBRuntimeUnitPresentation& Unit)
	{
		return Unit.OwnerId == OwnerId && Unit.bHero;
	});
	return Found != nullptr ? *Found : FWBRuntimeUnitPresentation();
}

UWorld* CreateHUDTestWorld()
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("WBRuntimeMatchHUDTestWorld")));
	if (World != nullptr && GEngine != nullptr)
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
	}
	return World;
}

void DestroyHUDTestWorld(UWorld* World)
{
	if (World == nullptr) return;
	if (GEngine != nullptr) GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
}

bool SubmitMoveTo(
	UWBRuntimeMatchHUDWidget* HUD,
	const FIntPoint Target)
{
	const TArray<FWBRuntimeLegalActionPresentation> Actions = HUD->GetBoundMatchHost()->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* Move = Actions.FindByPredicate([Target](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.Family == EWBRuntimeMatchActionFamily::Move && Action.TargetTile == Target;
	});
	return Move != nullptr
		&& HUD->SelectUnitSource(Move->SourceUnitId).bOk
		&& HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Move).bOk
		&& HUD->SelectTileTarget(Target).bOk
		&& HUD->SubmitResolvedAction().bOk;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDCreationBindingTest,
	"Wandbound.Runtime.PlayableUI.HUD.CreationBindingAndIdempotentRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDCreationBindingTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHUDWidget* HUD = NewObject<UWBRuntimeMatchHUDWidget>(GetTransientPackage());
	HUD->EnsureWidgetTreeBuilt();
	TestTrue(TEXT("C++ widget creates all regions without an asset"), HUD->AreRequiredRegionsBuilt());
	TestFalse(TEXT("Missing host fails safely"), HUD->BindToMatchHost(nullptr));
	UWBRuntimeMatchHostComponent* FirstHost = MakeHost();
	UWBRuntimeMatchHostComponent* SecondHost = MakeHost();
	TestNotNull(TEXT("First development host"), FirstHost);
	TestNotNull(TEXT("Second development host"), SecondHost);
	if (FirstHost == nullptr || SecondHost == nullptr) return false;
	TestTrue(TEXT("HUD binds"), HUD->BindToMatchHost(FirstHost));
	const int32 InitialCardCount = HUD->GetDisplayedHandWidgetCount();
	HUD->RefreshFromHost();
	HUD->RefreshFromHost();
	TestEqual(TEXT("Repeated refresh is idempotent"), HUD->GetDisplayedHandWidgetCount(), InitialCardCount);
	TestTrue(TEXT("HUD rebinds"), HUD->BindToMatchHost(SecondHost));
	TestTrue(TEXT("Prior host was replaced"), HUD->GetBoundMatchHost() == SecondHost);
	HUD->UnbindFromMatchHost();
	TestNull(TEXT("Explicit unbind clears host"), HUD->GetBoundMatchHost());
	TestEqual(TEXT("Explicit unbind clears private card widgets"), HUD->GetDisplayedHandWidgetCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDHandPrivacyTest,
	"Wandbound.Runtime.PlayableUI.Hand.ViewerSwitchClearsPrivateIdentityCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDHandPrivacyTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	TestNotNull(TEXT("HUD fixture"), HUD);
	if (HUD == nullptr) return false;
	TArray<FString> FirstIds;
	for (const FWBRuntimeHandCardPresentation& Card : HUD->GetDisplayedHand())
	{
		FirstIds.Add(Card.CardInstanceId);
		TestTrue(TEXT("Viewer zero owns every displayed identity"), Card.CardInstanceId.StartsWith(TEXT("p0_")));
	}
	TestTrue(TEXT("Initial private hand is visible"), !FirstIds.IsEmpty());
	HUD->bAllowDevelopmentViewerSwitch = true;
	TestTrue(TEXT("Explicit hot-seat viewer switch succeeds"), HUD->RequestViewerSwitch().bOk);
	for (const FWBRuntimeHandCardPresentation& Card : HUD->GetDisplayedHand())
	{
		TestTrue(TEXT("Viewer one owns every replacement identity"), Card.CardInstanceId.StartsWith(TEXT("p1_")));
		TestFalse(TEXT("Prior private identity was removed"), FirstIds.Contains(Card.CardInstanceId));
	}
	TestEqual(TEXT("Inactive viewer has no selectable legal actions"), HUD->GetDisplayedActions().Num(), 0);
	TestEqual(TEXT("Card selection cache cleared"), HUD->GetDisplayedSelection().SelectedCardInstanceId, FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDCardSelectionTest,
	"Wandbound.Runtime.PlayableUI.Hand.CardSelectionUsesCoordinatorFamiliesAndTiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDCardSelectionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Summon = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Summon);
	TestTrue(TEXT("Development hand has a legal summon"), !Summon.ActionId.IsEmpty());
	if (Summon.ActionId.IsEmpty()) return false;
	TestTrue(TEXT("Card source selects"), HUD->SelectHandCard(Summon.SourceCardInstanceId).bOk);
	TestTrue(TEXT("Summon family remains available"), !FindFamily(HUD->GetDisplayedActions(), EWBRuntimeMatchActionFamily::Summon).ActionId.IsEmpty());
	TestTrue(TEXT("Same card exposes Discard as a distinct legal family"), !FindFamily(HUD->GetDisplayedActions(), EWBRuntimeMatchActionFamily::Discard).ActionId.IsEmpty());
	TestTrue(TEXT("Multiple families do not resolve implicitly"), HUD->GetDisplayedSelection().ResolvedActionId.IsEmpty());
	TestTrue(TEXT("Family selection succeeds"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Summon).bOk);
	const TArray<FWBRuntimeBoardTilePresentation> Tiles = Host->GetCurrentTiles();
	TestTrue(TEXT("Exact summon tile is highlighted from legal metadata"), Tiles.ContainsByPredicate([Summon](const FWBRuntimeBoardTilePresentation& Tile)
	{
		return Tile.Tile == Summon.TargetTile && Tile.Highlight == EWBRuntimeBoardHighlight::Summon;
	}));
	TestFalse(TEXT("Illegal center tile does not resolve"), HUD->SelectTileTarget(FIntPoint(4, 4)).bOk);
	TestTrue(TEXT("Legal coordinator tile resolves"), HUD->SelectTileTarget(Summon.TargetTile).bOk);
	TestEqual(TEXT("Resolved ID is the coordinator ID"), HUD->GetDisplayedSelection().ResolvedActionId, Summon.ActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDUnitSelectionTest,
	"Wandbound.Runtime.PlayableUI.Unit.SourceSelectionAndMoveHighlightsAreLegalOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDUnitSelectionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Move = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Move);
	const FWBRuntimeUnitPresentation Opponent = FindOwnedHero(Host, 1);
	TestTrue(TEXT("Move exists"), !Move.ActionId.IsEmpty());
	TestTrue(TEXT("Opponent exists"), Opponent.UnitId >= 0);
	if (Move.ActionId.IsEmpty() || Opponent.UnitId < 0) return false;
	TestFalse(TEXT("Opponent cannot be selected as a source"), HUD->SelectUnitSource(Opponent.UnitId).bOk);
	TestFalse(TEXT("Unknown or neutral ID cannot be selected as a source"), HUD->SelectUnitSource(INDEX_NONE).bOk);
	TestTrue(TEXT("Friendly legal source selects"), HUD->SelectUnitSource(Move.SourceUnitId).bOk);
	TestEqual(TEXT("Stable source ID retained"), HUD->GetDisplayedSelection().SelectedUnitId, Move.SourceUnitId);
	TestTrue(TEXT("Move family filters"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Move).bOk);
	for (const FWBRuntimeBoardTilePresentation& Tile : Host->GetCurrentTiles())
	{
		if (Tile.Highlight != EWBRuntimeBoardHighlight::Move) continue;
		TestTrue(TEXT("Every highlighted move tile has a legal action"), HUD->GetDisplayedActions().ContainsByPredicate([&Tile](const FWBRuntimeLegalActionPresentation& Action)
		{
			return Action.Family == EWBRuntimeMatchActionFamily::Move && Action.TargetTile == Tile.Tile;
		}));
	}
	Host->GetCurrentPresentation();
	TestEqual(TEXT("Selection survives a presentation-only widget refresh"), HUD->GetDisplayedSelection().SelectedUnitId, Move.SourceUnitId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDAmbiguityTest,
	"Wandbound.Runtime.PlayableUI.Targeting.AmbiguityRequiresExactStableIdChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDAmbiguityTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeUnitPresentation Source = FindOwnedHero(Host, 0);
	const FWBRuntimeUnitPresentation Target = FindOwnedHero(Host, 1);
	TestTrue(TEXT("Activation source"), Source.UnitId >= 0);
	TestTrue(TEXT("Activation target"), Target.UnitId >= 0);
	if (Source.UnitId < 0 || Target.UnitId < 0) return false;
	TestTrue(TEXT("Source selects"), HUD->SelectUnitSource(Source.UnitId).bOk);
	TestTrue(TEXT("Activation family selects"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Activation).bOk);
	const FWBRuntimeMatchCommandResult Ambiguous = HUD->SelectUnitTarget(Target.UnitId);
	TestFalse(TEXT("Ambiguous target does not resolve implicitly"), Ambiguous.bOk);
	TestEqual(TEXT("Structured ambiguity reason"), Ambiguous.Reason, FString(TEXT("selected_unit_action_ambiguous")));
	TestTrue(TEXT("At least two exact candidates returned"), Ambiguous.CandidateActionIds.Num() >= 2);
	TestTrue(TEXT("Candidate IDs are sorted deterministically"), Algo::IsSorted(Ambiguous.CandidateActionIds));
	TestEqual(TEXT("Candidate widgets mirror candidates"), HUD->GetAmbiguityWidgetCount(), Ambiguous.CandidateActionIds.Num());
	const FString Chosen = Ambiguous.CandidateActionIds.Last();
	TestTrue(TEXT("Explicit candidate resolves"), HUD->ChooseAmbiguousAction(Chosen).bOk);
	const FWBRuntimeMatchCommandResult Submitted = HUD->SubmitResolvedAction();
	TestTrue(TEXT("Chosen activation submits"), Submitted.bOk);
	TestEqual(TEXT("Exact chosen stable ID submitted"), Submitted.ActionId, Chosen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDMoveSubmissionTest,
	"Wandbound.Runtime.PlayableUI.Submission.MoveUpdatesBoardAndClearsSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDMoveSubmissionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Move = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Move);
	if (Move.ActionId.IsEmpty()) return false;
	const int32 UnitId = Move.SourceUnitId;
	const FIntPoint Target = Move.TargetTile;
	TestTrue(TEXT("Source"), HUD->SelectUnitSource(UnitId).bOk);
	TestTrue(TEXT("Move family"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Move).bOk);
	TestTrue(TEXT("Target"), HUD->SelectTileTarget(Target).bOk);
	TestTrue(TEXT("Move submission"), HUD->SubmitResolvedAction().bOk);
	const FWBRuntimeUnitPresentation* Moved = Host->GetCurrentUnits().FindByPredicate([UnitId](const FWBRuntimeUnitPresentation& Unit)
	{
		return Unit.UnitId == UnitId;
	});
	TestNotNull(TEXT("Moved unit remains public"), Moved);
	if (Moved != nullptr) TestEqual(TEXT("Board reflects core result"), Moved->Tile, Target);
	TestEqual(TEXT("Selection clears on success"), HUD->GetDisplayedSelection().SelectedUnitId, -1);
	TestFalse(TEXT("Highlights clear on success"), Host->GetCurrentTiles().ContainsByPredicate([](const FWBRuntimeBoardTilePresentation& Tile)
	{
		return Tile.Highlight != EWBRuntimeBoardHighlight::None;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDStaleRecoveryTest,
	"Wandbound.Runtime.PlayableUI.Rejection.StaleOptionRefreshesWithoutMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDStaleRecoveryTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Captured = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Move);
	if (Captured.ActionId.IsEmpty()) return false;
	const TArray<FWBRuntimeUnitPresentation> Before = Host->GetCurrentUnits();
	TestTrue(TEXT("Host advances presentation revision"), Host->RefreshPresentation().bOk);
	const FWBRuntimeMatchCommandResult Rejected = HUD->SubmitActionOption(Captured);
	TestFalse(TEXT("Stale action rejected"), Rejected.bOk);
	TestEqual(TEXT("Stale revision is distinguished"), Rejected.Reason, FString(TEXT("stale_decision_revision")));
	TestEqual(TEXT("Safe structured reason is shown"), HUD->GetFeedbackText(), Rejected.Reason);
	TestEqual(TEXT("Rejected action does not change public units"), Host->GetCurrentUnits().Num(), Before.Num());
	TestFalse(TEXT("Stale candidate removed from current actions"), Host->GetCurrentLegalActions().ContainsByPredicate([&Captured](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.ActionId == Captured.ActionId && Action.DecisionRevision == Captured.DecisionRevision;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDCardAndTurnSubmissionTest,
	"Wandbound.Runtime.PlayableUI.Submission.SummonThenEndTurnRefreshesHandAndTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDCardAndTurnSubmissionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Summon = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Summon);
	if (Summon.ActionId.IsEmpty()) return false;
	const int32 HandBefore = HUD->GetDisplayedHand().Num();
	const int32 UnitsBefore = Host->GetCurrentUnits().Num();
	TestTrue(TEXT("Card selects"), HUD->SelectHandCard(Summon.SourceCardInstanceId).bOk);
	TestTrue(TEXT("Summon family selects"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Summon).bOk);
	TestTrue(TEXT("Summon target selects"), HUD->SelectTileTarget(Summon.TargetTile).bOk);
	TestTrue(TEXT("Summon submits"), HUD->SubmitResolvedAction().bOk);
	TestEqual(TEXT("Card leaves displayed own hand"), HUD->GetDisplayedHand().Num(), HandBefore - 1);
	TestEqual(TEXT("Public unit appears"), Host->GetCurrentUnits().Num(), UnitsBefore + 1);
	const int32 BeforePlayer = Host->GetCurrentPresentation().CurrentPlayerId;
	TestTrue(TEXT("End Turn routes through host"), HUD->RequestEndTurn().bOk);
	TestNotEqual(TEXT("Turn owner changes"), Host->GetCurrentPresentation().CurrentPlayerId, BeforePlayer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDEquipSubmissionTest,
	"Wandbound.Runtime.PlayableUI.Submission.EquipUsesCardAndStableUnitTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDEquipSubmissionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Equip = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Equip);
	TestTrue(TEXT("Development hand has legal equipment"), !Equip.ActionId.IsEmpty());
	if (Equip.ActionId.IsEmpty()) return false;
	const TArray<FWBRuntimeUnitPresentation> UnitsBefore = Host->GetCurrentUnits();
	const FWBRuntimeUnitPresentation* BeforeTarget = UnitsBefore.FindByPredicate([Equip](const FWBRuntimeUnitPresentation& Unit)
	{
		return Unit.UnitId == Equip.TargetUnitId;
	});
	const int32 RLUsedBefore = BeforeTarget != nullptr ? BeforeTarget->RLUsed : INDEX_NONE;
	const int32 HandBefore = HUD->GetDisplayedHand().Num();
	TestTrue(TEXT("Wand card selects"), HUD->SelectHandCard(Equip.SourceCardInstanceId).bOk);
	TestTrue(TEXT("Equip family selects"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Equip).bOk);
	TestTrue(TEXT("Stable unit target resolves"), HUD->SelectUnitTarget(Equip.TargetUnitId).bOk);
	TestTrue(TEXT("Exact equip action submits"), HUD->SubmitResolvedAction().bOk);
	TestEqual(TEXT("Equipped card leaves displayed hand"), HUD->GetDisplayedHand().Num(), HandBefore - 1);
	const TArray<FWBRuntimeUnitPresentation> UnitsAfter = Host->GetCurrentUnits();
	const FWBRuntimeUnitPresentation* AfterTarget = UnitsAfter.FindByPredicate([Equip](const FWBRuntimeUnitPresentation& Unit)
	{
		return Unit.UnitId == Equip.TargetUnitId;
	});
	TestNotNull(TEXT("Equipped unit remains public"), AfterTarget);
	if (AfterTarget != nullptr) TestTrue(TEXT("Public RL usage reflects equipment"), AfterTarget->RLUsed > RLUsedBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDDiscardSubmissionTest,
	"Wandbound.Runtime.PlayableUI.Submission.DiscardResolvesOnlyAfterExplicitFamilyChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDDiscardSubmissionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	if (HUD == nullptr) return false;
	const FWBRuntimeLegalActionPresentation Discard = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Discard);
	TestTrue(TEXT("Development hand has a discard action"), !Discard.ActionId.IsEmpty());
	if (Discard.ActionId.IsEmpty()) return false;
	const int32 HandBefore = HUD->GetDisplayedHand().Num();
	TestTrue(TEXT("Discard card selects"), HUD->SelectHandCard(Discard.SourceCardInstanceId).bOk);
	TestTrue(TEXT("No action is chosen before family input"), HUD->GetDisplayedSelection().ResolvedActionId.IsEmpty());
	const FWBRuntimeMatchCommandResult Family = HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Discard);
	TestTrue(TEXT("Explicit Discard family resolves"), Family.bOk);
	TestEqual(TEXT("No-target family resolves exact stable ID"), HUD->GetDisplayedSelection().ResolvedActionId, Discard.ActionId);
	TestTrue(TEXT("Discard submits through host"), HUD->SubmitResolvedAction().bOk);
	TestEqual(TEXT("Discarded card leaves own hand"), HUD->GetDisplayedHand().Num(), HandBefore - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDTerminalWorldTest,
	"Wandbound.Runtime.PlayableUI.World.NPCDefeatShowsOneTerminalOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDTerminalWorldTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateHUDTestWorld();
	TestNotNull(TEXT("Transient world"), World);
	if (World == nullptr) return false;
	AWBBoardViewActor* Board = World->SpawnActor<AWBBoardViewActor>();
	AActor* Owner = World->SpawnActor<AActor>();
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(Owner, TEXT("RuntimeMatchHost"));
	Owner->AddInstanceComponent(Host);
	Host->RegisterComponent();
	Host->BoardActor = Board;
	TestTrue(TEXT("Fragile deterministic match initializes"), Host->InitializeDevelopmentMatch(0, true).bOk);
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	TestNotNull(TEXT("HUD binds in transient world"), HUD);
	if (HUD != nullptr)
	{
		TestTrue(TEXT("First marker approach move"), SubmitMoveTo(HUD, FIntPoint(3, 8)));
		TestTrue(TEXT("Second marker approach move"), SubmitMoveTo(HUD, FIntPoint(3, 7)));
		for (const int32 SelectableUnitId : Host->GetSelectableUnitIds())
		{
			const TArray<FWBRuntimeUnitPresentation> PublicUnits = Host->GetCurrentUnits();
			const FWBRuntimeUnitPresentation* SelectableUnit = PublicUnits.FindByPredicate([SelectableUnitId](const FWBRuntimeUnitPresentation& Unit)
			{
				return Unit.UnitId == SelectableUnitId;
			});
			TestTrue(TEXT("Every command source is an active-player non-NPC unit"), SelectableUnit != nullptr
				&& !SelectableUnit->bNeutralNPC
				&& SelectableUnit->OwnerId == Host->GetCurrentPresentation().CurrentPlayerId);
		}
		TestTrue(TEXT("End Turn processes NPC phase"), HUD->RequestEndTurn().bOk);
		TestTrue(TEXT("Terminal overlay visible"), HUD->IsTerminalOverlayVisible());
		TestTrue(TEXT("Host terminal"), Host->IsGameOver());
		TestEqual(TEXT("Winner is opponent"), Host->GetWinnerPlayerId(), 1);
		TestEqual(TEXT("Terminal clears hand interaction"), HUD->GetDisplayedHand().Num(), 0);
		TestEqual(TEXT("Terminal clears legal actions"), Host->GetCurrentLegalActions().Num(), 0);
		TestEqual(TEXT("Final board remains"), Board->GetTilePresentations().Num(), 81);
		HUD->RefreshFromHost();
		TestTrue(TEXT("Repeated refresh retains one terminal state"), HUD->IsTerminalOverlayVisible());
		TestFalse(TEXT("Further End Turn fails"), HUD->RequestEndTurn().bOk);
		HUD->UnbindFromMatchHost();
	}
	DestroyHUDTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDControllerForwardingTest,
	"Wandbound.Runtime.PlayableUI.Controller.ForwardsStableIdsTilesAndCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDControllerForwardingTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateHUDTestWorld();
	if (World == nullptr) return false;
	AWBBoardViewActor* Board = World->SpawnActor<AWBBoardViewActor>();
	AWBRuntimePlayerController* Controller = World->SpawnActor<AWBRuntimePlayerController>();
	UWBRuntimeMatchHostComponent* Host = MakeHost();
	UWBRuntimeMatchHUDWidget* HUD = MakeHUD(Host);
	TestNotNull(TEXT("Controller"), Controller);
	if (Controller != nullptr && HUD != nullptr)
	{
		Controller->SetRuntimeReferences(Host, Board, HUD);
		const FWBRuntimeLegalActionPresentation Move = FindFamily(Host->GetCurrentLegalActions(), EWBRuntimeMatchActionFamily::Move);
		if (!Move.ActionId.IsEmpty())
		{
			TestTrue(TEXT("Stable unit ID forwarded"), Controller->ForwardUnitSelection(Move.SourceUnitId).bOk);
			TestTrue(TEXT("Family selected through HUD"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Move).bOk);
			TestTrue(TEXT("Canonical tile forwarded"), Controller->ForwardTileSelection(Move.TargetTile).bOk);
			Controller->ForwardClearSelection();
			TestEqual(TEXT("Clear input path clears source"), HUD->GetDisplayedSelection().SelectedUnitId, -1);
		}
		TestTrue(TEXT("End Turn input path routes through host"), Controller->ForwardEndTurn().bOk);
	}
	DestroyHUDTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeHUDSourceFirewallTest,
	"Wandbound.Runtime.PlayableUI.Guards.UIAndControllerRemainPresentationOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeHUDSourceFirewallTest::RunTest(const FString& Parameters)
{
	const FString RuntimePrivate = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/Private"));
	FString HUDSource;
	FString ControllerSource;
	FString CoreBuild;
	TestTrue(TEXT("HUD source readable"), FFileHelper::LoadFileToString(HUDSource, *FPaths::Combine(RuntimePrivate, TEXT("WBRuntimeMatchHUDWidget.cpp"))));
	TestTrue(TEXT("Controller source readable"), FFileHelper::LoadFileToString(ControllerSource, *FPaths::Combine(RuntimePrivate, TEXT("WBRuntimePlayerController.cpp"))));
	TestTrue(TEXT("Core build readable"), FFileHelper::LoadFileToString(CoreBuild, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/WandboundCore.Build.cs"))));
	for (const FString& Forbidden : { TEXT("WBRules"), TEXT("WBEffectRunner"), TEXT("FWBGameStateData"), TEXT("GetMutableStateForTest"), TEXT("EWBMarkerType"), TEXT("OpponentHand") })
	{
		TestFalse(*FString::Printf(TEXT("HUD excludes %s"), *Forbidden), HUDSource.Contains(Forbidden));
		TestFalse(*FString::Printf(TEXT("Controller excludes %s"), *Forbidden), ControllerSource.Contains(Forbidden));
	}
	TestFalse(TEXT("Core has no UMG dependency"), CoreBuild.Contains(TEXT("UMG")));
	TestFalse(TEXT("Core has no runtime dependency"), CoreBuild.Contains(TEXT("WandboundRuntime")));
	return true;
}

#endif
