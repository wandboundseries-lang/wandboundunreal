#include "WBRuntimeMatchBootstrapActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeMatchHostComponent.h"
#include "WBRuntimeMatchHUDWidget.h"
#include "WBRuntimePlayerController.h"
#include "WBRuntimePresentationAssetBinding.h"

DEFINE_LOG_CATEGORY_STATIC(LogWBRuntimeLocalPlay, Log, All);

AWBRuntimeMatchBootstrapActor::AWBRuntimeMatchBootstrapActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MatchHost = CreateDefaultSubobject<UWBRuntimeMatchHostComponent>(TEXT("RuntimeMatchHost"));
	BoardActorClass = AWBBoardViewActor::StaticClass();
	CameraActorClass = ACameraActor::StaticClass();
	HUDWidgetClass = UWBRuntimeMatchHUDWidget::StaticClass();
	DevelopmentPresentationAssetSet = TSoftObjectPtr<UWBRuntimePresentationAssetSet>(
		FSoftObjectPath(
			TEXT("/Game/Wandbound/Presentation/DA_WandboundStarterPresentation.DA_WandboundStarterPresentation")));
}

FWBRuntimeLocalPlayResult AWBRuntimeMatchBootstrapActor::InitializeLocalPlay(
	AWBRuntimePlayerController* LocalController)
{
	if (LocalPlayState == EWBRuntimeLocalPlayState::Ready) return MakeResult(true, TEXT("local_play_already_ready"));
	if (LocalPlayState == EWBRuntimeLocalPlayState::Starting) return MakeResult(false, TEXT("local_play_start_in_progress"));
	if (LocalPlayState == EWBRuntimeLocalPlayState::ShuttingDown) return MakeResult(false, TEXT("local_play_shutting_down"));
	if (GetWorld() == nullptr) return FailStartup(TEXT("local_play_world_missing"));
	if (LocalController == nullptr) return FailStartup(TEXT("local_player_controller_missing"));
	if (!LocalController->IsLocalController() && GetWorld()->GetGameViewport() != nullptr)
	{
		return FailStartup(TEXT("local_player_controller_missing"));
	}
	if (!bInitializeDevelopmentMatch) return FailStartup(TEXT("development_match_initialization_disabled"));
	if (MatchHost == nullptr) return FailStartup(TEXT("match_host_missing"));

	LocalPlayState = EWBRuntimeLocalPlayState::Starting;
	StartupFailureReason.Reset();
	RuntimeController = LocalController;
	HUDWidget = RuntimeController->GetRuntimeHUD();
	RuntimeController->SetRuntimeInteractionEnabled(false);
	if (!EnsureBoard()) return FailStartup(BoardActorClass == nullptr ? TEXT("board_actor_class_missing") : TEXT("board_spawn_failed"));
	if (!EnsureCamera()) return FailStartup(CameraActorClass == nullptr ? TEXT("camera_actor_class_missing") : TEXT("camera_spawn_failed"));

	ResolveDevelopmentPresentationAssetSet();
	MatchHost->BoardActor = BoardActor;
	MatchHost->ConfigurePresentationAssets(
		ResolvedPresentationAssetSet,
		CameraActor,
		IsPresentationAssetLoadingEnabledByPolicy());
	const FWBRuntimeMatchCommandResult MatchResult = MatchHost->InitializeDevelopmentMatch(InitialViewerPlayerId, false);
	if (!MatchResult.bOk) return FailStartup(MatchResult.Reason.IsEmpty() ? TEXT("development_match_initialization_failed") : MatchResult.Reason);
	if (!EnsureHUD()) return FailStartup(HUDWidgetClass == nullptr ? TEXT("hud_widget_class_missing") : TEXT("hud_creation_failed"));
	if (!HUDWidget->BindToMatchHost(MatchHost)) return FailStartup(TEXT("hud_host_binding_failed"));

	ConfigureController();
	const FWBRuntimeMatchCommandResult RefreshResult = MatchHost->RefreshPresentation();
	if (!RefreshResult.bOk) return FailStartup(RefreshResult.Reason);
	if (BoardActor->GetTilePresentations().Num() != 81) return FailStartup(TEXT("board_presentation_incomplete"));

	LocalPlayState = EWBRuntimeLocalPlayState::Ready;
	RuntimeController->SetRuntimeInteractionEnabled(true);
	return MakeResult(true, TEXT("local_play_ready"));
}

