#include "WBRuntimeLocalPlayGameMode.h"

#include "EngineUtils.h"
#include "WBRuntimePlayerController.h"

AWBRuntimeLocalPlayGameMode::AWBRuntimeLocalPlayGameMode()
{
	PlayerControllerClass = AWBRuntimePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	SpectatorClass = nullptr;
	bStartPlayersAsSpectators = true;
	BootstrapActorClass = AWBRuntimeMatchBootstrapActor::StaticClass();
}

void AWBRuntimeLocalPlayGameMode::StartPlay()
{
	Super::StartPlay();
	AWBRuntimeMatchBootstrapActor* Bootstrap = EnsureSingleBootstrap();
	if (Bootstrap == nullptr) return;
	if (AWBRuntimePlayerController* Controller = Cast<AWBRuntimePlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		StartLocalPlayForController(Controller);
	}
}

void AWBRuntimeLocalPlayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (AWBRuntimePlayerController* RuntimeController = Cast<AWBRuntimePlayerController>(NewPlayer))
	{
		StartLocalPlayForController(RuntimeController);
	}
}

FWBRuntimeLocalPlayResult AWBRuntimeLocalPlayGameMode::StartLocalPlayForController(
	AWBRuntimePlayerController* LocalController)
{
	AWBRuntimeMatchBootstrapActor* Bootstrap = EnsureSingleBootstrap();
	if (Bootstrap == nullptr)
	{
		FWBRuntimeLocalPlayResult Result;
		Result.Reason = StartupFailureReason;
		Result.State = EWBRuntimeLocalPlayState::Failed;
		return Result;
	}
	const FWBRuntimeLocalPlayResult Result = Bootstrap->InitializeLocalPlay(LocalController);
	if (!Result.bOk) StartupFailureReason = Result.Reason;
	else StartupFailureReason.Reset();
	return Result;
}

FWBRuntimeLocalPlayResult AWBRuntimeLocalPlayGameMode::RestartDevelopmentMatch()
{
	if (BootstrapActor == nullptr)
	{
		FWBRuntimeLocalPlayResult Result;
		Result.Reason = TEXT("bootstrap_actor_missing");
		return Result;
	}
	return BootstrapActor->RestartDevelopmentMatch();
}

AWBRuntimeMatchBootstrapActor* AWBRuntimeLocalPlayGameMode::GetBootstrapActor() const { return BootstrapActor; }
FString AWBRuntimeLocalPlayGameMode::GetStartupFailureReason() const { return StartupFailureReason; }

void AWBRuntimeLocalPlayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BootstrapActor != nullptr)
	{
		BootstrapActor->ShutdownLocalPlay();
		if (bOwnsBootstrapActor && IsValid(BootstrapActor)) BootstrapActor->Destroy();
	}
	BootstrapActor = nullptr;
	bOwnsBootstrapActor = false;
	Super::EndPlay(EndPlayReason);
}

AWBRuntimeMatchBootstrapActor* AWBRuntimeLocalPlayGameMode::EnsureSingleBootstrap()
{
	if (BootstrapActor != nullptr && IsValid(BootstrapActor)) return BootstrapActor;
	if (GetWorld() == nullptr)
	{
		StartupFailureReason = TEXT("local_play_world_missing");
		return nullptr;
	}
	TArray<AWBRuntimeMatchBootstrapActor*> Existing;
	for (TActorIterator<AWBRuntimeMatchBootstrapActor> It(GetWorld()); It; ++It) Existing.Add(*It);
	if (Existing.Num() > 1)
	{
		StartupFailureReason = TEXT("duplicate_bootstrap_actors");
		return nullptr;
	}
	if (Existing.Num() == 1)
	{
		BootstrapActor = Existing[0];
		bOwnsBootstrapActor = false;
		return BootstrapActor;
	}
	if (BootstrapActorClass == nullptr)
	{
		StartupFailureReason = TEXT("bootstrap_actor_class_missing");
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BootstrapActor = GetWorld()->SpawnActor<AWBRuntimeMatchBootstrapActor>(BootstrapActorClass, FTransform::Identity, Params);
	bOwnsBootstrapActor = BootstrapActor != nullptr;
	if (BootstrapActor == nullptr) StartupFailureReason = TEXT("bootstrap_spawn_failed");
	return BootstrapActor;
}
