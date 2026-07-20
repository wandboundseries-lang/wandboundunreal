#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "WBBoardViewActor.h"
#include "WBCardDefinitionRepository.h"
#include "WBMatchCoordinator.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeUnitPresentationActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBCardDefinition MakeCharacter(const FString& CardId, const EWBCardDefinitionKind Kind = EWBCardDefinitionKind::Character)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	Definition.CharacterStats.HP = Kind == EWBCardDefinitionKind::NPC ? 5 : 8;
	Definition.CharacterStats.ATK = Kind == EWBCardDefinitionKind::NPC ? 2 : 3;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 3;
	return Definition;
}

FWBCardDefinition MakeWand()
{
	FWBCardDefinition Definition;
	Definition.CardId = TEXT("test_wand");
	Definition.PublicName = TEXT("Test Wand");
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = 1;
	return Definition;
}

FWBCardDefinition MakeSimpleDefinition(const FString& CardId, const EWBCardDefinitionKind Kind)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = Kind;
	return Definition;
}

FWBCardInstanceRef MakeCard(const FString& InstanceId, const FString& CardId, const int32 OwnerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerId;
	return Card;
}

FWBMatchPlayerSetup MakePlayer(const int32 PlayerId)
{
	const FString Prefix = FString::Printf(TEXT("p%d"), PlayerId);
	FWBMatchPlayerSetup Player;
	Player.PlayerId = PlayerId;
	Player.HeroInstanceId = Prefix + TEXT("_hero_instance");
	Player.HeroCardId = PlayerId == 0 ? TEXT("hero_alpha") : TEXT("hero_beta");
	Player.OrderedDeck.Add(MakeCard(Player.HeroInstanceId, Player.HeroCardId, PlayerId));
	Player.OrderedDeck.Add(MakeCard(Prefix + TEXT("_student"), TEXT("student"), PlayerId));
	Player.OrderedDeck.Add(MakeCard(Prefix + TEXT("_wand"), TEXT("test_wand"), PlayerId));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Player.OrderedDeck.Add(MakeCard(
			FString::Printf(TEXT("%s_private_%d"), *Prefix, Index),
			TEXT("filler"),
			PlayerId));
	}
	return Player;
}

FWBSetupMarkerPlacement MakeMarker(
	const int32 PlayerId,
	const EWBMarkerType Type,
	const FWBTile& Tile,
	const int32 Order)
{
	FWBSetupMarkerPlacement Marker;
	Marker.PlayerId = PlayerId;
	Marker.Type = Type;
	Marker.Tile = Tile;
	Marker.DefinitionId = Type == EWBMarkerType::Trap ? TEXT("basic_trap") : TEXT("basic_npc");
	Marker.PlacementOrder = Order;
	return Marker;
}

FWBMatchInitializationRequest MakeRequest(const bool bFragileFirstHero = false)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 91234;
	Request.FirstPlayerId = 0;
	Request.Repository.RepositoryId = TEXT("runtime_match_host_tests");
	Request.Repository.SourceVersion = TEXT("1");
	Request.Repository.Definitions = {
		MakeCharacter(TEXT("hero_alpha")),
		MakeCharacter(TEXT("hero_beta")),
		MakeCharacter(TEXT("student")),
		MakeWand(),
		MakeSimpleDefinition(TEXT("filler"), EWBCardDefinitionKind::Action),
		MakeSimpleDefinition(TEXT("basic_trap"), EWBCardDefinitionKind::Trap),
		MakeCharacter(TEXT("basic_npc"), EWBCardDefinitionKind::NPC)
	};
	if (bFragileFirstHero)
	{
		Request.Repository.Definitions[0].CharacterStats.HP = 1;
	}
	Request.Players = { MakePlayer(0), MakePlayer(1) };
	Request.MarkerPlacements = {
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(0, 8), 0),
		MakeMarker(0, EWBMarkerType::Trap, FWBTile(1, 8), 1),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(2, 8), 2),
		MakeMarker(0, EWBMarkerType::NPC, FWBTile(3, 7), 3),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(0, 0), 4),
		MakeMarker(1, EWBMarkerType::Trap, FWBTile(1, 0), 5),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(2, 0), 6),
		MakeMarker(1, EWBMarkerType::NPC, FWBTile(3, 1), 7)
	};
	return Request;
}

const FWBMatchLegalAction* FindMove(const FWBMatchObservation& Observation)
{
	return Observation.LegalActions.FindByPredicate([](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Move;
	});
}

const FWBMatchLegalAction* FindMoveTo(const FWBMatchObservation& Observation, const FIntPoint Tile)
{
	return Observation.LegalActions.FindByPredicate([Tile](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::CoreAction
			&& Action.CoreAction.Type == EWBActionType::Move
			&& Action.CoreAction.ToTile == FWBTile(Tile.X, Tile.Y);
	});
}