FWBRuntimeLocalPlayResult AWBRuntimeMatchBootstrapActor::RestartDevelopmentMatch()
{
	if (LocalPlayState != EWBRuntimeLocalPlayState::Ready || RuntimeController == nullptr)
	{
		return MakeResult(false, TEXT("local_play_not_ready"));
	}
	LocalPlayState = EWBRuntimeLocalPlayState::Starting;
	RuntimeController->SetRuntimeInteractionEnabled(false);
	HUDWidget->UnbindFromMatchHost();
	MatchHost->ClearSelection();
	BoardActor->ClearBoardView();
	ResolveDevelopmentPresentationAssetSet();
	MatchHost->ConfigurePresentationAssets(
		ResolvedPresentationAssetSet,
		CameraActor,
		IsPresentationAssetLoadingEnabledByPolicy());
	const FWBRuntimeMatchCommandResult MatchResult = MatchHost->InitializeDevelopmentMatch(InitialViewerPlayerId, false);
	if (!MatchResult.bOk) return FailStartup(MatchResult.Reason);
	if (!HUDWidget->BindToMatchHost(MatchHost)) return FailStartup(TEXT("hud_host_binding_failed"));
	ConfigureController();
	const FWBRuntimeMatchCommandResult RefreshResult = MatchHost->RefreshPresentation();
	if (!RefreshResult.bOk) return FailStartup(RefreshResult.Reason);
	LocalPlayState = EWBRuntimeLocalPlayState::Ready;
	RuntimeController->SetRuntimeInteractionEnabled(true);
	return MakeResult(true, TEXT("development_match_restarted"));
}

void AWBRuntimeMatchBootstrapActor::ShutdownLocalPlay()
{
	if (LocalPlayState == EWBRuntimeLocalPlayState::ShuttingDown) return;
	if (LocalPlayState == EWBRuntimeLocalPlayState::Uninitialized && BoardActor == nullptr && CameraActor == nullptr && HUDWidget == nullptr) return;
	LocalPlayState = EWBRuntimeLocalPlayState::ShuttingDown;
	if (RuntimeController != nullptr) RuntimeController->ClearRuntimeReferences(bOwnsHUDWidget);
	if (HUDWidget != nullptr)
	{
		HUDWidget->UnbindFromMatchHost();
		if (bOwnsHUDWidget) HUDWidget->RemoveFromParent();
	}
	if (MatchHost != nullptr)
	{
		MatchHost->ResetMatch();
		MatchHost->BoardActor = nullptr;
	}
	if (BoardActor != nullptr && bOwnsBoardActor && IsValid(BoardActor)) BoardActor->Destroy();
	if (CameraActor != nullptr && bOwnsCameraActor && IsValid(CameraActor)) CameraActor->Destroy();
	BoardActor = nullptr;
	CameraActor = nullptr;
	RuntimeController = nullptr;
	HUDWidget = nullptr;
	ResolvedPresentationAssetSet = nullptr;
	bOwnsBoardActor = false;
	bOwnsCameraActor = false;
	bOwnsHUDWidget = false;
	LocalPlayState = EWBRuntimeLocalPlayState::Uninitialized;
}

void AWBRuntimeMatchBootstrapActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownLocalPlay();
	Super::EndPlay(EndPlayReason);
}

