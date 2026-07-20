#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WBRuntimeMatchBootstrapActor.generated.h"

class ACameraActor;
class AWBBoardViewActor;
class AWBRuntimePlayerController;
class UWBRuntimeMatchHostComponent;
class UWBRuntimeMatchHUDWidget;

UENUM(BlueprintType)
enum class EWBRuntimeLocalPlayState : uint8
{
	Uninitialized,
	Starting,
	Ready,
	Failed,
	ShuttingDown
};

USTRUCT(BlueprintType)
struct WANDBOUNDRUNTIME_API FWBRuntimeLocalPlayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bOk = false;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	UPROPERTY(BlueprintReadOnly)
	EWBRuntimeLocalPlayState State = EWBRuntimeLocalPlayState::Uninitialized;
};

UCLASS()
class WANDBOUNDRUNTIME_API AWBRuntimeMatchBootstrapActor : public AActor
{
	GENERATED_BODY()

public:
	AWBRuntimeMatchBootstrapActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wandbound|Local Play")
	TObjectPtr<UWBRuntimeMatchHostComponent> MatchHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	bool bInitializeDevelopmentMatch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	bool bEnableDevelopmentViewerSwitch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	int32 InitialViewerPlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	FVector BoardLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	FRotator BoardRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	float BoardTileSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	FVector CameraOffset = FVector(0.0f, -1400.0f, 1400.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	float CameraFieldOfView = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	TSubclassOf<AWBBoardViewActor> BoardActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	TSubclassOf<ACameraActor> CameraActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wandbound|Local Play")
	TSubclassOf<UWBRuntimeMatchHUDWidget> HUDWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Local Play", meta = (DevelopmentOnly))
	FWBRuntimeLocalPlayResult InitializeLocalPlay(AWBRuntimePlayerController* LocalController);

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Local Play", meta = (DevelopmentOnly))
	FWBRuntimeLocalPlayResult RestartDevelopmentMatch();

	UFUNCTION(BlueprintCallable, Category = "Wandbound|Local Play")
	void ShutdownLocalPlay();

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	bool IsLocalPlayReady() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	EWBRuntimeLocalPlayState GetLocalPlayState() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	FString GetStartupFailureReason() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	UWBRuntimeMatchHostComponent* GetMatchHost() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	AWBBoardViewActor* GetBoardActor() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	ACameraActor* GetCameraActor() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	AWBRuntimePlayerController* GetRuntimeController() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	UWBRuntimeMatchHUDWidget* GetHUDWidget() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	bool OwnsBoardActor() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	bool OwnsCameraActor() const;

	UFUNCTION(BlueprintPure, Category = "Wandbound|Local Play")
	bool OwnsHUDWidget() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AWBBoardViewActor> BoardActor;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> CameraActor;

	UPROPERTY(Transient)
	TObjectPtr<AWBRuntimePlayerController> RuntimeController;

	UPROPERTY(Transient)
	TObjectPtr<UWBRuntimeMatchHUDWidget> HUDWidget;

	EWBRuntimeLocalPlayState LocalPlayState = EWBRuntimeLocalPlayState::Uninitialized;
	FString StartupFailureReason;
	bool bOwnsBoardActor = false;
	bool bOwnsCameraActor = false;
	bool bOwnsHUDWidget = false;

	FWBRuntimeLocalPlayResult MakeResult(bool bOk, const FString& Reason) const;
	FWBRuntimeLocalPlayResult FailStartup(const FString& Reason);
	void RollbackStartup();
	bool EnsureBoard();
	bool EnsureCamera();
	bool EnsureHUD();
	void ConfigureController();
};
