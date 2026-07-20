#include "Misc/AutomationTest.h"

#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SpectatorPawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeLocalPlayGameMode.h"
#include "WBRuntimeMatchBootstrapActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeMatchHUDWidget.h"
#include "WBRuntimePlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWorld* CreateLocalPlayWorld()
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("WBRuntimeLocalPlayTestWorld")));
	if (World != nullptr && GEngine != nullptr)
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
	}
	return World;
}

void DestroyLocalPlayWorld(UWorld* World)
{
	if (World == nullptr) return;
	for (TActorIterator<AWBRuntimeMatchBootstrapActor> It(World); It; ++It) It->ShutdownLocalPlay();
	if (GEngine != nullptr) GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
}

template <typename TActor>
int32 CountActors(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<TActor> It(World); It; ++It) ++Count;
	return Count;
}

struct FLocalPlayFixture
{
	UWorld* World = nullptr;
	AWBRuntimePlayerController* Controller = nullptr;
	AWBRuntimeMatchBootstrapActor* Bootstrap = nullptr;

	bool Create()
	{
		World = CreateLocalPlayWorld();
		if (World == nullptr) return false;
		Controller = World->SpawnActor<AWBRuntimePlayerController>();
		Bootstrap = World->SpawnActor<AWBRuntimeMatchBootstrapActor>();
		if (Controller != nullptr) Controller->bCreateHUDOnBeginPlay = false;
		return Controller != nullptr && Bootstrap != nullptr;
	}

	void Destroy()
	{
		DestroyLocalPlayWorld(World);
		World = nullptr;
		Controller = nullptr;
		Bootstrap = nullptr;
	}
};