bool AWBRuntimeMatchBootstrapActor::IsLocalPlayReady() const { return LocalPlayState == EWBRuntimeLocalPlayState::Ready; }
EWBRuntimeLocalPlayState AWBRuntimeMatchBootstrapActor::GetLocalPlayState() const { return LocalPlayState; }
FString AWBRuntimeMatchBootstrapActor::GetStartupFailureReason() const { return StartupFailureReason; }
UWBRuntimeMatchHostComponent* AWBRuntimeMatchBootstrapActor::GetMatchHost() const { return MatchHost; }
AWBBoardViewActor* AWBRuntimeMatchBootstrapActor::GetBoardActor() const { return BoardActor; }
ACameraActor* AWBRuntimeMatchBootstrapActor::GetCameraActor() const { return CameraActor; }
AWBRuntimePlayerController* AWBRuntimeMatchBootstrapActor::GetRuntimeController() const { return RuntimeController; }
UWBRuntimeMatchHUDWidget* AWBRuntimeMatchBootstrapActor::GetHUDWidget() const { return HUDWidget; }
bool AWBRuntimeMatchBootstrapActor::OwnsBoardActor() const { return bOwnsBoardActor; }
bool AWBRuntimeMatchBootstrapActor::OwnsCameraActor() const { return bOwnsCameraActor; }
bool AWBRuntimeMatchBootstrapActor::OwnsHUDWidget() const { return bOwnsHUDWidget; }
bool AWBRuntimeMatchBootstrapActor::IsPresentationAssetSetConfigured() const
{
	return PresentationAssetSet != nullptr
		|| !DevelopmentPresentationAssetSet.IsNull();
}

bool AWBRuntimeMatchBootstrapActor::IsPresentationAssetLoadingEnabledByPolicy() const
{
	return bEnablePresentationAssetLoading
		&& !FParse::Param(
			FCommandLine::Get(),
			TEXT("WandboundLocalPlaySmoke"));
}

bool AWBRuntimeMatchBootstrapActor::IsPresentationFallbackActive() const
{
	return ResolvedPresentationAssetSet == nullptr
		|| !IsPresentationAssetLoadingEnabledByPolicy();
}

EWBRuntimePresentationAssetSetStatus
AWBRuntimeMatchBootstrapActor::GetPresentationAssetSetStatus() const
{
	return PresentationAssetSetStatus;
}

FWBRuntimeLocalPlayResult AWBRuntimeMatchBootstrapActor::MakeResult(const bool bOk, const FString& Reason) const
{
	FWBRuntimeLocalPlayResult Result;
	Result.bOk = bOk;
	Result.Reason = Reason;
	Result.State = LocalPlayState;
	return Result;
}

FWBRuntimeLocalPlayResult AWBRuntimeMatchBootstrapActor::FailStartup(const FString& Reason)
{
	StartupFailureReason = Reason;
	UE_LOG(LogWBRuntimeLocalPlay, Log, TEXT("Wandbound local-play startup failed: %s"), *Reason);
	RollbackStartup();
	LocalPlayState = EWBRuntimeLocalPlayState::Failed;
	return MakeResult(false, Reason);
}

void AWBRuntimeMatchBootstrapActor::RollbackStartup()
{
	if (RuntimeController != nullptr) RuntimeController->ClearRuntimeReferences(bOwnsHUDWidget);
	if (HUDWidget != nullptr)
	{
		HUDWidget->UnbindFromMatchHost();
		if (bOwnsHUDWidget) HUDWidget->RemoveFromParent();
	}
	if (MatchHost != nullptr)
	{
		MatchHost->ResetMatch();
		MatchHost->BoardActor = nullptr;
	}
	if (BoardActor != nullptr && bOwnsBoardActor && IsValid(BoardActor)) BoardActor->Destroy();
	if (CameraActor != nullptr && bOwnsCameraActor && IsValid(CameraActor)) CameraActor->Destroy();
	BoardActor = nullptr;
	CameraActor = nullptr;
	RuntimeController = nullptr;
	HUDWidget = nullptr;
	ResolvedPresentationAssetSet = nullptr;
	bOwnsBoardActor = false;
	bOwnsCameraActor = false;
	bOwnsHUDWidget = false;
}

bool AWBRuntimeMatchBootstrapActor::EnsureBoard()
{
	if (BoardActor != nullptr) return true;
	if (BoardActorClass == nullptr) return false;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BoardActor = GetWorld()->SpawnActor<AWBBoardViewActor>(BoardActorClass, BoardLocation, BoardRotation, Params);
	bOwnsBoardActor = BoardActor != nullptr;
	if (BoardActor != nullptr)
	{
		BoardActor->ViewSettings.BoardWidth = 9;
		BoardActor->ViewSettings.BoardHeight = 9;
		BoardActor->ViewSettings.TileSize = BoardTileSize;
	}
	return BoardActor != nullptr;
}