UWorld* CreateRuntimeTestWorld()
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("WBRuntimeMatchHostTestWorld")));
	if (World != nullptr && GEngine != nullptr)
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
	}
	return World;
}

void DestroyRuntimeTestWorld(UWorld* World)
{
	if (World == nullptr) return;
	if (GEngine != nullptr) GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostInitializationTest,
	"Wandbound.Runtime.PlayableBoard.MatchHost.InitializationBuildsSafeObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostInitializationTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	const FWBRuntimeMatchCommandResult Result = Host->InitializeMatch(MakeRequest(), 0);
	TestTrue(TEXT("Initialization succeeds"), Result.bOk);
	TestTrue(TEXT("Host initialized"), Host->IsMatchInitialized());
	TestNotNull(TEXT("One coordinator exists"), Host->GetCoordinatorForInspection());
	TestEqual(TEXT("All board tiles projected"), Host->GetCurrentTiles().Num(), 81);
	TestEqual(TEXT("Both Heroes projected"), Host->GetCurrentUnits().Num(), 2);
	TestTrue(TEXT("Active viewer receives legal actions"), Host->GetCurrentLegalActions().Num() > 0);
	TestEqual(TEXT("Generation starts at one"), Host->GetCurrentPresentation().MatchGeneration, 1);
	TestTrue(TEXT("Decision revision assigned"), Host->GetCurrentPresentation().DecisionRevision > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostFailedInitializationTest,
	"Wandbound.Runtime.PlayableBoard.MatchHost.FailedInitializationIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostFailedInitializationTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	FWBMatchInitializationRequest Invalid;
	const FWBRuntimeMatchCommandResult Result = Host->InitializeMatch(Invalid, 0);
	TestFalse(TEXT("Initialization fails"), Result.bOk);
	TestFalse(TEXT("Host remains uninitialized"), Host->IsMatchInitialized());
	TestNull(TEXT("No coordinator retained"), Host->GetCoordinatorForInspection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostStaleRevisionTest,
	"Wandbound.Runtime.PlayableBoard.MatchHost.StaleGenerationAndDecisionRejectedAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostStaleRevisionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	TestTrue(TEXT("Initialization succeeds"), Host->InitializeMatch(MakeRequest(), 0).bOk);
	const FWBMatchLegalAction* Move = FindMove(Host->GetCurrentObservation());
	TestNotNull(TEXT("Move available"), Move);
	if (Move == nullptr) return false;
	const FString ActionId = Move->ActionId;
	const int32 Generation = Host->GetCurrentPresentation().MatchGeneration;
	const int32 Revision = Host->GetCurrentPresentation().DecisionRevision;
	const int32 BeforeX = Host->GetCurrentUnits()[0].Tile.X;

	TestTrue(TEXT("Reinitialization succeeds"), Host->InitializeMatch(MakeRequest(), 0).bOk);
	TestEqual(TEXT("Generation increments"), Host->GetCurrentPresentation().MatchGeneration, Generation + 1);
	const FWBRuntimeMatchCommandResult StaleGeneration = Host->SubmitLegalActionAtRevision(ActionId, Generation, Revision);
	TestFalse(TEXT("Old generation rejected"), StaleGeneration.bOk);
	TestEqual(TEXT("Old generation reason"), StaleGeneration.Reason, FString(TEXT("stale_match_generation")));
	TestEqual(TEXT("Presentation unchanged"), Host->GetCurrentUnits()[0].Tile.X, BeforeX);

	const FWBRuntimeMatchCommandResult StaleDecision = Host->SubmitLegalActionAtRevision(
		ActionId,
		Host->GetCurrentPresentation().MatchGeneration,
		Host->GetCurrentPresentation().DecisionRevision - 1);
	TestFalse(TEXT("Old decision rejected"), StaleDecision.bOk);
	TestEqual(TEXT("Old decision reason"), StaleDecision.Reason, FString(TEXT("stale_decision_revision")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeBoardTransformedCoordinatesTest,
	"Wandbound.Runtime.PlayableBoard.Coordinates.TransformedCustomSpacingRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeBoardTransformedCoordinatesTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* Board = NewObject<AWBBoardViewActor>(GetTransientPackage());
	Board->ViewSettings.TileSize = 175.0f;
	Board->SetActorTransform(FTransform(FRotator(0.0, 37.0, 0.0), FVector(900.0, -320.0, 140.0)));
	for (const FIntPoint Tile : { FIntPoint(0, 0), FIntPoint(8, 0), FIntPoint(4, 4), FIntPoint(0, 8), FIntPoint(8, 8) })
	{
		FIntPoint RoundTrip;
		TestTrue(*FString::Printf(TEXT("Tile %s converts back"), *Tile.ToString()), Board->WorldToTile(Board->TileToWorld(Tile), RoundTrip));
		TestEqual(TEXT("Round-trip tile"), RoundTrip, Tile);
	}
	FIntPoint Outside;
	TestFalse(TEXT("Outside point rejected"), Board->WorldToTile(Board->TileToWorld(FIntPoint(8, 8)) + FVector(10000.0, 0.0, 0.0), Outside));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostMarkerFirewallTest,
	"Wandbound.Runtime.PlayableBoard.HiddenInfo.ViewerSwitchKeepsMarkersConcealed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostMarkerFirewallTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	TestTrue(TEXT("Initialization succeeds"), Host->InitializeMatch(MakeRequest(), 0).bOk);
	TArray<FWBRuntimeBoardTilePresentation> FirstTiles = Host->GetCurrentTiles();
	const int32 FirstMarkerCount = FirstTiles.FilterByPredicate([](const FWBRuntimeBoardTilePresentation& Tile)
	{
		return Tile.bHasConcealedMarker;
	}).Num();
	TestEqual(TEXT("All concealed markers displayed generically"), FirstMarkerCount, 8);
	TestTrue(TEXT("Viewer zero has private hand"), Host->GetCurrentObservation().CardZones.OwnHand.Cards.Num() > 0);

	TestTrue(TEXT("Viewer switch succeeds"), Host->SetCurrentViewerPlayerId(1).bOk);
	const int32 SecondMarkerCount = Host->GetCurrentTiles().FilterByPredicate([](const FWBRuntimeBoardTilePresentation& Tile)
	{
		return Tile.bHasConcealedMarker;
	}).Num();
	TestEqual(TEXT("Marker presentation equivalent"), SecondMarkerCount, FirstMarkerCount);
	for (const FWBObservedMarkerSummary& Marker : Host->GetCurrentObservation().CardZones.PublicSummary.Markers)
	{
		TestEqual(TEXT("Marker remains hidden"), Marker.PublicState, EWBMarkerPublicState::Hidden);
	}
	TestEqual(TEXT("New viewer owns cached hand"), Host->GetCurrentObservation().CardZones.OwnHand.OwnerPlayerId, 1);
	TestEqual(TEXT("Inactive viewer has no legal actions"), Host->GetCurrentLegalActions().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostLegalSelectionTest,
	"Wandbound.Runtime.PlayableBoard.Selection.UsesCoordinatorLegalTargetsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostLegalSelectionTest::RunTest(const FString& Parameters)
{
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(GetTransientPackage());
	TestTrue(TEXT("Initialization succeeds"), Host->InitializeMatch(MakeRequest(), 0).bOk);
	const FWBMatchLegalAction* Move = FindMove(Host->GetCurrentObservation());
	TestNotNull(TEXT("Move available"), Move);
	if (Move == nullptr) return false;
	TestTrue(TEXT("Source selects"), Host->SelectUnit(Move->CoreAction.SourceUnitId).bOk);
	const FIntPoint LegalTarget(Move->CoreAction.ToTile.X, Move->CoreAction.ToTile.Y);
	TestTrue(TEXT("Coordinator target highlighted"), Host->GetCurrentTiles().ContainsByPredicate([LegalTarget](const FWBRuntimeBoardTilePresentation& Tile)
	{
		return Tile.Tile == LegalTarget && Tile.Highlight == EWBRuntimeBoardHighlight::Move;
	}));
	const FWBRuntimeMatchCommandResult Illegal = Host->SelectTile(FIntPoint(4, 4));
	TestFalse(TEXT("Non-legal tile cannot resolve"), Illegal.bOk);
	const FWBRuntimeMatchCommandResult Selected = Host->SelectTile(LegalTarget);
	TestTrue(TEXT("Legal tile resolves"), Selected.bOk);
	TestEqual(TEXT("Stable action ID returned"), Selected.ActionId, Move->ActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostTransientWorldMoveTest,
	"Wandbound.Runtime.PlayableBoard.World.MatchBoardMovePreservesUnitActorIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostTransientWorldMoveTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRuntimeTestWorld();
	TestNotNull(TEXT("Transient world"), World);
	if (World == nullptr) return false;

	AWBBoardViewActor* Board = World->SpawnActor<AWBBoardViewActor>();
	AActor* Owner = World->SpawnActor<AActor>();
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(Owner, TEXT("RuntimeMatchHost"));
	Owner->AddInstanceComponent(Host);
	Host->RegisterComponent();
	Host->BoardActor = Board;
	TestTrue(TEXT("Match initializes in world"), Host->InitializeMatch(MakeRequest(), 0).bOk);
	TestEqual(TEXT("Board has 81 tiles"), Board->GetTilePresentations().Num(), 81);
	TestEqual(TEXT("Two stable unit actors"), Board->GetUnitPresentationActorCount(), 2);

	const FWBMatchLegalAction* Move = FindMove(Host->GetCurrentObservation());
	TestNotNull(TEXT("Move available"), Move);
	if (Move != nullptr)
	{
		AWBRuntimeUnitPresentationActor* Before = Board->FindUnitPresentationActor(Move->CoreAction.SourceUnitId);
		const FIntPoint Target(Move->CoreAction.ToTile.X, Move->CoreAction.ToTile.Y);
		TestTrue(TEXT("Select unit"), Host->SelectUnit(Move->CoreAction.SourceUnitId).bOk);
		TestTrue(TEXT("Select target"), Host->SelectTile(Target).bOk);
		TestTrue(TEXT("Submit move"), Host->SubmitSelectedAction().bOk);
		AWBRuntimeUnitPresentationActor* After = Board->FindUnitPresentationActor(Move->CoreAction.SourceUnitId);
		TestTrue(TEXT("Same Actor identity survives move"), Before == After);
		TestEqual(TEXT("Actor moved to target tile"), After->GetPresentation().Tile, Target);
		TestTrue(TEXT("Decision revision advanced"), Host->GetCurrentPresentation().DecisionRevision > 1);
	}

	DestroyRuntimeTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeMatchHostTransientWorldNPCTerminalTest,
	"Wandbound.Runtime.PlayableBoard.World.NPCAutomaticPhaseRefreshesTerminalPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeMatchHostTransientWorldNPCTerminalTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRuntimeTestWorld();
	TestNotNull(TEXT("Transient world"), World);
	if (World == nullptr) return false;

	AWBBoardViewActor* Board = World->SpawnActor<AWBBoardViewActor>();
	AActor* Owner = World->SpawnActor<AActor>();
	UWBRuntimeMatchHostComponent* Host = NewObject<UWBRuntimeMatchHostComponent>(Owner, TEXT("RuntimeMatchHost"));
	Owner->AddInstanceComponent(Host);
	Host->RegisterComponent();
	Host->BoardActor = Board;
	TestTrue(TEXT("Fragile-Hero fixture initializes"), Host->InitializeMatch(MakeRequest(true), 0).bOk);

	for (const FIntPoint Target : { FIntPoint(3, 8), FIntPoint(3, 7) })
	{
		const FWBMatchLegalAction* Move = FindMoveTo(Host->GetCurrentObservation(), Target);
		TestNotNull(*FString::Printf(TEXT("Move to %s is legal"), *Target.ToString()), Move);
		if (Move == nullptr)
		{
			DestroyRuntimeTestWorld(World);
			return false;
		}
		const FString ActionId = Move->ActionId;
		TestTrue(TEXT("Coordinator move submission succeeds"), Host->SubmitLegalActionById(ActionId).bOk);
	}

	TestFalse(TEXT("Triggered concealed marker removed"), Host->GetCurrentTiles().ContainsByPredicate([](const FWBRuntimeBoardTilePresentation& Tile)
	{
		return Tile.Tile == FIntPoint(3, 7) && Tile.bHasConcealedMarker;
	}));
	const FWBRuntimeMatchCommandResult EndTurnResult = Host->EndTurn();
	TestTrue(TEXT("End turn and automatic NPC phase succeed"), EndTurnResult.bOk);
	TestTrue(TEXT("NPC phase traces reached runtime"), Host->GetCurrentPresentation().NPCTraceEventCount > 0);
	TestTrue(TEXT("NPC damage enters terminal state"), Host->IsGameOver());
	TestEqual(TEXT("Opponent wins"), Host->GetWinnerPlayerId(), 1);
	TestEqual(TEXT("Defeated player exposed"), Host->GetCurrentPresentation().DefeatedPlayerId, 0);
	TestEqual(TEXT("Terminal reason exposed"), Host->GetCurrentPresentation().TerminalReason, FString(TEXT("hero_defeated")));
	TestEqual(TEXT("Final revision captured"), Host->GetCurrentPresentation().FinalPresentationRevision, Host->GetCurrentPresentation().PresentationRevision);
	TestEqual(TEXT("Normal legal actions cleared"), Host->GetCurrentLegalActions().Num(), 0);
	TestEqual(TEXT("Defeated Hero presentation removed"), Host->GetCurrentUnits().FilterByPredicate([](const FWBRuntimeUnitPresentation& Unit)
	{
		return Unit.OwnerId == 0 && Unit.bHero;
	}).Num(), 0);
	TestEqual(TEXT("Dead Hero Actor removed"), Board->GetUnitPresentationActorCount(), Host->GetCurrentUnits().Num());
	TestFalse(TEXT("Terminal submission rejected"), Host->EndTurn().bOk);
	TestEqual(TEXT("Final 81-tile board retained"), Board->GetTilePresentations().Num(), 81);

	DestroyRuntimeTestWorld(World);
	return true;
}

#endif