FWBRuntimeLegalActionPresentation FindMove(UWBRuntimeMatchHostComponent* Host)
{
	const TArray<FWBRuntimeLegalActionPresentation> Actions = Host->GetCurrentLegalActions();
	const FWBRuntimeLegalActionPresentation* Move = Actions.FindByPredicate([](const FWBRuntimeLegalActionPresentation& Action)
	{
		return Action.Family == EWBRuntimeMatchActionFamily::Move;
	});
	return Move != nullptr ? *Move : FWBRuntimeLegalActionPresentation();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayGameModeConfigurationTest,
	"Wandbound.Runtime.LocalPlayBootstrap.GameMode.ConfigurationAndScriptPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayGameModeConfigurationTest::RunTest(const FString& Parameters)
{
	const AWBRuntimeLocalPlayGameMode* GameMode = GetDefault<AWBRuntimeLocalPlayGameMode>();
	TestTrue(TEXT("Runtime controller configured"), GameMode->PlayerControllerClass == AWBRuntimePlayerController::StaticClass());
	TestNull(TEXT("No gameplay Pawn required"), GameMode->DefaultPawnClass);
	TestNull(TEXT("No Blueprint HUD class required"), GameMode->HUDClass);
	TestNull(TEXT("No spectator Pawn required"), GameMode->SpectatorClass);
	UClass* Loaded = StaticLoadClass(
		AGameModeBase::StaticClass(),
		nullptr,
		TEXT("/Script/WandboundRuntime.WBRuntimeLocalPlayGameMode"));
	TestTrue(TEXT("Development GameMode loads by script path"), Loaded == AWBRuntimeLocalPlayGameMode::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayGameModeSingleBootstrapTest,
	"Wandbound.Runtime.LocalPlayBootstrap.GameMode.RepeatedStartupKeepsSingleBootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayGameModeSingleBootstrapTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateLocalPlayWorld();
	if (World == nullptr) return false;
	AWBRuntimePlayerController* Controller = World->SpawnActor<AWBRuntimePlayerController>();
	AWBRuntimeLocalPlayGameMode* GameMode = World->SpawnActor<AWBRuntimeLocalPlayGameMode>();
	TestNotNull(TEXT("Controller"), Controller);
	TestNotNull(TEXT("GameMode"), GameMode);
	if (Controller != nullptr && GameMode != nullptr)
	{
		Controller->bCreateHUDOnBeginPlay = false;
		TestTrue(TEXT("First startup succeeds"), GameMode->StartLocalPlayForController(Controller).bOk);
		AWBRuntimeMatchBootstrapActor* First = GameMode->GetBootstrapActor();
		TestTrue(TEXT("Repeated startup is idempotent"), GameMode->StartLocalPlayForController(Controller).bOk);
		TestTrue(TEXT("Same bootstrap retained"), First == GameMode->GetBootstrapActor());
		TestEqual(TEXT("Exactly one bootstrap Actor"), CountActors<AWBRuntimeMatchBootstrapActor>(World), 1);
	}
	DestroyLocalPlayWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayInitializationTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Bootstrap.InitializesCompleteObjectGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayInitializationTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	const FWBRuntimeLocalPlayResult Result = Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller);
	TestTrue(TEXT("Bootstrap initializes"), Result.bOk);
	TestEqual(TEXT("Bootstrap reaches Ready"), Fixture.Bootstrap->GetLocalPlayState(), EWBRuntimeLocalPlayState::Ready);
	TestNotNull(TEXT("One host component"), Fixture.Bootstrap->GetMatchHost());
	TestNotNull(TEXT("One board"), Fixture.Bootstrap->GetBoardActor());
	TestNotNull(TEXT("One camera"), Fixture.Bootstrap->GetCameraActor());
	TestNotNull(TEXT("One HUD"), Fixture.Bootstrap->GetHUDWidget());
	if (Result.bOk)
	{
		UWBRuntimeMatchHostComponent* Host = Fixture.Bootstrap->GetMatchHost();
		TestTrue(TEXT("Development match initialized"), Host->IsMatchInitialized());
		TestEqual(TEXT("Board displays 81 tiles"), Fixture.Bootstrap->GetBoardActor()->GetTilePresentations().Num(), 81);
		TestEqual(TEXT("Two Heroes visible"), Host->GetCurrentUnits().FilterByPredicate([](const FWBRuntimeUnitPresentation& Unit)
		{
			return Unit.bHero;
		}).Num(), 2);
		TestEqual(TEXT("All setup markers remain concealed"), Host->GetCurrentTiles().FilterByPredicate([](const FWBRuntimeBoardTilePresentation& Tile)
		{
			return Tile.bHasConcealedMarker;
		}).Num(), 8);
		TestEqual(TEXT("Six-card own hand appears"), Host->GetCurrentHandCards().Num(), 6);
		TestTrue(TEXT("Initial legal actions exist"), Host->GetCurrentLegalActions().Num() > 0);
		TestTrue(TEXT("HUD bound to host"), Fixture.Bootstrap->GetHUDWidget()->GetBoundMatchHost() == Host);
	}
	TestEqual(TEXT("Exactly one board Actor"), CountActors<AWBBoardViewActor>(Fixture.World), 1);
	TestEqual(TEXT("Exactly one camera Actor"), CountActors<ACameraActor>(Fixture.World), 1);
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayControllerBindingTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Controller.BindingAndPrebindFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayControllerBindingTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	const FWBRuntimeMatchCommandResult Before = Fixture.Controller->ForwardEndTurn();
	TestFalse(TEXT("Controller fails safely before binding"), Before.bOk);
	TestEqual(TEXT("Prebind diagnostic"), Before.Reason, FString(TEXT("runtime_interaction_not_bound")));
	const FWBRuntimeLocalPlayResult InitializeResult = Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller);
	TestTrue(TEXT("Bootstrap initializes"), InitializeResult.bOk);
	if (!InitializeResult.bOk)
	{
		Fixture.Destroy();
		return false;
	}
	TestTrue(TEXT("Controller has host"), Fixture.Controller->GetRuntimeMatchHost() == Fixture.Bootstrap->GetMatchHost());
	TestTrue(TEXT("Controller has board"), Fixture.Controller->GetRuntimeBoardActor() == Fixture.Bootstrap->GetBoardActor());
	TestTrue(TEXT("Controller has HUD"), Fixture.Controller->GetRuntimeHUD() == Fixture.Bootstrap->GetHUDWidget());
	TestTrue(TEXT("Interaction enabled"), Fixture.Controller->IsRuntimeInteractionBound());
	TestTrue(TEXT("Game-and-UI input configured"), Fixture.Controller->IsRuntimeInputModeConfigured());
	TestTrue(TEXT("Mouse cursor shown"), Fixture.Controller->bShowMouseCursor);
	TestTrue(TEXT("Click events enabled"), Fixture.Controller->bEnableClickEvents);
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayCameraTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Camera.FramesCornersAndPreservesTileConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayCameraTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	Fixture.Bootstrap->BoardLocation = FVector(350.0f, -175.0f, 80.0f);
	Fixture.Bootstrap->BoardRotation = FRotator(0.0f, 23.0f, 0.0f);
	const FWBRuntimeLocalPlayResult InitializeResult = Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller);
	TestTrue(TEXT("Bootstrap initializes"), InitializeResult.bOk);
	if (!InitializeResult.bOk)
	{
		Fixture.Destroy();
		return false;
	}
	AWBBoardViewActor* Board = Fixture.Bootstrap->GetBoardActor();
	ACameraActor* Camera = Fixture.Bootstrap->GetCameraActor();
	TestTrue(
		TEXT("Local controller uses camera as view target"),
		!Fixture.Controller->IsLocalController() || Fixture.Controller->GetViewTarget() == Camera);
	const FVector CameraForward = Camera->GetActorForwardVector();
	for (const FIntPoint Corner : { FIntPoint(0, 0), FIntPoint(8, 0), FIntPoint(0, 8), FIntPoint(8, 8) })
	{
		const FVector CornerWorld = Board->TileToWorld(Corner);
		const FVector Direction = (CornerWorld - Camera->GetActorLocation()).GetSafeNormal();
		TestTrue(TEXT("Corner lies inside broad camera framing cone"), FVector::DotProduct(CameraForward, Direction) > 0.85f);
		FIntPoint RoundTrip;
		TestTrue(TEXT("Camera setup preserves tile conversion"), Board->WorldToTile(CornerWorld, RoundTrip));
		TestEqual(TEXT("Custom-transform tile round trip"), RoundTrip, Corner);
	}
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayGameplaySmokeTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Gameplay.ControllerHUDMoveAndEndTurnSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayGameplaySmokeTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	if (!Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller).bOk) return false;
	UWBRuntimeMatchHostComponent* Host = Fixture.Bootstrap->GetMatchHost();
	UWBRuntimeMatchHUDWidget* HUD = Fixture.Bootstrap->GetHUDWidget();
	const FWBRuntimeLegalActionPresentation Move = FindMove(Host);
	TestTrue(TEXT("Move available"), !Move.ActionId.IsEmpty());
	if (!Move.ActionId.IsEmpty())
	{
		TestTrue(TEXT("Controller forwards stable source"), Fixture.Controller->ForwardUnitSelection(Move.SourceUnitId).bOk);
		TestTrue(TEXT("HUD filters Move"), HUD->SelectActionFamily(EWBRuntimeMatchActionFamily::Move).bOk);
		TestTrue(TEXT("Controller forwards canonical tile"), Fixture.Controller->ForwardTileSelection(Move.TargetTile).bOk);
		TestTrue(TEXT("Controller submits resolved ID"), Fixture.Controller->ForwardSubmit().bOk);
		TestTrue(TEXT("Board refresh contains moved unit"), Fixture.Bootstrap->GetBoardActor()->GetUnitPresentations().ContainsByPredicate([Move](const FWBRuntimeUnitPresentation& Unit)
		{
			return Unit.UnitId == Move.SourceUnitId && Unit.Tile == Move.TargetTile;
		}));
	}
	const int32 FirstPlayer = Host->GetCurrentPresentation().CurrentPlayerId;
	TestTrue(TEXT("End Turn routes through host"), Fixture.Controller->ForwardEndTurn().bOk);
	TestNotEqual(TEXT("Turn advances"), Host->GetCurrentPresentation().CurrentPlayerId, FirstPlayer);
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayRestartTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Restart.ReusesCompositionAndAdvancesGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayRestartTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	if (!Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller).bOk) return false;
	AWBBoardViewActor* Board = Fixture.Bootstrap->GetBoardActor();
	ACameraActor* Camera = Fixture.Bootstrap->GetCameraActor();
	UWBRuntimeMatchHUDWidget* HUD = Fixture.Bootstrap->GetHUDWidget();
	UWBRuntimeMatchHostComponent* Host = Fixture.Bootstrap->GetMatchHost();
	const int32 Generation = Host->GetCurrentPresentation().MatchGeneration;
	const FWBRuntimeLegalActionPresentation Move = FindMove(Host);
	if (!Move.ActionId.IsEmpty()) Fixture.Controller->ForwardUnitSelection(Move.SourceUnitId);
	TestTrue(TEXT("Selection exists before restart"), Host->GetCurrentSelection().SelectedUnitId >= 0);
	TestTrue(TEXT("Restart succeeds"), Fixture.Bootstrap->RestartDevelopmentMatch().bOk);
	TestTrue(TEXT("Same board"), Board == Fixture.Bootstrap->GetBoardActor());
	TestTrue(TEXT("Same camera"), Camera == Fixture.Bootstrap->GetCameraActor());
	TestTrue(TEXT("Same HUD"), HUD == Fixture.Bootstrap->GetHUDWidget());
	TestTrue(TEXT("Same host component"), Host == Fixture.Bootstrap->GetMatchHost());
	TestEqual(TEXT("Generation advances once"), Host->GetCurrentPresentation().MatchGeneration, Generation + 1);
	TestEqual(TEXT("Old source selection cleared"), Host->GetCurrentSelection().SelectedUnitId, -1);
	TestFalse(TEXT("Terminal overlay reset"), HUD->IsTerminalOverlayVisible());
	TestEqual(TEXT("One board after restart"), CountActors<AWBBoardViewActor>(Fixture.World), 1);
	TestEqual(TEXT("One camera after restart"), CountActors<ACameraActor>(Fixture.World), 1);
	TestEqual(TEXT("Two current unit Actors"), Board->GetUnitPresentationActorCount(), 2);
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayFailureRetryTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Failure.RollbackAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayFailureRetryTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	const FWBRuntimeLocalPlayResult MissingController = Fixture.Bootstrap->InitializeLocalPlay(nullptr);
	TestFalse(TEXT("Missing controller fails"), MissingController.bOk);
	TestEqual(TEXT("Missing controller diagnostic"), MissingController.Reason, FString(TEXT("local_player_controller_missing")));
	TestNull(TEXT("No board left after failure"), Fixture.Bootstrap->GetBoardActor());
	TestFalse(TEXT("Host remains uninitialized"), Fixture.Bootstrap->GetMatchHost()->IsMatchInitialized());
	Fixture.Bootstrap->BoardActorClass = nullptr;
	const FWBRuntimeLocalPlayResult MissingBoardClass = Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller);
	TestFalse(TEXT("Missing board class fails"), MissingBoardClass.bOk);
	TestEqual(TEXT("Board class diagnostic"), MissingBoardClass.Reason, FString(TEXT("board_actor_class_missing")));
	TestNull(TEXT("No camera left after rollback"), Fixture.Bootstrap->GetCameraActor());
	TestFalse(TEXT("Controller unbound after rollback"), Fixture.Controller->IsRuntimeInteractionBound());
	Fixture.Bootstrap->BoardActorClass = AWBBoardViewActor::StaticClass();
	TestTrue(TEXT("Retry succeeds"), Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller).bOk);
	TestTrue(TEXT("Retry reaches Ready"), Fixture.Bootstrap->IsLocalPlayReady());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlayShutdownTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Teardown.ExplicitAndRepeatedShutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlayShutdownTest::RunTest(const FString& Parameters)
{
	FLocalPlayFixture Fixture;
	if (!Fixture.Create()) return false;
	if (!Fixture.Bootstrap->InitializeLocalPlay(Fixture.Controller).bOk) return false;
	Fixture.Bootstrap->ShutdownLocalPlay();
	TestEqual(TEXT("Shutdown returns to Uninitialized"), Fixture.Bootstrap->GetLocalPlayState(), EWBRuntimeLocalPlayState::Uninitialized);
	TestNull(TEXT("Board reference cleared"), Fixture.Bootstrap->GetBoardActor());
	TestNull(TEXT("Camera reference cleared"), Fixture.Bootstrap->GetCameraActor());
	TestNull(TEXT("HUD reference cleared"), Fixture.Bootstrap->GetHUDWidget());
	TestFalse(TEXT("Host reset"), Fixture.Bootstrap->GetMatchHost()->IsMatchInitialized());
	TestFalse(TEXT("Controller unbound"), Fixture.Controller->IsRuntimeInteractionBound());
	Fixture.Bootstrap->ShutdownLocalPlay();
	TestEqual(TEXT("Repeated shutdown remains safe"), Fixture.Bootstrap->GetLocalPlayState(), EWBRuntimeLocalPlayState::Uninitialized);
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWBRuntimeLocalPlaySourceGuardTest,
	"Wandbound.Runtime.LocalPlayBootstrap.Guards.CompositionOnlyAndAssetFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBRuntimeLocalPlaySourceGuardTest::RunTest(const FString& Parameters)
{
	const FString PrivateRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundRuntime/Private"));
	FString GameModeSource;
	FString BootstrapSource;
	FString ControllerSource;
	FString CoreBuild;
	TestTrue(TEXT("GameMode source readable"), FFileHelper::LoadFileToString(GameModeSource, *FPaths::Combine(PrivateRoot, TEXT("WBRuntimeLocalPlayGameMode.cpp"))));
	TestTrue(TEXT("Bootstrap source readable"), FFileHelper::LoadFileToString(BootstrapSource, *FPaths::Combine(PrivateRoot, TEXT("WBRuntimeMatchBootstrapActor.cpp"))));
	TestTrue(TEXT("Controller source readable"), FFileHelper::LoadFileToString(ControllerSource, *FPaths::Combine(PrivateRoot, TEXT("WBRuntimePlayerController.cpp"))));
	TestTrue(TEXT("Core build readable"), FFileHelper::LoadFileToString(CoreBuild, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/WandboundCore/WandboundCore.Build.cs"))));
	const FString CompositionSource = GameModeSource + BootstrapSource + ControllerSource;
	for (const FString& Forbidden : {
		TEXT("WBRules"), TEXT("WBEffectRunner"), TEXT("WBEquipExecution"), TEXT("WBSummonExecution"),
		TEXT("FWBGameStateData"), TEXT("GetMutableStateForTest"), TEXT("/Game/"), TEXT("Content/"),
		TEXT(".uasset"), TEXT(".umap"), TEXT("Reference/GodotProject") })
	{
		TestFalse(*FString::Printf(TEXT("Composition excludes %s"), *Forbidden), CompositionSource.Contains(Forbidden));
	}
	TestFalse(TEXT("Core remains runtime-independent"), CoreBuild.Contains(TEXT("WandboundRuntime")));
	return true;
}

#endif