bool AWBRuntimeMatchBootstrapActor::EnsureCamera()
{
	if (CameraActor != nullptr) return true;
	if (CameraActorClass == nullptr) return false;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector CameraLocation = BoardLocation + CameraOffset;
	const FRotator CameraRotation = (BoardLocation - CameraLocation).Rotation();
	CameraActor = GetWorld()->SpawnActor<ACameraActor>(CameraActorClass, CameraLocation, CameraRotation, Params);
	bOwnsCameraActor = CameraActor != nullptr;
	if (CameraActor != nullptr && CameraActor->GetCameraComponent() != nullptr)
	{
		CameraActor->GetCameraComponent()->SetFieldOfView(CameraFieldOfView);
	}
	return CameraActor != nullptr;
}

bool AWBRuntimeMatchBootstrapActor::EnsureHUD()
{
	if (HUDWidget != nullptr) return true;
	HUDWidget = RuntimeController != nullptr ? RuntimeController->GetRuntimeHUD() : nullptr;
	if (HUDWidget != nullptr) return true;
	if (HUDWidgetClass == nullptr) return false;
	if (GetWorld()->GetGameViewport() != nullptr)
	{
		HUDWidget = CreateWidget<UWBRuntimeMatchHUDWidget>(RuntimeController, HUDWidgetClass);
	}
	else
	{
		HUDWidget = NewObject<UWBRuntimeMatchHUDWidget>(this, HUDWidgetClass);
		HUDWidget->EnsureWidgetTreeBuilt();
	}
	bOwnsHUDWidget = HUDWidget != nullptr;
	if (HUDWidget != nullptr && GetWorld()->GetGameViewport() != nullptr && !HUDWidget->IsInViewport())
	{
		HUDWidget->AddToViewport();
	}
	return HUDWidget != nullptr;
}

void AWBRuntimeMatchBootstrapActor::ConfigureController()
{
	RuntimeController->bEnableDevelopmentViewerSwitch = bEnableDevelopmentViewerSwitch;
	HUDWidget->bAllowDevelopmentViewerSwitch = bEnableDevelopmentViewerSwitch;
	RuntimeController->SetRuntimeReferences(MatchHost, BoardActor, HUDWidget);
	RuntimeController->SetViewTarget(CameraActor);
}

void AWBRuntimeMatchBootstrapActor::ResolveDevelopmentPresentationAssetSet()
{
	ResolvedPresentationAssetSet = PresentationAssetSet;
	if (ResolvedPresentationAssetSet != nullptr)
	{
		PresentationAssetSetStatus =
			IsPresentationAssetLoadingEnabledByPolicy()
				? EWBRuntimePresentationAssetSetStatus::Loaded
				: EWBRuntimePresentationAssetSetStatus::LoadingDisabled;
		return;
	}
	if (DevelopmentPresentationAssetSet.IsNull())
	{
		PresentationAssetSetStatus =
			EWBRuntimePresentationAssetSetStatus::NotConfigured;
		return;
	}
	if (!IsPresentationAssetLoadingEnabledByPolicy())
	{
		PresentationAssetSetStatus =
			EWBRuntimePresentationAssetSetStatus::LoadingDisabled;
		return;
	}

	ResolvedPresentationAssetSet =
		DevelopmentPresentationAssetSet.LoadSynchronous();
	PresentationAssetSetStatus =
		ResolvedPresentationAssetSet != nullptr
			? EWBRuntimePresentationAssetSetStatus::Loaded
			: EWBRuntimePresentationAssetSetStatus::Missing;
	if (ResolvedPresentationAssetSet != nullptr)
	{
		UE_LOG(
			LogWBRuntimeLocalPlay,
			Log,
			TEXT("Wandbound presentation asset status: starter_asset_set_loaded"));
	}
	else
	{
		UE_LOG(
			LogWBRuntimeLocalPlay,
			Warning,
			TEXT("Wandbound presentation asset status: starter_asset_set_missing"));
	}
}
